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
#include        <fcntl.h>
#include        <string.h>
#include        <time.h>
#include        <stdlib.h>
#include        <unistd.h>
#include        <getopt.h>
#include        <errno.h>
#include        <sys/ioctl.h>
#include        <sys/time.h>
#include        <sys/types.h>
#include        <sys/mman.h>
#include        <sys/utsname.h>
#include        <inttypes.h>
#include        "uiomem-ioctl.h"

struct proc_time_info {
	struct timespec start;
	struct timespec done;
	long long       elapsed;
};
static  long long proc_time_calc_elapsed(struct proc_time_info* t)
{
	long time_sec  = t->done.tv_sec  - t->start.tv_sec; 
	long time_nsec = t->done.tv_nsec - t->start.tv_nsec;
	t->elapsed = 1000*1000*1000*time_sec + time_nsec;
}

int xioctl(int fd, int ioctl_code, void* parameter)
{
    int result;
    do {
        result = ioctl(fd, ioctl_code, parameter);
    } while ((result == -1) && (errno == EINTR));
    return result;
}

int  check_driver_version(const char* device_name, const char* driver_version)
{
    int   fd;
    char  attr[1024];
    char  file_name[1024];

    sprintf(file_name, "/sys/class/uiomem/%s/driver_version", device_name);
    
    if ((fd  = open(file_name, O_RDONLY)) < 0) {
        perror(file_name);
        exit(-1);
    } else {
        int len;
        len = read(fd, attr, sizeof(attr));
        while(--len >= 0) {
            if (attr[len] =='\n') {
                attr[len] = '\0';
                break;
            }
        }
        close(fd);
    }
    if (strcmp(attr, driver_version) != 0) {
        printf("read %s => %s\n", file_name, attr);
        printf("mismatch driver_version(=%s)\n", driver_version);
        exit(-1);
    }
    return 0;
}

int  check_device_info(const char* device_name, uiomem_ioctl_dev_info* dev_info)
{
    int      fd;
    char     attr[1024];
    char     file_name[1024];
    size_t   size;
    uint64_t phys_addr;
    int      shareable;
    int      cached;
    int      coherent;
    int      status;
    
    sprintf(file_name, "/sys/class/uiomem/%s/size", device_name);
    if ((fd  = open(file_name, O_RDONLY)) < 0) {
        perror(file_name);
        exit(-1);
    }
    status = read(fd, attr, sizeof(attr));
    if (status < 0) {
        printf("Can not read %s errno=%d (%s)\n", file_name, errno, strerror(errno));
        exit(-1);
    }
    sscanf(attr, "%" SCNu64, &size);
    close(fd);

    if (size != dev_info->size) {
        printf("read %s => %s\n", file_name, attr);
        printf("mismatch size(=%" PRIu64 ")\n", dev_info->size);
        exit(-1);
    }
    
    sprintf(file_name, "/sys/class/uiomem/%s/phys_addr", device_name);
    if ((fd  = open(file_name, O_RDONLY)) < 0) {
        perror(file_name);
        exit(-1);
    }
    status = read(fd, attr, sizeof(attr));
    if (status < 0) {
        printf("Can not read %s errno=%d (%s)\n", file_name, errno, strerror(errno));
        exit(-1);
    }
    sscanf(attr, "%" SCNx64, &phys_addr);
    close(fd);

    if (phys_addr != dev_info->addr) {
        printf("read %s => %s\n", file_name, attr);
        printf("mismatch phys_addr (=0x%" PRIx64 ")\n", dev_info->addr);
        exit(-1);
    }

    sprintf(file_name, "/sys/class/uiomem/%s/shareable", device_name);
    if ((fd  = open(file_name, O_RDONLY)) < 0) {
        perror(file_name);
        exit(-1);
    }
    status = read(fd, attr, sizeof(attr));
    if (status < 0) {
        printf("Can not read %s errno=%d (%s)\n", file_name, errno, strerror(errno));
        exit(-1);
    }
    sscanf(attr, "%d", &shareable);
    close(fd);

    if (shareable != GET_UIOMEM_IOCTL_FLAGS_SHAREABLE(dev_info)) {
        printf("read %s => %s\n", file_name, attr);
        printf("mismatch shareable(=%d)\n", GET_UIOMEM_IOCTL_FLAGS_SHAREABLE(dev_info));
        exit(-1);
    }

    sprintf(file_name, "/sys/class/uiomem/%s/cached", device_name);
    if ((fd  = open(file_name, O_RDONLY)) < 0) {
        perror(file_name);
        exit(-1);
    }
    status = read(fd, attr, sizeof(attr));
    if (status < 0) {
        printf("Can not read %s errno=%d (%s)\n", file_name, errno, strerror(errno));
        exit(-1);
    }
    sscanf(attr, "%d", &cached);
    close(fd);

    if (cached != GET_UIOMEM_IOCTL_FLAGS_CACHED(dev_info)) {
        printf("read %s => %s\n", file_name, attr);
        printf("mismatch cached(=%d)\n", GET_UIOMEM_IOCTL_FLAGS_CACHED(dev_info));
        exit(-1);
    }

    sprintf(file_name, "/sys/class/uiomem/%s/coherent", device_name);
    if ((fd  = open(file_name, O_RDONLY)) < 0) {
        perror(file_name);
        exit(-1);
    }
    status = read(fd, attr, sizeof(attr));
    if (status < 0) {
        printf("Can not read %s errno=%d (%s)\n", file_name, errno, strerror(errno));
        exit(-1);
    }
    sscanf(attr, "%d", &coherent);
    close(fd);

    if (coherent != GET_UIOMEM_IOCTL_FLAGS_COHERENT(dev_info)) {
        printf("read %s => %s\n", file_name, attr);
        printf("mismatch coherent(=%d)\n", GET_UIOMEM_IOCTL_FLAGS_COHERENT(dev_info));
        exit(-1);
    }
}

