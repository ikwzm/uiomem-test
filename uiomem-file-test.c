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
#include        "uiomem.h"

struct test_time
{
    struct timeval main;
    struct timeval sync_for_cpu;
    struct timeval sync_for_dev;
    struct timeval total;
};

static void diff_time(struct timeval* run_time, struct timeval* start_time, struct timeval* end_time)
{
    if (end_time->tv_usec < start_time->tv_usec) {
        run_time->tv_sec  = end_time->tv_sec  - start_time->tv_sec  - 1;
        run_time->tv_usec = end_time->tv_usec - start_time->tv_usec + 1000*1000;
    } else {
        run_time->tv_sec  = end_time->tv_sec  - start_time->tv_sec ;
        run_time->tv_usec = end_time->tv_usec - start_time->tv_usec;
    }
}

int uiomem_mmap_write_test(struct uiomem* uiomem, void* buf, unsigned int size, int sync, struct test_time* time)
{
    int            fd;
    void*          iomem;
    struct timeval test_start_time, test_end_time;
    struct timeval main_start_time, main_end_time;

    if (sync == 0)
        uiomem_set_sync_area(uiomem, 0, size, UIOMEM_WRITE_ONLY);
      
    if ((fd  = uiomem_open(uiomem, O_RDWR | ((sync)?O_SYNC:0))) != -1) {
        iomem = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        gettimeofday(&test_start_time, NULL);
        if (sync == 0)
          uiomem_sync_for_cpu(uiomem, fd);
        gettimeofday(&main_start_time, NULL);
        memcpy(iomem, buf, size);
        gettimeofday(&main_end_time, NULL);
        if (sync == 0)
          uiomem_sync_for_dev(uiomem, fd);
        gettimeofday(&test_end_time  , NULL);
        if (time != NULL) {
            diff_time(&time->total       , &test_start_time, &test_end_time  );
            diff_time(&time->sync_for_cpu, &test_start_time, &main_start_time);
            diff_time(&time->main        , &main_start_time, &main_end_time  );
            diff_time(&time->sync_for_dev, &main_end_time  , &test_end_time  );
        }
        (void)close(fd);
        return 0;
    } else {
        return -1;
    }
}

int uiomem_mmap_read_test(struct uiomem* uiomem, void* buf, unsigned int size, int sync, struct test_time* time)
{
    int            fd;
    void*          iomem;
    struct timeval test_start_time, test_end_time;
    struct timeval main_start_time, main_end_time;

    if (sync == 0)
        uiomem_set_sync_area(uiomem, 0, size, UIOMEM_READ_ONLY);
      
    if ((fd  = uiomem_open(uiomem, O_RDWR | ((sync)?O_SYNC:0))) != -1) {
        iomem = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        gettimeofday(&test_start_time, NULL);
        if (sync == 0)
            uiomem_sync_for_cpu(uiomem, fd);
        gettimeofday(&main_start_time, NULL);
        memcpy(buf, iomem, size);
        gettimeofday(&main_end_time  , NULL);
        if (sync == 0)
            uiomem_sync_for_dev(uiomem, fd);
        gettimeofday(&test_end_time  , NULL);
        if (time != NULL) {
            diff_time(&time->total       , &test_start_time, &test_end_time  );
            diff_time(&time->sync_for_cpu, &test_start_time, &main_start_time);
            diff_time(&time->main        , &main_start_time, &main_end_time  );
            diff_time(&time->sync_for_dev, &main_end_time  , &test_end_time  );
        }
        close(fd);
        return 0;
    } else {
        return -1;
    }
}

int uiomem_file_write_test(struct uiomem* uiomem, void* buf, unsigned int size, int sync, struct test_time* time)
{
    int            fd;
    int            len;
    void*          ptr;
    struct timeval test_start_time, test_end_time;
    struct timeval main_start_time, main_end_time;

    if (sync == 0)
        uiomem_set_sync_area(uiomem, 0, size, UIOMEM_WRITE_ONLY);
      
    if ((fd  = uiomem_open(uiomem, O_RDWR | ((sync)?O_SYNC:0))) != -1) {
        gettimeofday(&test_start_time, NULL);
        if (sync == 0)
            uiomem_sync_for_cpu(uiomem, fd);
        gettimeofday(&main_start_time, NULL);
        len = size;
        ptr = buf;
        while(len > 0) {
            int count = write(fd, ptr, len);
            if (count < 0) {
                break;
            }
            ptr += count;
            len -= count;
        }
        gettimeofday(&main_end_time, NULL);
        if (sync == 0)
            uiomem_sync_for_dev(uiomem, fd);
        gettimeofday(&test_end_time, NULL);
        if (time != NULL) {
            diff_time(&time->total       , &test_start_time, &test_end_time  );
            diff_time(&time->sync_for_cpu, &test_start_time, &main_start_time);
            diff_time(&time->main        , &main_start_time, &main_end_time  );
            diff_time(&time->sync_for_dev, &main_end_time  , &test_end_time  );
        }
        (void)close(fd);
        return 0;
    } else {
        return -1;
    }
}

