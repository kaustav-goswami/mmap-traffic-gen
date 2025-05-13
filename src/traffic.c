#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>  
#include <fcntl.h>   
#include <errno.h>   
#include <limits.h>  
#include <assert.h>  
#include <time.h>    
#include <string.h>
                     
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

void fatal(char *reason) {
    printf("%s\n", reason);
    exit(-1);
}
// an asm utility function on x86 to force flush the cache.
// [[gnu::unused]] static inline __attribute__((always_inline)) 
void clflushopt(volatile void *p) {
    // @params
    //
    // p: a pointer to the memory 
    asm volatile("clflushopt (%0)\n"::"r"(p) 
    : "memory");
}

int create_ladder(int *ptr, size_t size, size_t length) {
    // @params
    // :ptr: mmap pointer to a /dev
    // :size: size of the device memory
    // :length: number of memory addresses to create
    //
    // @returns
    // :int: a dummy number to prevent compiler from doing any BS

    // make sure that the ptr is now backed by a standard array
    int *arr = &ptr[0];

    // accessing indices of arr will create a ladder pattern
    // figure out the max size:
    const size_t limit = size / sizeof(int);

    // ladder params needed to generate traffic
    const int ladder_length = 4;
    const int ladder_drop_size = 8;
    // we need the cache line size to not interfere our traffic. This is 64
    // Bytes
    const int ladder_interval = 64 / sizeof(int);
    const int ladder_rise = 13;

    const int base_addr = 0;

    size_t processed_index = 0;
    // read the processed index! This creates exactly one memory request!
    int dummy_single_variable_to_read = arr[processed_index];
    // confident?
    clflushopt(&arr[processed_index]);

    // start generating addresses
    for (size_t i = 0 ; i < length ; i++) {
        // find the ladder index for the current iteration
        int this_ladder_index = i % ladder_length;
        // find the amount of data processed by the traffic generator.
   
        // until LADDER_LENGTH - 1, there will be a const stride. Not necessarily!
        if (this_ladder_index == 0) {
            // this is a rise
            processed_index += ladder_rise;
        }
        else if ((this_ladder_index > 0) && 
                        (this_ladder_index < (ladder_length - 1))) {
            processed_index += (ladder_interval + this_ladder_index);
    }
        // If this is drop
        else if (this_ladder_index == (ladder_length - 1)) {
            processed_index -= ladder_drop_size;
        }
        else {
            // this should not happen
            fatal("error! unhandled exception at create_ladder(..)");
        }

        // prevent the compiler from doing any BS
        dummy_single_variable_to_read += arr[processed_index];
        clflushopt(&arr[processed_index]);

        // loosing precision!
        printf("%zu %zu, ", i, processed_index);
    }
    return dummy_single_variable_to_read;
}


int main() {
    // this is a simple program that mmaps into a /dev and tries to generate
    // specific memory access patters using addresses/indices.
    size_t size = 1024 * 1024 * 1024;
    int length = 100;
    int fd;
    // It is assumed that we are working with DAX devices. Will change change
    // to something else if needed later.
    const char *path = "/dev/zero";
    fd = open(path, O_RDWR);
    if (fd < 0) {
        printf("Error mounting! Make sure that the mount point is valid!\n");
        exit(EXIT_FAILURE);
    }

    // Try allocating the required size for the graph. This might be
    // complicated if the graph is very large!
    // With all the testings, it seems that the mmap call is not a problem.
    // Please submit a bug report/issue if you see an error.

    // Make sure only the allocator has READ/WRITE permission. This forces
    // software coherence!
    int *ptr = (int *) mmap (NULL, size,
                                    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    printf("\ndone: %d\n", create_ladder(ptr, size, length));
    return 0;
}