int read_sync_variables(char* device_name, uint64_t* sync_offset, uint64_t* sync_size, int* sync_direction, int* sync_owner, int* sync_for_cpu, int* sync_for_device, int* sync_mode)
{
    int      fd;
    int      status;
    char     attr[1024];
    char     file_name[1024];

    sprintf(file_name, "/sys/class/uiomem/%s/sync_offset", device_name);
    if ((fd  = open(file_name, O_RDONLY)) < 0) {
        perror(file_name);
        exit(-1);
    }
    status = read(fd, attr, sizeof(attr));
    if (status < 0) {
        printf("Can not read %s errno=%d (%s)\n", file_name, errno, strerror(errno));
        exit(-1);
    }
    sscanf(attr, "%" SCNx64, sync_offset);
    close(fd);

    sprintf(file_name, "/sys/class/uiomem/%s/sync_size", device_name);
    if ((fd  = open(file_name, O_RDONLY)) < 0) {
        perror(file_name);
        exit(-1);
    }
    status = read(fd, attr, sizeof(attr));
    if (status < 0) {
        printf("Can not read %s errno=%d (%s)\n", file_name, errno, strerror(errno));
        exit(-1);
    }
    sscanf(attr, "%" SCNu64, sync_size);
    close(fd);

    sprintf(file_name, "/sys/class/uiomem/%s/sync_direction", device_name);
    if ((fd  = open(file_name, O_RDONLY)) < 0) {
        perror(file_name);
        exit(-1);
    }
    status = read(fd, attr, sizeof(attr));
    if (status < 0) {
        printf("Can not read %s errno=%d (%s)\n", file_name, errno, strerror(errno));
        exit(-1);
    }
    sscanf(attr, "%d", sync_direction);
    close(fd);

    sprintf(file_name, "/sys/class/uiomem/%s/sync_owner", device_name);
    if ((fd  = open(file_name, O_RDONLY)) < 0) {
        perror(file_name);
        exit(-1);
    }
    status = read(fd, attr, sizeof(attr));
    if (status < 0) {
        printf("Can not read %s errno=%d (%s)\n", file_name, errno, strerror(errno));
        exit(-1);
    }
    sscanf(attr, "%d", sync_owner);
    close(fd);

    sprintf(file_name, "/sys/class/uiomem/%s/sync_for_cpu", device_name);
    if ((fd  = open(file_name, O_RDONLY)) < 0) {
        perror(file_name);
        exit(-1);
    }
    status = read(fd, attr, sizeof(attr));
    if (status < 0) {
        printf("Can not read %s errno=%d (%s)\n", file_name, errno, strerror(errno));
        exit(-1);
    }
    sscanf(attr, "%d", sync_for_cpu);
    close(fd);

    sprintf(file_name, "/sys/class/uiomem/%s/sync_for_device", device_name);
    if ((fd  = open(file_name, O_RDONLY)) < 0) {
        perror(file_name);
        exit(-1);
    }
    status = read(fd, attr, sizeof(attr));
    if (status < 0) {
        printf("Can not read %s errno=%d (%s)\n", file_name, errno, strerror(errno));
        exit(-1);
    }
    sscanf(attr, "%d", sync_for_device);
    close(fd);

    sprintf(file_name, "/sys/class/uiomem/%s/sync_mode", device_name);
    if ((fd  = open(file_name, O_RDONLY)) < 0) {
        perror(file_name);
        exit(-1);
    }
    status = read(fd, attr, sizeof(attr));
    if (status < 0) {
        printf("Can not read %s errno=%d (%s)\n", file_name, errno, strerror(errno));
        exit(-1);
    }
    sscanf(attr, "%d", sync_mode);
    close(fd);

    return 0;
}

