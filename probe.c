#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <papi.h>
#include <unistd.h>

//#define PAGE_SIZE 4096
#define HUGE_PAGE_SIZE (2 * 1024 * 1024)
#define NUM_RUNS 1
//#define L1_TLB_SIZE_4KB 64
//#define L2_TLB_SIZE 1536  // Assuming both 4KB and 2MB share the same L2 TLB size ?
#define HUGE_PAGE_ALLOCATION 1536
#define BASE ((void *)0x133800000000ULL)
#define MAP_HUGE_2MB    (21 << MAP_HUGE_SHIFT)
//#define NUM_PAGES_TO_ACCESS (L1_TLB_SIZE_4KB)
#define OFFSET (12 * L1_TLB_SIZE_4KB * PAGE_SIZE)

static inline uint64_t rdtsc() {
    uint32_t lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}//rdtscp

void pin_to_core(int core) {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(core, &mask);

    if (sched_setaffinity(0, sizeof(mask), &mask) == -1) {
        perror("Could not set CPU affinity");
        exit(1);
    }
}

int available_hugepages() {
    int fd = open("/sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages", O_RDONLY);
    if (fd == -1) {
        perror("Failed to open nr_hugepages");
        exit(1);
    }

    char buffer[16];
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    if (bytes_read <= 0) {
        perror("Failed to read nr_hugepages");
        exit(1);
    }
    buffer[bytes_read] = '\0';
    return atoi(buffer);
}

char *allocate_pages(size_t num_pages, size_t page_size, int hugepage) {
    int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE ;
    if (hugepage) {
        flags |= MAP_HUGETLB | MAP_HUGE_2MB;
    }
    //mmaps = NULL, HUGE_PAGE_ALLOC * PAGE_SIZE
    //char *pages = mmap(NULL, num_pages * page_size, PROT_READ | PROT_WRITE, flags, -1, 0);
    char *pages = mmap(BASE, num_pages * page_size, PROT_READ | PROT_WRITE, flags, -1, 0);
    if (pages == MAP_FAILED) {
        perror("Failed to allocate pages");
        exit(1);
    }
    return pages;
}

uint64_t access_and_time(char *pages, size_t page_size, size_t num_pages) {
    uint64_t start = rdtsc();

    for (size_t i = 0; i < num_pages; i++) {
        char *addr = pages + i * page_size;
        *addr = *addr + 1;
    }
    uint64_t end = rdtsc();
    return end - start;
}


// uint64_t access_and_time(char *pages, size_t page_size) {
//     uintptr_t * indices[NUM_PAGES_TO_ACCESS];
//     for (int i = 0; i < NUM_PAGES_TO_ACCESS; i++) {
//         indices[i] = (uintptr_t *)(i < NUM_PAGES_TO_ACCESS - 1 ? &indices[(i*OFFSET) + 1] : &indices[0]);//linking
//     }

//     uintptr_t *chaser = indices[0];
//     uint64_t start = rdtsc();
//     for (int i = 0; i < NUM_PAGES_TO_ACCESS; i++) {
//         chaser=(uintptr_t*)*chaser;//chasing
//     }
//     uint64_t end = rdtsc();

//     return end - start;
// }

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <page size in KB> <number of pages>\n", argv[0]);
        return 1;
    }
    size_t PAGE_SIZE = (strtoul(argv[1], NULL, 10))*1024;
    size_t NUM_PAGES_TO_ACCESS = strtoul(argv[2], NULL, 10);
    

    pin_to_core(0);  // Pin to the first core

    int retval;
    retval=PAPI_library_init(PAPI_VER_CURRENT);
    if (retval!=PAPI_VER_CURRENT) {
    fprintf(stderr,"Error initializing PAPI! %s\n",
    PAPI_strerror(retval));
    return 0;
    }
    int eventset=PAPI_NULL;

    retval=PAPI_create_eventset(&eventset);
    if (retval!=PAPI_OK) {
    fprintf(stderr,"Error creating eventset! %s\n",
    PAPI_strerror(retval));
    }

    retval=PAPI_add_named_event(eventset,"PAPI_TOT_CYC");
    if (retval!=PAPI_OK) {
	fprintf(stderr,"Error adding PAPI_TOT_CYC: %s\n",
	PAPI_strerror(retval));
    }

    long long count;
    // 4KB pages
    char *pages = (PAGE_SIZE != 2097152) ? allocate_pages(NUM_PAGES_TO_ACCESS, PAGE_SIZE, 0) : allocate_pages(NUM_PAGES_TO_ACCESS, PAGE_SIZE, 1);
    uint64_t total_time = 0;
    PAPI_reset(eventset);
    retval=PAPI_start(eventset);
    if (retval!=PAPI_OK) {
	fprintf(stderr,"Error starting : %s\n",
		PAPI_strerror(retval));
    }
    for (int i = 0; i < NUM_RUNS; i++) {
        total_time += access_and_time(pages, PAGE_SIZE,NUM_PAGES_TO_ACCESS);
    }
    retval=PAPI_stop(eventset,&count);
    if (retval!=PAPI_OK) {
    fprintf(stderr,"Error stopping:  %s\n",
                        PAPI_strerror(retval));
    }
    else {
      printf("Measured %lld cycles\n",count);
    }
    //sleep(1); check for hugepages alloc
    printf("Average time per page access: %f cycles\n", (double)total_time / (NUM_PAGES_TO_ACCESS * NUM_RUNS));
    munmap(pages, NUM_PAGES_TO_ACCESS * PAGE_SIZE);

    // 2MB huge pages
    // int num_hugepages = available_hugepages();
    // if (num_hugepages < HUGE_PAGE_ALLOCATION) {
    //     printf("Not enough huge pages available. Needed: %d, Available: %d\n", HUGE_PAGE_ALLOCATION, num_hugepages);
    //     return 1;
    // }

    // char *pages_2mb = allocate_pages(HUGE_PAGE_ALLOCATION, HUGE_PAGE_SIZE, 1);
    // uint64_t total_time_2mb = 0;
    // for (int i = 0; i < NUM_RUNS; i++) {
    //     total_time_2mb += access_and_time(pages_2mb, HUGE_PAGE_SIZE);
    // }
    // printf("Average time per 2MB huge page access: %f cycles\n", (double)total_time_2mb / (HUGE_PAGE_ALLOCATION * NUM_RUNS));
    // munmap(pages_2mb, HUGE_PAGE_ALLOCATION * (1ll)* HUGE_PAGE_SIZE);

    return 0;
}
