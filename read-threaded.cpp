#include <boost/thread/mutex.hpp>
#include "boost/threadpool.hpp"
#include "boost/bind.hpp"
#include <openssl/md5.h>
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
#include <set>
#include <vector>

extern "C" {
    #include "gethrxtime.h"
    #include "lz4/lz4.h"
    #include "lz4/lz4hc.h"
}

using namespace boost::threadpool;
using namespace std;

// Globals
int (*compress)(const char*, char*, int) = 0;
int hashing_enabled = 0;
int verbose = 0;
set<string> hash_pool;
boost::mutex hash_mutex;


// 10MB Block size seams optimal on our hardware
ssize_t block_size = 10485760;

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

int exists(const char* hash){
    set<string>::iterator it;
    string key(hash);

    // Will lock the mutex until we lose scope
    boost::mutex::scoped_lock l(hash_mutex);

    it = hash_pool.find(key);
    // hash doesn't already exist
    if( it == hash_pool.end() ){
        // Add the hash to the pool
        hash_pool.insert(key);
        return 0;
    }
    return 1; 
}

void read_block(const char* file, ssize_t offset, int num_blocks){
    unsigned char hash[MD5_DIGEST_LENGTH];
    char hash_print_buf[MD5_DIGEST_LENGTH * 2];
    char* compress_buf = 0;
    char *local_buf = 0;
    int fd = 0;

    printf("Reading From Offset %ld\n", offset);
    if( (fd = open(file, O_RDONLY | O_DIRECT)) == -1){
        printf("T - Error Opening: '%s'\n", strerror(errno));
        return;
    }
    
    if( lseek(fd, offset, SEEK_SET) == -1){
        printf("T - Error Seeking to '%ld' - %s\n", offset, strerror(errno));
        return;
    }

    if(compress){
        // lz4 may need more space to compress, ask
        // it for the worst possible resulting buffer size
        compress_buf = (char*)malloc(LZ4_compressBound(block_size));
    }

    if(posix_memalign((void**)&local_buf, 512, block_size) != 0){
        printf("Error Allocating memory\n");
    }

    for(int i=0; i < num_blocks; ++i){
        int count = read(fd, local_buf, block_size);
        if (count < 0 || errno == EINTR){
            printf("T - Read Error: '%s'\n", strerror(errno));
            return;
        }

        if(hashing_enabled) {
            // Hash the block
            MD5((const unsigned char*)local_buf, block_size, hash);
            // Convert to string (much like python would) for use in a set() and printf()
            for(int j=0; j < MD5_DIGEST_LENGTH; j++) sprintf(hash_print_buf + (j * 2), "%02x", hash[j]);
            if(verbose > 1){ printf("Hashed: %s\n", hash_print_buf); }

            // Did we already compress this block?
            if(exists(hash_print_buf)){
                if(verbose){ printf("Skip Compress: %s\n", hash_print_buf); }
                continue;
            }
        }

        if(compress){
            // Preform the compression
            ssize_t compress_size = compress(local_buf, compress_buf, block_size);
            if(verbose){ printf("Compressed block %d from %lu to: %lu Bytes\n", i, block_size, compress_size); }
        }
    }
    free(compress_buf);
    free(local_buf);
}


void usage(void) {
    printf("read-threaded -d <device> [ -b <block_size> -j <num_of_jobs> -c <compression level> ]\n");
    exit(1);
}


int main(int argc, char **argv){
    uintmax_t total = 0;
    int num_jobs =  10;
    char *device = 0;
    int c;

    while ((c = getopt (argc, argv, "hvd:b:j:c:")) != -1) {
        switch (c) {
            case 'd': device = optarg; break;
            case 'b': block_size = atoll(optarg); break;
            case 'j': num_jobs = atoi(optarg); break;
            case 'v': verbose += 1; break;
            case 'h': hashing_enabled = 1; break;
            case 'c':
                // Compression Level
                switch (atoi(optarg)) {
                    case 0 : compress = 0; break;
                    case 1 : compress = LZ4_compress; break;
                    case 2 : compress = LZ4_compressHC; break;
                    default: break;
                }
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

    if(verbose > 1){
        printf("MD5 Hash Set contains\n");
        for (set<string>::iterator it = hash_pool.begin(); it!=hash_pool.end(); ++it) {
            printf("- %s\n", (*it).c_str());
        }
    }

    if (start_time < now) {
        uintmax_t delta_xtime = now;
        delta_xtime -= start_time;
        double delta_s = delta_xtime / XTIME_PRECISIONe0;
        printf("Throughput %g s @ %g MiB/s\n", delta_s, (file_size / delta_s) / 1048576);
    }
    return 0;
}