int check_ioctl_set_sync_for_cpu(char* driver_name, int uiomem_fd, int sync_offset, int sync_size, int sync_direction)
{
    int      status = 0;
    uint64_t sync_for_cpu = ((uint64_t)(sync_offset) <<32) | ((uint64_t)(sync_size) & 0xFFFFFFF0) | ((uint64_t)(sync_direction) << 2) | 1;
    
    status = xioctl(uiomem_fd, UIOMEM_IOCTL_SET_SYNC_FOR_CPU, &sync_for_cpu);
    if (status != 0) {
        perror("UIOMEM_IOCTL_SET_SYNC_FOR_CPU");
        exit(-1);
    }
    uint64_t var_sync_offset;
    uint64_t var_sync_size;
    int      var_sync_direction;
    int      var_sync_owner;
    int      var_sync_for_cpu;
    int      var_sync_for_device;
    int      var_sync_mode;
    status = read_sync_variables(driver_name, &var_sync_offset, &var_sync_size, &var_sync_direction, &var_sync_owner, &var_sync_for_cpu, &var_sync_for_device, &var_sync_mode);
    if (var_sync_for_cpu != 0) {
	printf("%s: sync_for_cpu is not 0\n", __func__);
	status = -1;
    }
    if (var_sync_for_device != 0) {
	printf("%s: sync_for_device is not 0\n", __func__);
	status = -1;
    }
    if (var_sync_owner != 0) {
	printf("%s: sync_owner is not 0\n", __func__);
	status = -1;
    }
    return status;
}

int check_ioctl_set_sync_for_device(char* driver_name, int uiomem_fd, int sync_offset, int sync_size, int sync_direction)
{
    int      status = 0;
    uint64_t sync_for_cpu = ((uint64_t)(sync_offset) <<32) | ((uint64_t)(sync_size) & 0xFFFFFFF0) | ((uint64_t)(sync_direction) << 2) | 1;
    
    status = xioctl(uiomem_fd, UIOMEM_IOCTL_SET_SYNC_FOR_DEVICE, &sync_for_cpu);
    if (status != 0) {
        perror("UIOMEM_IOCTL_SET_SYNC_FOR_DEVICE");
        exit(-1);
    }
    uint64_t var_sync_offset;
    uint64_t var_sync_size;
    int      var_sync_direction;
    int      var_sync_owner;
    int      var_sync_for_cpu;
    int      var_sync_for_device;
    int      var_sync_mode;
    status = read_sync_variables(driver_name, &var_sync_offset, &var_sync_size, &var_sync_direction, &var_sync_owner, &var_sync_for_cpu, &var_sync_for_device, &var_sync_mode);
    if (var_sync_for_cpu != 0) {
	printf("%s: sync_for_cpu is not 0\n", __func__);
	status = -1;
    }
    if (var_sync_for_device != 0) {
	printf("%s: sync_for_device is not 0\n", __func__);
	status = -1;
    }
    if (var_sync_owner != 1) {
	printf("%s: sync_owner is not 1\n", __func__);
	status = -1;
    }
    return status;
}

