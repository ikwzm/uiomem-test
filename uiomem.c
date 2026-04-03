/*********************************************************************************
 *
 *       Copyright (C) 2026 Ichiro Kawazome
 *       All rights reserved.
 * 
 *       Redistribution and use in source and binary forms, with or without
 *       modification, are permitted provided that the following conditions
 *       are met:
 * 
 *         1. Redistributions of source code must retain the above copyright
 *            notice, this list of conditions and the following disclaimer.
 * 
 *         2. Redistributions in binary form must reproduce the above copyright
 *            notice, this list of conditions and the following disclaimer in
 *            the documentation and/or other materials provided with the
 *            distribution.
 * 
 *       THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *       "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *       LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *       A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
 *       OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *       SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *       LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *       DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *       THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *       (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *       OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 ********************************************************************************/

#include        <stdio.h>
#include        <stdlib.h>
#include        <unistd.h>
#include        <fcntl.h>
#include        <string.h>
#include        <errno.h>
#include        <sys/ioctl.h>
#include        <sys/types.h>
#include        "uiomem.h"

#ifdef USE_UIOMEM_IOCTL
static int xioctl(int fd, int ioctl_code, void* parameter)
{
    int result;
    do {
        result = ioctl(fd, ioctl_code, parameter);
    } while ((result == -1) && (errno == EINTR));
    return result;
}
#endif

void uiomem_destroy(struct uiomem* this)
{
    if (this == NULL)
        return;
    
    if (this->sync_for_cpu_file >= 0) close(this->sync_for_cpu_file);
    if (this->sync_for_dev_file >= 0) close(this->sync_for_dev_file);
    if (this->name     != NULL) free(this->name);
    if (this->dev_name != NULL) free(this->dev_name);
    if (this->sys_path != NULL) free(this->sys_path);
    if (this->version  != NULL) free(this->version);
    if (this->sync_op  != NULL) free(this->sync_op);
    free(this);
}

static int  get_int_from_sys_dev_file(struct uiomem* this, char* name, int* value_ptr)
{
    char    file_name[1024];
    char    attr[1024];
    int     str_len;
    int     fd;
    int     status;
    str_len = sprintf(file_name, "%s/%s", this->sys_path, name);
    fd = open(file_name, O_RDONLY);
    if (fd == -1) {
        printf("Can not open %s errno=%d (%s)\n", file_name, errno, strerror(errno));
        status = -1;
        goto done;
    }
    status = read(fd, attr, sizeof(attr));
    if (status < 0) {
        printf("Can not read %s errno=%d (%s)\n", file_name, errno, strerror(errno));
        status = -1;
        goto done;
    }
    sscanf(attr, "%d", value_ptr);
    status = 0;
  done:
    if (fd >= 0)
        close(fd);
    return status;
}

static int  get_size_from_sys_dev_file(struct uiomem* this, char* name, size_t* value_ptr)
{
    char    file_name[1024];
    char    attr[1024];
    int     str_len;
    int     fd;
    int     status;
    str_len = sprintf(file_name, "%s/%s", this->sys_path, name);
    fd = open(file_name, O_RDONLY);
    if (fd == -1) {
        printf("Can not open %s errno=%d (%s)\n", file_name, errno, strerror(errno));
        status = -1;
        goto done;
    }
    status = read(fd, attr, sizeof(attr));
    if (status < 0) {
        printf("Can not read %s errno=%d (%s)\n", file_name, errno, strerror(errno));
        status = -1;
        goto done;
    }
    sscanf(attr, "%zu", value_ptr);
    status = 0;
  done:
    if (fd >= 0)
        close(fd);
    return status;
}

static int  get_str_from_sys_dev_file(struct uiomem* this, char* name, char** str_ptr)
{
    char    file_name[1024];
    char    attr[1024];
    int     str_len;
    int     len;
    int     fd;
    int     status;
    str_len = sprintf(file_name, "%s/%s", this->sys_path, name);
    fd = open(file_name, O_RDONLY);
    if (fd == -1) {
        printf("Can not open %s errno=%d (%s)\n", file_name, errno, strerror(errno));
        status = -1;
        goto done;
    }
    len = read(fd, attr, sizeof(attr));
    if (len < 0) {
        printf("Can not read %s errno=%d (%s)\n", file_name, errno, strerror(errno));
        status = -1;
        goto done;
    }
    while(--len >= 0) {
        if (attr[len] =='\n') {
            attr[len] = '\0';
            break;
        }
    }
    *str_ptr = strdup(attr);
    status = 0;
  done:
    if (fd >= 0)
        close(fd);
    return status;
}

static int  get_fd_from_sys_dev_file(struct uiomem* this, char* name)
{
    char    file_name[1024];
    int     str_len;
    int     fd;
    str_len = sprintf(file_name, "%s/%s", this->sys_path, name);
    fd = open(file_name, O_RDWR);
    if (fd < 0) {
        printf("Can not open %s errno=%d (%s)\n", file_name, errno, strerror(errno));
    }
    return fd;
}

struct uiomem* uiomem_create(char* name)
{
    struct uiomem*  this;
    char            file_name[1024];
    char            attr[1024];
    int             str_len;
    int             fd;
    int             status;

    if ((this = calloc(1, sizeof(struct uiomem))) == NULL) {
        printf("Can not alloc uiomem\n");
        goto failed;
    }
    this->sync_for_cpu_file = -1;
    this->sync_for_dev_file = -1;
    
