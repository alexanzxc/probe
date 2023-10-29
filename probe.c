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

#define PAGE_SIZE 4096
#define HUGE_PAGE_SIZE (2 * 1024 * 1024)
#define NUM_RUNS 10
#define L1_TLB_SIZE_4KB 64
#define L2_TLB_SIZE 1536  // Assuming both 4KB and 2MB share the same L2 TLB size ?
#define HUGE_PAGE_ALLOCATION 1536
#define BASE ((void *)0x133800000000ULL)
#define MAP_HUGE_2MB    (21 << MAP_HUGE_SHIFT)

static inline uint64_t rdtsc() {
    uint32_t lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
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
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    if (hugepage) {
        flags |= MAP_HUGETLB | MAP_HUGE_2MB;
    }
    char *pages = mmap(BASE, num_pages * page_size, PROT_READ | PROT_WRITE, flags, -1, 0);
    if (pages == MAP_FAILED) {
        perror("Failed to allocate pages");
        exit(1);
    }
    return pages;
}

void shuffle_array(int *array, size_t n) {
    if (n <= 1) return;
    srand(time(NULL));
    for (size_t i = n - 1; i > 0; i--) {
        size_t j = rand() % (i + 1);
        int temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

uint64_t access_and_time(char *pages, size_t page_size) {
    int indices[L2_TLB_SIZE];
    for (int i = 0; i < L2_TLB_SIZE; i++) {
        indices[i] = i;
    }
    shuffle_array(indices, L2_TLB_SIZE);
    uint64_t start = rdtsc();
    for (int i = 0; i < HUGE_PAGE_ALLOCATION; i++) {
        pages[indices[i] * page_size]++;
    }
    uint64_t end = rdtsc();

    return end - start;
}

int main() {
    pin_to_core(0);  // Pin to the first core

    // 4KB pages
    char *pages_4kb = allocate_pages(L2_TLB_SIZE, PAGE_SIZE, 0);
    uint64_t total_time_4kb = 0;
    for (int i = 0; i < NUM_RUNS; i++) {
        total_time_4kb += access_and_time(pages_4kb, PAGE_SIZE);
    }
    printf("Average time per 4KB page access: %f cycles\n", (double)total_time_4kb / (HUGE_PAGE_ALLOCATION * NUM_RUNS));
    munmap(pages_4kb, L2_TLB_SIZE * PAGE_SIZE);

    // 2MB huge pages
    int num_hugepages = available_hugepages();
    if (num_hugepages < HUGE_PAGE_ALLOCATION) {
        printf("Not enough huge pages available. Needed: %d, Available: %d\n", HUGE_PAGE_ALLOCATION, num_hugepages);
        return 1;
    }

    char *pages_2mb = allocate_pages(HUGE_PAGE_ALLOCATION, HUGE_PAGE_SIZE, 1);
    uint64_t total_time_2mb = 0;
    for (int i = 0; i < NUM_RUNS; i++) {
        total_time_2mb += access_and_time(pages_2mb, HUGE_PAGE_SIZE);
    }
    printf("Average time per 2MB huge page access: %f cycles\n", (double)total_time_2mb / (HUGE_PAGE_ALLOCATION * NUM_RUNS));
    munmap(pages_2mb, HUGE_PAGE_ALLOCATION * (1ll)* HUGE_PAGE_SIZE);

    return 0;
}