int check_ioctl_set_sync(char* driver_name, int uiomem_fd, uiomem_ioctl_sync_args* sync_args)
{
    int      status         = 0;
    uint64_t sync_offset    = sync_args->offset;
    uint64_t sync_size      = sync_args->size;;
    int      sync_direction = GET_UIOMEM_IOCTL_FLAGS_SYNC_DIR(sync_args);
    int      sync_command   = GET_UIOMEM_IOCTL_FLAGS_SYNC_CMD(sync_args);
	
    if (1) {
        printf("UIOMEM_IOCTL_SET_SYNC: SET\n");
	printf("    sync_offset    : %" PRIu64 "\n", sync_offset   );
	printf("    sync_size      : %" PRIu64 "\n", sync_size     );
	printf("    sync_direction : %d\n"         , sync_direction);
	printf("    sync_command   : %d\n"         , sync_command  );
    }
    status = xioctl(uiomem_fd, UIOMEM_IOCTL_SET_SYNC, sync_args);
    if (status != 0) {
        perror("UIOMEM_IOCTL_SET_SYNC");
        exit(-1);
    }
    uint64_t var_sync_offset;
    uint64_t var_sync_size;
    int      var_sync_direction;
    int      var_sync_owner;
    int      var_sync_for_cpu;
    int      var_sync_for_device;
    int      var_sync_mode;
    status = read_sync_variables(driver_name, &var_sync_offset, &var_sync_size, &var_sync_direction, &var_sync_owner, &var_sync_for_cpu, &var_sync_for_device, &var_sync_mode);
    if (1) {
        printf("UIOMEM_IOCTL_SET_SYNC: READ VARIABLES\n");
	printf("    sync_offset    : %" PRIu64 "\n", var_sync_offset   );
	printf("    sync_size      : %" PRIu64 "\n", var_sync_size     );
	printf("    sync_direction : %d\n"         , var_sync_direction);
	printf("    sync_owner     : %d\n"         , var_sync_owner    );
	printf("    sync_mode      : %d\n"         , var_sync_mode     );
    }
    if (var_sync_for_cpu != 0) {
	printf("%s: sync_for_cpu is not 0\n", __func__);
	status = -1;
    }
    if (var_sync_for_device != 0) {
	printf("%s: sync_for_device is not 0\n", __func__);
	status = -1;
    }
    if (sync_offset != 0) {
	if (var_sync_offset != sync_offset) {
            printf("%s: sync_offset mismatch (set=%" PRIu64 ", var=%" PRIu64 ")\n", __func__, sync_offset, var_sync_offset);
	    status = -1;
        }
    }
    if (sync_size != 0) {
	if (var_sync_size != sync_size) {
            printf("%s: sync_size mismatch (set=%" PRIu64 ", var=%" PRIu64 ")\n", __func__, sync_size, var_sync_size);
	    status = -1;
        }
    }
    if (var_sync_direction != sync_direction) {
        printf("%s: sync_direction mismatch (set=%d, var=%d)\n", __func__, sync_direction, var_sync_direction);
	status = -1;
    }
    if (sync_command == UIOMEM_IOCTL_FLAGS_SYNC_CMD_FOR_DEVICE) {
        if (var_sync_owner != 1) {
	    printf("%s: sync_owner is not 1\n", __func__);
	    status = -1;
        }
    }
    if (sync_command == UIOMEM_IOCTL_FLAGS_SYNC_CMD_FOR_CPU) {
        if (var_sync_owner != 0) {
	    printf("%s: sync_owner is not 0\n", __func__);
	    status = -1;
        }
    }
    return status;
}