    if ((this->name = strdup(name)) == NULL) {
        printf("Can not alloc uiomem->name\n");
        goto failed;
    }
    str_len = sprintf(file_name, "/dev/%s", this->name);
    if ((this->dev_name = strdup(file_name)) == NULL) {
        printf("Can not alloc uiomem->dev_name\n");
        goto failed;
    }
    str_len = sprintf(file_name, "/sys/class/uiomem/%s", this->name);
    if ((this->sys_path = strdup(file_name)) == NULL) {
        printf("Can not alloc uiomem->sys_path\n");
        goto failed;
    }
    if (get_size_from_sys_dev_file(this, "size"           , &this->size     ) != 0)
        goto failed;
    if (get_int_from_sys_dev_file( this, "shareable"      , &this->shareable) != 0)
        goto failed;
    if (get_int_from_sys_dev_file( this, "cached"         , &this->cached   ) != 0)
        goto failed;
    if (get_int_from_sys_dev_file( this, "coherent"       , &this->coherent ) != 0)
        goto failed;
    if (get_str_from_sys_dev_file( this, "driver_version" , &this->version  ) != 0)
        goto failed;
    if (get_str_from_sys_dev_file( this, "sync_operation" , &this->sync_op  ) != 0)
        goto failed;

#ifdef USE_UIOMEM_IOCTL
    this->ioctl_version = 0;
    str_len = sprintf(file_name, "%s/ioctl_version", this->sys_path);
    if ((fd = open(file_name, O_RDONLY)) != -1) {
        int len = read(fd, attr, sizeof(attr));
        if (len >= 0) 
            sscanf(attr, "%d", &this->ioctl_version);
        else
            printf("Can not read %s errno=%d (%s)\n", file_name, errno, strerror(errno));
        close(fd);
    }      
    if (this->ioctl_version > 0) {
        this->sync_for_cpu_args.offset = 0;
        this->sync_for_cpu_args.size   = this->size;
        SET_UIOMEM_IOCTL_FLAGS_SYNC_DIR(&this->sync_for_cpu_args, UIOMEM_READ_WRITE);
        SET_UIOMEM_IOCTL_FLAGS_SYNC_CMD(&this->sync_for_cpu_args, UIOMEM_IOCTL_FLAGS_SYNC_CMD_FOR_CPU);
        this->sync_for_dev_args.offset = 0;
        this->sync_for_dev_args.size   = this->size;
        SET_UIOMEM_IOCTL_FLAGS_SYNC_DIR(&this->sync_for_dev_args, UIOMEM_READ_WRITE);
        SET_UIOMEM_IOCTL_FLAGS_SYNC_CMD(&this->sync_for_dev_args, UIOMEM_IOCTL_FLAGS_SYNC_CMD_FOR_DEVICE);
    }
#else
    this->ioctl_version = 0;
#endif
    if (this->ioctl_version == 0) {
        if ((this->sync_for_cpu_file = get_fd_from_sys_dev_file(this, "sync_for_cpu")) < 0)
            goto failed;
        if ((this->sync_for_dev_file = get_fd_from_sys_dev_file(this, "sync_for_device")) < 0)
            goto failed;
    }
    return this;
      
  failed:
    uiomem_destroy(this);
    return NULL;
}

int  uiomem_open(struct uiomem* this, int flags)
{
    return open(this->dev_name, flags);
}

void uiomem_set_sync_area(struct uiomem* this, unsigned int offset, unsigned int size, int direction)
{
#ifdef USE_UIOMEM_IOCTL
    if (this->ioctl_version > 0) {
        this->sync_for_cpu_args.offset = offset;
        this->sync_for_cpu_args.size   = size;
        SET_UIOMEM_IOCTL_FLAGS_SYNC_DIR(&this->sync_for_cpu_args, direction);
        SET_UIOMEM_IOCTL_FLAGS_SYNC_CMD(&this->sync_for_cpu_args, UIOMEM_IOCTL_FLAGS_SYNC_CMD_FOR_CPU);
        this->sync_for_dev_args.offset = offset;
        this->sync_for_dev_args.size   = size;
        SET_UIOMEM_IOCTL_FLAGS_SYNC_DIR(&this->sync_for_dev_args, direction);
        SET_UIOMEM_IOCTL_FLAGS_SYNC_CMD(&this->sync_for_dev_args, UIOMEM_IOCTL_FLAGS_SYNC_CMD_FOR_DEVICE);
    }
#endif
    this->sync_command_len = 
        sprintf(this->sync_command_str, "0x%08X%08X\n",
                offset,
               ((size & 0xFFFFFFF0) | (direction << 2) | 1));
    this->sync_command = ((uint64_t)offset << 32) |
               ((size & 0xFFFFFFF0) | (direction << 2) | 1);
}

size_t uiomem_sync_for_cpu(struct uiomem* this, int fd)
{
    int status = 0;
#ifdef USE_UIOMEM_IOCTL
    if (this->ioctl_version > 0)
        return xioctl(fd, UIOMEM_IOCTL_SET_SYNC, &this->sync_for_cpu_args);
#endif
    if (this->sync_command_len > 0)
        status = write(this->sync_for_cpu_file,
                       this->sync_command_str,
                       this->sync_command_len);
    return status;
}

size_t uiomem_sync_for_dev(struct uiomem* this, int fd)
{
    int status = 0;
#ifdef USE_UIOMEM_IOCTL
    if (this->ioctl_version > 0)
        return xioctl(fd, UIOMEM_IOCTL_SET_SYNC, &this->sync_for_dev_args);
#endif
    if (this->sync_command_len > 0)
        status = write(this->sync_for_dev_file,
                       this->sync_command_str,
                       this->sync_command_len);
    return status;
}

