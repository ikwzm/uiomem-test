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
#ifndef         UIOMEM_H
#define         UIOMEM_H

#include        <stdint.h>
#include        <stddef.h>

#ifdef          USE_UIOMEM_IOCTL
#include        "uiomem-ioctl.h"
#endif

struct uiomem
{
    char*    name;
    char*    dev_name;
    char*    sys_path;
    char*    version;
    char*    sync_op;
    size_t   size;
    int      shareable;
    int      cached;
    int      coherent;
    int      ioctl_version;
    int      sync_for_cpu_file;
    int      sync_for_dev_file;
    char     sync_command_str[1024];
    int      sync_command_len;
    uint64_t sync_command;
#ifdef USE_UIOMEM_IOCTL
    uiomem_ioctl_sync_args sync_for_cpu_args;
    uiomem_ioctl_sync_args sync_for_dev_args;
#endif
};

#define  UIOMEM_READ_WRITE  0
#define  UIOMEM_WRITE_ONLY  1
#define  UIOMEM_READ_ONLY   2

struct uiomem* uiomem_create(char* name);
void uiomem_destroy(struct uiomem* this);
int  uiomem_open(struct uiomem* this, int flags);
void uiomem_set_sync_area(struct uiomem* this, unsigned int offset, unsigned int size, int direction);
size_t uiomem_sync_for_cpu(struct uiomem* this, int fd);
size_t uiomem_sync_for_dev(struct uiomem* this, int fd);

#endif