int check_ioctl_get_sync(char* driver_name, int uiomem_fd)
{
    int                    status = 0;
    uiomem_ioctl_sync_args sync_args;
	
    status = xioctl(uiomem_fd, UIOMEM_IOCTL_GET_SYNC, &sync_args);
    if (status != 0) {
        perror("UIOMEM_IOCTL_GET_SYNC");
        exit(-1);
    }
    uint64_t sync_offset    = sync_args.offset;
    uint64_t sync_size      = sync_args.size;;
    int      sync_direction = GET_UIOMEM_IOCTL_FLAGS_SYNC_DIR  (&sync_args);
    int      sync_command   = GET_UIOMEM_IOCTL_FLAGS_SYNC_CMD  (&sync_args);
    int      sync_mode      = GET_UIOMEM_IOCTL_FLAGS_SYNC_MODE (&sync_args);
    int      sync_owner     = GET_UIOMEM_IOCTL_FLAGS_SYNC_OWNER(&sync_args);
    if (0) {
	printf("%s: get sync_args\n", __func__);
	printf("    sync_offset    = %ld\n", sync_offset   );
	printf("    sync_size      = %ld\n", sync_size     );
	printf("    sync_direction = %d\n" , sync_direction);
	printf("    sync_owner     = %d\n" , sync_owner    );
	printf("    sync_mode      = %d\n" , sync_mode     );
    }
    uint64_t var_sync_offset;
    uint64_t var_sync_size;
    int      var_sync_direction;
    int      var_sync_owner;
    int      var_sync_for_cpu;
    int      var_sync_for_device;
    int      var_sync_mode;
    status = read_sync_variables(driver_name, &var_sync_offset, &var_sync_size, &var_sync_direction, &var_sync_owner, &var_sync_for_cpu, &var_sync_for_device, &var_sync_mode);
    if (0) {
	printf("%s: read variable\n", __func__);
	printf("    sync_offset    = %ld\n", var_sync_offset   );
	printf("    sync_size      = %ld\n", var_sync_size     );
	printf("    sync_direction = %d\n" , var_sync_direction);
	printf("    sync_owner     = %d\n" , var_sync_owner    );
	printf("    sync_mode      = %d\n" , var_sync_mode     );
    }
    if (var_sync_for_cpu != 0) {
	printf("%s: sync_for_cpu is not 0\n", __func__);
	status = -1;
    }
    if (var_sync_for_device != 0) {
	printf("%s: sync_for_device is not 0\n", __func__);
	status = -1;
    }
    if (var_sync_offset != sync_offset) {
        printf("%s: sync_offset mismatch (get=%ld, var=%ld)\n", __func__, sync_offset, var_sync_offset);
	status = -1;
    }
    if (var_sync_size != sync_size) {
        printf("%s: sync_size mismatch (get=%ld, var=%ld)\n", __func__, sync_size, var_sync_size);
	status = -1;
    }
    if (var_sync_direction != sync_direction) {
        printf("%s: sync_direction mismatch (get=%d, var=%d)\n", __func__, sync_direction, var_sync_direction);
	status = -1;
    }
    if (var_sync_owner != sync_owner) {
	printf("%s: sync_owner mismatch (get=%d, var=%d)\n", __func__, sync_owner, var_sync_owner);
	status = -1;
    }
    return status;
}

