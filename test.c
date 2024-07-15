#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <linux/mman.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <sched.h>

#define PAGE_SIZE 4096
#define HUGE_PAGE_SIZE 2097152
#define BASE ((void *)0x13370000000L)
#define TLLINE(x) ((x) >> 21)
#define TLLINE2(x) ((x) >> 12)

//TODO: dynamic number of pages and number of accesses, figure out pattern for L2 miss.

static inline uintptr_t TDL1(uintptr_t va) {
    return TLLINE(va) & 0x0f;
}

static inline uintptr_t TSL2(uintptr_t va) {
    uintptr_t p = TLLINE2(va);
    return (p ^ (p >> 7)) & 0x7f;
}

static inline uintptr_t TSL2x(uintptr_t va)
{
	uintptr_t p = TLLINE(va);
	return (p ^ (p >> 7)) & 0x7f;
}

static inline uint64_t rdtsc() {
    unsigned int lo, hi;
    __asm__ __volatile__ (
        "cpuid\n"
        "rdtsc\n"
        "mov %%edx, %0\n"
        "mov %%eax, %1\n" : "=r" (hi), "=r" (lo) ::
        "%rax", "%rbx", "%rcx", "%rdx"
    );
    return ((uint64_t)hi << 32) | lo;
}

static void *setup_default()
{
    void *mapped_address;

    mapped_address = mmap(BASE, (65*PAGE_SIZE), PROT_READ|PROT_WRITE, MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS|MAP_POPULATE, -1, 0);

    // Print the mapped address and size
    if (mapped_address != MAP_FAILED) {
        // printf("Mapped address: %p\n", mapped_address);
        // printf("Size: %ld bytes\n", (long) (5*PAGE_SIZE));
    } else {
        perror("Failed to mmap"); // Print why the mapping failed
    }

    return mapped_address;
}

static void *setup_huge()
{
    void *mapped_address;

    mapped_address = mmap(BASE, (33*HUGE_PAGE_SIZE), PROT_READ|PROT_WRITE, MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB|MAP_HUGE_2MB|MAP_POPULATE, -1, 0);

    // Print the mapped address and size
    if (mapped_address != MAP_FAILED) {
        // printf("Mapped address: %p\n", mapped_address);
        // printf("Size: %ld bytes\n", (long) (5*HUGE_PAGE_SIZE));
    } else {
        perror("Failed to mmap"); // Print why the mapping failed
    }

    return mapped_address;
}

void access_and_time(char *pages, size_t page_size, size_t num_pages,int num_accesses) {
    // uint64_t start = rdtsc();
for(int i =0;i<num_accesses;i++){
    for (size_t j = 0; j < num_pages; j++) {
        char *addr = pages + j * page_size;
        volatile char touch = *addr;
        (void)touch;
        //printf("touched address *%p\n",(void *)addr);
        
    }
    // uint64_t end = rdtsc();
    // return end - start;
    }
}

void measure_tlb(char *memory, size_t page_size, int num_pages, const char *label,int num_accesses) {
    uint64_t start_l1 = 0 , end_l1 = 0 ,start_l2 = 0, end_l2 = 0;
    uint64_t total_l1_time = 0 , total_l2_time = 0;
    volatile char *addr_l2 = (memory+num_pages*page_size);

    // Measure access time for L1 DTLB
    start_l1 = rdtsc();
    access_and_time(memory, page_size, num_pages, num_accesses);
    end_l1 = rdtsc();
    total_l1_time = end_l1 - start_l1;

    // Measure access time for L2 DTLB
    start_l2 = rdtsc();
    volatile char l2_touch = *addr_l2;
    end_l2 = rdtsc();
    //printf("touched L2 %p\n",(void *)addr_l2);
    (void)l2_touch; 
    total_l2_time = end_l2 - start_l2;
    printf("%s\n", label);
    printf("Average access time for L1 DTLB: %" PRIu64 " cycles\n", (total_l1_time/num_accesses)/num_pages);
    printf("Average access time for L2 DTLB: %" PRIu64 " cycles\n", total_l2_time );
    printf("\n");
}

void pin_to_core(int core) {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(core, &mask);

    if (sched_setaffinity(0, sizeof(mask), &mask) == -1) {
        perror("Could not set CPU affinity");
        exit(1);
    }
}
int main(int argc, char *argv[]) {
    pin_to_core(0);  // Pin to the first core
    void *default_page_buffer;
    void *huge_page_buffer;
     if (argc != 2) {
        fprintf(stderr, "Usage: %s number of accesses\n", argv[0]);
        return 1;
    }
    
    int num_accesses = atoi(argv[1]);

    if ((default_page_buffer = setup_default()) == MAP_FAILED) {
			fprintf(stderr, "###Error in mmap for default pages: %s\n", strerror(errno));
			perror("Error setting up default page buffer");
			return 2;
		}

    // Measure TLB access times for 4KB pages
    measure_tlb(default_page_buffer, PAGE_SIZE, 64, "TLB Access Times for 4KB Pages",num_accesses);
    munmap(default_page_buffer, PAGE_SIZE);
    
    if ((huge_page_buffer = setup_huge()) == MAP_FAILED) {
			fprintf(stderr, "###Error in mmap for huge pages: %s\n", strerror(errno));
			perror("Error setting up huge page buffer");
			return 2;
		}    

    // Measure TLB access times for 2MB pages
    measure_tlb(huge_page_buffer, HUGE_PAGE_SIZE, 32, "TLB Access Times for 2MB Pages",num_accesses);
    munmap(huge_page_buffer, HUGE_PAGE_SIZE);

    return 0;
}