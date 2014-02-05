#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>

#include "gethrxtime.h"

int main(int argc, char **argv){
    double XTIME_PRECISIONe0 = XTIME_PRECISION;
    char const *bytes_per_second;
    uintmax_t delta_xtime;
    ssize_t size = 1073741824; // 1G
    //ssize_t size = 4294967296; // 4G
    //ssize_t size = 4194304; // 4MB
    uintmax_t total = 0;
    double delta_s;
    ssize_t count;
    int fd = 0;
    void *buf;

    if (argc == 1){
        printf("Please supply a device to read\n");
        exit(1);
    }
    // Allocate a 1G buffer
    if(posix_memalign(&buf, 512, size) != 0){
        printf("Error Allocating memory\n");
    }
    if( (fd = open(argv[1], O_RDONLY)) == -1){
    //if( (fd = open(argv[1], O_RDONLY| O_DIRECT)) == -1){
        printf("File Error: '%s'\n", strerror(errno));
        return 1;
    }

    xtime_t start_time = gethrxtime();
    for (;;){
        count = read(fd, buf, size);
        if (count < 0 || errno == EINTR) {
            printf("Read Error: '%s'\n", strerror(errno));
            return 1;
        }
        total += count;
        if (count == 0){
            xtime_t now = gethrxtime();
            if (start_time < now) {
                delta_xtime = now;
                delta_xtime -= start_time;
                delta_s = delta_xtime / XTIME_PRECISIONe0;
                printf("Throughput %gs @ %gMB/s\n", delta_s, (total / delta_s) / 1048576);
            }
            return 0;
        }
    }
}