int main(int argc, char* argv[])
{
    int                         status;
    int                         uiomem_fd;
    uiomem_ioctl_drv_info       uiomem_drv_info;
    uiomem_ioctl_dev_info       uiomem_dev_info;
    uiomem_ioctl_sync_args      uiomem_sync_args;
    uint64_t                    uiomem_size;
    uint64_t                    uiomem_phys_addr;
    char                        device_name[256];
    char                        device_file_name[1024];
    char*                       driver_version;
    char*                       sync_operation;
    int                         ioctl_version;
    uint64_t                    sync_offset;
    uint64_t                    sync_size;
    int                         sync_direction;
    int                         verbose         = 0;
    int                         opt;
    int                         optindex;
    struct option               longopts[] = {
      { "name"      , required_argument, NULL, 'n'},
      { "verbose"   , no_argument      , NULL, 'v'},
      { NULL        , 0                , NULL,  0 },
    };

    strncpy(device_name, "uiomem0", sizeof(device_name));
    while ((opt = getopt_long(argc, argv, "n:v", longopts, &optindex)) != -1) {
      switch (opt) {
        case 'n':
          strncpy(device_name, optarg, sizeof(device_name));
          break;
        case 'v':
          verbose = 1;
          break;
        default:
          printf("error options\n");
          break;
      }
    }

    //
    // uiomem_fd
    //
    sprintf(device_file_name, "/dev/%s", device_name);
    if ((uiomem_fd = open(device_file_name, O_RDWR)) < 0) {
        perror(device_file_name);
        exit(-1);
    }
    printf("DEVICE_NAME: %s\n", device_name);
    //
    // UIOMEM_IOCTL_GET_DRV_INFO
    //
    status = xioctl(uiomem_fd, UIOMEM_IOCTL_GET_DRV_INFO, &uiomem_drv_info);
    if (status != 0) {
        perror("UIOMEM_IOCTL_GET_DRV_INFO");
        exit(-1);
    } else {
        driver_version = &uiomem_drv_info.version[0];
        sync_operation = &uiomem_drv_info.sync_operation[0];
        ioctl_version  = GET_UIOMEM_IOCTL_FLAGS_IOCTL_VERSION(&uiomem_drv_info);
        printf("UIOMEM_IOCTL_GET_DRV_INFO: \n");
        printf("    driver_version : %s\n", driver_version);
        printf("    sync_operation : %s\n", sync_operation);
        printf("    ioctl_version  : %d\n", ioctl_version );
    }
    //
    // UIOMEM_IOCTL_GET_DEV_INFO
    //
    status = xioctl(uiomem_fd, UIOMEM_IOCTL_GET_DEV_INFO, &uiomem_dev_info);
    if (status != 0) {
        perror("UIOMEM_IOCTL_GET_DEV_INFO");
        exit(-1);
    } else {
        int  shareable = GET_UIOMEM_IOCTL_FLAGS_SHAREABLE(&uiomem_dev_info);
        int  cached    = GET_UIOMEM_IOCTL_FLAGS_CACHED(&uiomem_dev_info);
        int  coherent  = GET_UIOMEM_IOCTL_FLAGS_COHERENT(&uiomem_dev_info);
        printf("UIOMEM_IOCTL_GET_DEV_INFO: \n");
        printf("    size           : %"   PRIu64 "\n" , uiomem_dev_info.size);
        printf("    phys_addr      : 0x%" PRIx64 "\n" , uiomem_dev_info.addr);
        printf("    shareable      : %d\n" , shareable);
        printf("    cached         : %d\n" , cached);
        printf("    cohrent        : %d\n" , coherent);
    }
    //
    // UIOMEM_IOCTL_GET_SIZE
    //
    status = xioctl(uiomem_fd, UIOMEM_IOCTL_GET_SIZE, &uiomem_size);
    if (status != 0) {
        perror("UIOMEM_IOCTL_GET_SIZE");
        exit(-1);
    } else {
        printf("UIOMEM_IOCTL_GET_SIZE: \n");
        printf("    size           : %" PRIu64 "\n"   , uiomem_size);
    }
    if (uiomem_size != uiomem_dev_info.size) {
        printf("mismatch buffer size\n");
        exit(-1);
    }
    //
    // UIOMEM_IOCTL_GET_PHYS_ADDR
    //
    status = xioctl(uiomem_fd, UIOMEM_IOCTL_GET_PHYS_ADDR, &uiomem_phys_addr);
    if (status != 0) {
        perror("UIOMEM_IOCTL_GET_PHYS_ADDR");
        exit(-1);
    } else {
        printf("UIOMEM_IOCTL_GET_PHYS_ADDR: \n");
        printf("    phys_addr      : 0x%" PRIx64 "\n" , uiomem_phys_addr);
    }
    if (uiomem_phys_addr != uiomem_dev_info.addr) {
        printf("mismatch phys_addr\n");
        exit(-1);
    }
    //
    // check_driver_version()
    //
    status = check_driver_version(device_name, &uiomem_drv_info.version[0]);
    //
    // check_device_info()
    //
    status = check_device_info(device_name, &uiomem_dev_info);
    //
    // check_ioctl_set_sync_for_device() -> check_ioctl_set_sync_for_cpu()
    //
    sync_offset    = 0;
    sync_size      = uiomem_dev_info.size;
    sync_direction = 0;
    status = check_ioctl_set_sync_for_device(device_name, uiomem_fd, sync_offset, sync_size, sync_direction);
    status = check_ioctl_set_sync_for_cpu(device_name, uiomem_fd, sync_offset, sync_size, sync_direction);
    //
    // check_ioctl_set_sync_for_device() -> check_ioctl_set_sync_for_cpu() 
    //
    sync_offset    = uiomem_dev_info.size/2;
    sync_size      = uiomem_dev_info.size/4;
    sync_direction = 3;
    status = check_ioctl_set_sync_for_device(device_name, uiomem_fd, sync_offset, sync_size, sync_direction);
    status = check_ioctl_set_sync_for_cpu(device_name, uiomem_fd, sync_offset, sync_size, sync_direction);
    //
    // check_ioctl_set_sync() -> check_ioctl_get_sync() 
    //
    memset(&uiomem_sync_args, 0, sizeof(uiomem_sync_args));
    uiomem_sync_args.offset = 0;
    uiomem_sync_args.size   = uiomem_dev_info.size/2;
    SET_UIOMEM_IOCTL_FLAGS_SYNC_CMD(&uiomem_sync_args, UIOMEM_IOCTL_FLAGS_SYNC_CMD_FOR_DEVICE);
    SET_UIOMEM_IOCTL_FLAGS_SYNC_DIR(&uiomem_sync_args, 2);
    status = check_ioctl_set_sync(device_name, uiomem_fd, &uiomem_sync_args);
    status = check_ioctl_get_sync(device_name, uiomem_fd);
    //
    // check_ioctl_set_sync() -> check_ioctl_get_sync() 
    //
    memset(&uiomem_sync_args, 0, sizeof(uiomem_sync_args));
    uiomem_sync_args.offset = 0;
    uiomem_sync_args.size   = uiomem_dev_info.size/2;
    SET_UIOMEM_IOCTL_FLAGS_SYNC_CMD(&uiomem_sync_args, UIOMEM_IOCTL_FLAGS_SYNC_CMD_FOR_CPU);
    SET_UIOMEM_IOCTL_FLAGS_SYNC_DIR(&uiomem_sync_args, 2);
    status = check_ioctl_set_sync(device_name, uiomem_fd, &uiomem_sync_args);
    status = check_ioctl_get_sync(device_name, uiomem_fd);
    //
    // check_ioctl_set_sync() -> check_ioctl_get_sync() 
    //
    memset(&uiomem_sync_args, 0, sizeof(uiomem_sync_args));
    uiomem_sync_args.offset = uiomem_dev_info.size/4;
    uiomem_sync_args.size   = uiomem_dev_info.size/4;
    SET_UIOMEM_IOCTL_FLAGS_SYNC_CMD(&uiomem_sync_args, UIOMEM_IOCTL_FLAGS_SYNC_CMD_FOR_DEVICE);
    SET_UIOMEM_IOCTL_FLAGS_SYNC_DIR(&uiomem_sync_args, 0);
    status = check_ioctl_set_sync(device_name, uiomem_fd, &uiomem_sync_args);
    status = check_ioctl_get_sync(device_name, uiomem_fd);
	    
    close(uiomem_fd);
    return status;
}