int uiomem_file_read_test(struct uiomem* uiomem, void* buf, unsigned int size, int sync, struct test_time* time)
{
    int            fd;
    int            len;
    void*          ptr;
    struct timeval test_start_time, test_end_time;
    struct timeval main_start_time, main_end_time;

    if (sync == 0)
        uiomem_set_sync_area(uiomem, 0, size, UIOMEM_READ_ONLY);
      
    if ((fd  = uiomem_open(uiomem, O_RDWR | ((sync)?O_SYNC:0))) != -1) {
        gettimeofday(&test_start_time, NULL);
        if (sync == 0)
            uiomem_sync_for_cpu(uiomem, fd);
        gettimeofday(&main_start_time, NULL);
        len = size;
        ptr = buf;
        while(len > 0) {
            int count = read(fd, ptr, len);
            if (count < 0) {
                break;
            }
            ptr += count;
            len -= count;
        }
        gettimeofday(&main_end_time  , NULL);
        if (sync == 0)
            uiomem_sync_for_dev(uiomem, fd);
        gettimeofday(&test_end_time  , NULL);
        if (time != NULL) {
            diff_time(&time->total       , &test_start_time, &test_end_time  );
            diff_time(&time->sync_for_cpu, &test_start_time, &main_start_time);
            diff_time(&time->main        , &main_start_time, &main_end_time  );
            diff_time(&time->sync_for_dev, &main_end_time  , &test_end_time  );
        }
        close(fd);
        return 0;
    } else {
        return -1;
    }
}

int main(int argc, char* argv[])
{
    struct uiomem*    uiomem;
    char              device_name[256];
    unsigned int      err_count = 0;
    size_t            buf_size;
    void*             null_buf = NULL;
    void*             src0_buf = NULL;
    void*             src1_buf = NULL;
    void*             temp_buf = NULL;
    int               verbose  = 0;
    int               opt;
    int               optindex;
    struct option     longopts[] = {
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
    printf("device=%s\n", device_name);
    //
    // uiomem
    //
    if ((uiomem = uiomem_create(device_name)) == NULL) {
        goto done;
    }
    printf("driver_version=%s\n", uiomem->version      );
    printf("sync_operation=%s\n", uiomem->sync_op      );
    printf("ioctl_version=%d\n" , uiomem->ioctl_version);
    printf("size=%ld\n"         , uiomem->size         );
    printf("shareable=%d\n"     , uiomem->shareable    );
    printf("cached=%d\n"        , uiomem->cached       );
    printf("coherent=%d\n"      , uiomem->coherent     );
    //
    // initilize buffers 
    //
    buf_size = uiomem->size;
    if ((null_buf = malloc(buf_size)) == NULL) {
        printf("Can not malloc null_buf\n");
        goto done;
    } else {
        memset(null_buf, 0, buf_size);
    }
    if ((src0_buf = malloc(buf_size)) == NULL) {
        printf("Can not malloc src0_buf\n");
        goto done;
    } else {
        int*   word  = (int *)src0_buf;
        size_t words = buf_size/sizeof(int);
        for(int i = 0; i < words; i++) {
            word[i] = i;
        }
    }
    if ((src1_buf = malloc(buf_size)) == NULL) {
        printf("Can not malloc src1_buf\n");
        goto done;
    } else {
        int*   word  = (int *)src1_buf;
        size_t words = buf_size/sizeof(int);
        for(int i = 0; i < words; i++) {
            word[i] = ~i;
        }
    }
    if ((temp_buf = malloc(buf_size)) == NULL) {
        printf("Can not malloc temp_buf\n");
        goto done;
    } else {
        memset(temp_buf, 0, buf_size);
    }
    //
    // define TEST1()
    //
#define TEST1(w_type,w_sync,r_type,r_sync,src,dst,size)    \
    {                                                      \
        struct test_time w_time;                           \
        struct test_time r_time;                           \
        memset(dst, 0, buf_size);                          \
        printf(#w_type " write test : sync=%d ", w_sync);  \
        uiomem_##w_type##_write_test(uiomem, src, size, w_sync, &w_time); \
        printf("time=%ld.%06ld sec (%ld.%06ld sec)\n", w_time.total.tv_sec, w_time.total.tv_usec, w_time.main.tv_sec, w_time.main.tv_usec); \
        printf(#r_type " read  test : sync=%d ", r_sync);  \
        uiomem_##r_type##_read_test (uiomem, dst, size, r_sync, &r_time); \
        printf("time=%ld.%06ld sec (%ld.%06ld sec)\n", r_time.total.tv_sec, r_time.total.tv_usec, r_time.main.tv_sec, r_time.main.tv_usec); \
        if (memcmp(dst, src, size) != 0) {   \
            printf("compare = mismatch\n");  \
            err_count++;                     \
        } else {                             \
            printf("compare = ok\n");        \
        }                                    \
    }

    TEST1(mmap, 1, mmap, 1, src0_buf, temp_buf, buf_size);
    TEST1(mmap, 0, mmap, 1, src1_buf, temp_buf, buf_size);
    TEST1(mmap, 1, mmap, 0, src0_buf, temp_buf, buf_size);
    TEST1(mmap, 0, mmap, 0, src1_buf, temp_buf, buf_size);
    TEST1(file, 1, mmap, 0, src0_buf, temp_buf, buf_size);
    TEST1(file, 0, mmap, 0, src1_buf, temp_buf, buf_size);
    TEST1(mmap, 0, file, 1, src0_buf, temp_buf, buf_size);
    TEST1(mmap, 0, file, 0, src1_buf, temp_buf, buf_size);

 done:
    if (temp_buf != NULL)
        free(temp_buf);
    if (src1_buf != NULL)
        free(src1_buf);
    if (src0_buf != NULL)
        free(src0_buf);
    if (null_buf != NULL)
        free(null_buf);
    if (uiomem   != NULL)
        uiomem_destroy(uiomem);
}
