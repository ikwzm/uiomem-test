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

int uiomem_mmap_write_test(struct uiomem* uiomem, unsigned int size, int sync, struct test_time* time)
{
    int            fd;
    void*          iomem;
    uint64_t*      word_vec;
    size_t         words = size/sizeof(uint64_t);
    struct timeval test_start_time, test_end_time;
    struct timeval main_start_time, main_end_time;

    if (sync == 0)
        uiomem_set_sync_area(uiomem, 0, size, UIOMEM_WRITE_ONLY);
      
    if ((fd  = uiomem_open(uiomem, O_RDWR | ((sync)?O_SYNC:0))) != -1) {
        iomem = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        if (iomem == MAP_FAILED) {
            close(fd);
            return -1;
        }
        word_vec = (uint64_t*)iomem;
        gettimeofday(&test_start_time, NULL);
        if (sync == 0)
          uiomem_sync_for_cpu(uiomem, fd);
        gettimeofday(&main_start_time, NULL);
        for (int i = 0; i < words; i++) {
            word_vec[i] = i;
        }
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

int uiomem_mmap_read_test(struct uiomem* uiomem, unsigned int size, int sync, struct test_time* time)
{
    int            fd;
    void*          iomem;
    uint64_t*      word_vec;
    size_t         words = size/sizeof(uint64_t);
    int            mismatch = 0;
    struct timeval test_start_time, test_end_time;
    struct timeval main_start_time, main_end_time;

    if (sync == 0)
        uiomem_set_sync_area(uiomem, 0, size, UIOMEM_READ_ONLY);
      
    if ((fd  = uiomem_open(uiomem, O_RDWR | ((sync)?O_SYNC:0))) != -1) {
        iomem = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        if (iomem == MAP_FAILED) {
            close(fd);
            return -1;
        }
        word_vec = (uint64_t*)iomem;
        gettimeofday(&test_start_time, NULL);
        if (sync == 0)
            uiomem_sync_for_cpu(uiomem, fd);
        gettimeofday(&main_start_time, NULL);
        for (int i = 0; i < words; i++) {
            if (word_vec[i] != i)
                mismatch++;
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
        return mismatch;
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
    buf_size = uiomem->size;

#define TEST1(w_sync,r_sync,size,try_num)                       \
    {                                                           \
        int      status = 0;                                    \
        struct   test_time w_time;                              \
        struct   test_time r_time;                              \
        size_t   total_size   = 0;                              \
        int64_t  w_total_usec = 0;                              \
        int64_t  w_main_usec  = 0;                              \
        int64_t  r_total_usec = 0;                              \
        int64_t  r_main_usec  = 0;                              \
        for (int try = 0; try < try_num; try++) {               \
            if (verbose > 0)                                    \
                printf("mmap write test : sync=%d ", w_sync);   \
            status = uiomem_mmap_write_test(uiomem, size, w_sync, &w_time); \
            if (status != 0) {     \
                printf("error\n"); \
                exit(-1);          \
            }                      \
            w_total_usec += (int64_t)w_time.total.tv_sec*(1000*1000) + w_time.total.tv_usec; \
            w_main_usec  += (int64_t)w_time.main .tv_sec*(1000*1000) + w_time.main .tv_usec; \
            if (verbose > 0)                                    \
                printf("time=%ld.%06ld sec (%ld.%06ld sec)\n", w_time.total.tv_sec, w_time.total.tv_usec, w_time.main.tv_sec, w_time.main.tv_usec); \
            if (verbose > 0)                                    \
                printf("mmap read  test : sync=%d ", r_sync);   \
            status = uiomem_mmap_read_test (uiomem, size, r_sync, &r_time); \
            if (status < 0) {      \
                printf("error\n"); \
                exit(-1);          \
            }                      \
            r_total_usec += (int64_t)r_time.total.tv_sec*(1000*1000) + r_time.total.tv_usec; \
            r_main_usec  += (int64_t)r_time.main .tv_sec*(1000*1000) + r_time.main .tv_usec; \
            if (verbose > 0)                                    \
                printf("time=%ld.%06ld sec (%ld.%06ld sec)\n", r_time.total.tv_sec, r_time.total.tv_usec, r_time.main.tv_sec, r_time.main.tv_usec); \
            if (status == 0) {                   \
                if (verbose > 0)                 \
                    printf("compare = ok\n");    \
            } else {                             \
                printf("compare = mismatch\n");  \
                err_count++;                     \
            }                                    \
            total_size += size;                  \
        }                                        \
        printf("mmap write test : sync=%d throughput=%5.1f MBytes/sec\n", w_sync, (double)total_size/((double)w_total_usec)); \
        printf("mmap read  test : sync=%d throughput=%5.1f MBytes/sec\n", r_sync, (double)total_size/((double)r_total_usec)); \
    }
    TEST1(0, 0, buf_size, 10);
    TEST1(1, 1, buf_size, 10);

 done:
    if (uiomem   != NULL)
        uiomem_destroy(uiomem);
}
