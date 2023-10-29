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
#define NUM_ACCESSES 1000000
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

    mapped_address = mmap(BASE, (5*PAGE_SIZE), PROT_READ|PROT_WRITE, MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS|MAP_POPULATE, -1, 0);

    // Print the mapped address and size
    if (mapped_address != MAP_FAILED) {
        printf("Mapped address: %p\n", mapped_address);
        printf("Size: %ld bytes\n", (long) (5*PAGE_SIZE));
    } else {
        perror("Failed to mmap"); // Print why the mapping failed
    }

    return mapped_address;
}

static void *setup_huge()
{
    void *mapped_address;

    mapped_address = mmap((BASE+HUGE_PAGE_SIZE), (5*HUGE_PAGE_SIZE), PROT_READ|PROT_WRITE, MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB|MAP_HUGE_2MB|MAP_POPULATE, -1, 0);

    // Print the mapped address and size
    if (mapped_address != MAP_FAILED) {
        printf("Mapped address: %p\n", mapped_address);
        printf("Size: %ld bytes\n", (long) (5*HUGE_PAGE_SIZE));
    } else {
        perror("Failed to mmap"); // Print why the mapping failed
    }

    return mapped_address;
}

void access_and_time(char *pages, size_t page_size, size_t num_pages) {
    // uint64_t start = rdtsc();
for(int i =0;i<NUM_ACCESSES;i++){
    for (size_t i = 0; i < num_pages; i++) {
        char *addr = pages + i * page_size;
        //printf("touching address *%p\n",addr);
        *addr = *addr + 1;
    }
    // uint64_t end = rdtsc();
    // return end - start;
}
}

void measure_tlb(char *memory, size_t page_size, int num_pages, const char *label) {
    uint64_t start_l1, end_l1;
    uint64_t total_l1_time = 0;

    // Measure access time for L1 DTLB
    start_l1 = rdtsc();
    access_and_time(memory, page_size, num_pages);
    end_l1 = rdtsc();
    total_l1_time = end_l1 - start_l1;

    // Measure access time for L2 DTLB
    // start_l2 = rdtsc();
    // access_and_time(memory, page_size, num_pages);
    // end_l2 = rdtsc();
    // total_l2_time = end_l2 - start_l2;

    printf("%s\n", label);
    printf("Average access time for L1 DTLB: %" PRIu64 " cycles\n", total_l1_time / NUM_ACCESSES);
    //printf("Average access time for L2 DTLB: %" PRIu64 " cycles\n", total_l2_time / NUM_ACCESSES);
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
int main() {
    pin_to_core(0);  // Pin to the first core
    void *default_page_buffer;
    void *huge_page_buffer;

    if ((default_page_buffer = setup_default()) == MAP_FAILED) {
			fprintf(stderr, "###Error in mmap for default pages: %s\n", strerror(errno));
			perror("Error setting up default page buffer");
			return 2;
		}

    if ((huge_page_buffer = setup_huge()) == MAP_FAILED) {
			fprintf(stderr, "###Error in mmap for huge pages: %s\n", strerror(errno));
			perror("Error setting up huge page buffer");
			return 2;
		}    

    // Measure TLB access times for 4KB pages
    measure_tlb(default_page_buffer, PAGE_SIZE, 4, "TLB L1 Access Times for 4KB Pages");

    // Measure TLB access times for 2MB pages
    measure_tlb(huge_page_buffer, HUGE_PAGE_SIZE, 4, "TLB L1 Access Times for 2MB Pages");

    // Free allocated memory
    munmap(default_page_buffer, PAGE_SIZE);
    munmap(huge_page_buffer, HUGE_PAGE_SIZE);

    return 0;
}

