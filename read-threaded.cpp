#include "boost/threadpool.hpp"
#include "boost/bind.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>
#include <stdlib.h>

extern "C" {
  #include "gethrxtime.h"
}

using namespace boost::threadpool;

// 1 gig block size
ssize_t block_size = 1073741824;

ssize_t size(const char* file){

    int fd = 0;
    if( (fd = open(file, O_RDONLY | O_DIRECT)) == -1){
        printf("Error Opening: '%s'\n", strerror(errno));
        exit(1);
    }
    ssize_t offset = lseek(fd, 0, SEEK_END);
    if( offset == -1){
        printf("Error Seeking to EOF - %s\n", strerror(errno));
        exit(1);
    }
    return offset;
}

void read_block(const char* file, ssize_t offset, int num_blocks){
    printf("Reading From Offset %ld\n", offset);
    int fd = 0;
    if( (fd = open(file, O_RDONLY | O_DIRECT)) == -1){
        printf("T - Error Opening: '%s'\n", strerror(errno));
        return;
    }
    
    if( lseek(fd, offset, SEEK_SET) == -1){
        printf("T - Error Seeking to '%ld' - %s\n", offset, strerror(errno));
        return;
    }

    void *local_buf;
    if(posix_memalign(&local_buf, 512, block_size) != 0){
        printf("Error Allocating memory\n");
    }

    for ( int i=0; i < num_blocks; ++i){
        int count = read(fd, local_buf, block_size);
        if (count < 0 || errno == EINTR){
            printf("T - Read Error: '%s'\n", strerror(errno));
            return;
        }
    }

    free(local_buf);
}


void usage(void) {
    printf("read-threaded -d <device> [ -b <block_size> -j <num_of_jobs> ]\n");
    exit(1);
}


int main(int argc, char **argv){
    uintmax_t total = 0;
    int num_jobs =  10;
    char *device = 0;
    int c;

    while ((c = getopt (argc, argv, "d:b:j:")) != -1) {
        switch (c) {
            case 'd':
                device = optarg; 
            break;
            case 'b':
                block_size = atoll(optarg);
            break;
            case 'j':
                num_jobs = atoi(optarg);
            break;
            default:
                printf("Unknown option character `\\x%x'.\n", optopt);
                usage();
        }
    }
    
    if(!device){
        printf("Please supply a device to read with the -d option\n");
        usage();
    }

    ssize_t file_size = size(device);

    // figure the total number of blocks on the device
    int total_blocks = (int)(file_size / block_size);
    // figure how many blocks a threads should read before quiting
    int blocks_per_thread = total_blocks / num_jobs;
    pool tp(num_jobs);

    printf("file-size: %ld block-size %ld total-blocks: %d blocks-per-thread: %d \n", file_size, block_size, total_blocks, blocks_per_thread);
    xtime_t start_time = gethrxtime();
    for(int i=0; i < num_jobs; ++i) {
        // figure the offset the threads should start reading at
        ssize_t offset = i * (block_size * blocks_per_thread);
        // Start a read thread
        tp.schedule(boost::bind(read_block, device, offset, blocks_per_thread));
    }
    tp.wait();

    // Spit out our stats
    double XTIME_PRECISIONe0 = XTIME_PRECISION;
    xtime_t now = gethrxtime();
    if (start_time < now) {
        uintmax_t delta_xtime = now;
        delta_xtime -= start_time;
        double delta_s = delta_xtime / XTIME_PRECISIONe0;
        printf("Throughput %g s @ %g MiB/s\n", delta_s, (file_size / delta_s) / 1048576);
    }
    return 0;
}


/*int main(int argc, char **argv){

    // Create a thread pool.
    pool tp(2);

    // Add some tasks to the pool.
    tp.schedule(&first_task);
    tp.schedule(&second_task);
    tp.schedule(&third_task);

    tp.wait();

    return 0;
}*/
