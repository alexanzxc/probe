#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <iostream>
#include <string>
#include <papi.h>
#include <assert.h>

#define PAGE_SIZE 4096
#define LINE_SIZE 64
// #define loopCount 1000000000
// #define loopCount 10000000
#define loopCount 100000

uint64_t start_time = 0;
uint64_t stop_time = 0;

static inline uint64_t rdtsc()
{
	uint32_t lo, hi;
	__asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
	return (uint64_t)hi << 32 | lo;
}

int retval;
int EventSet;
char* EventName1[] = {"DTLB_LOAD_MISSES:WALK_DURATION", "L1D:REPLACEMENT", "PAPI_L1_DCM"};
char *EventName2[] = {"L2_LINES_IN:ANY", "LLC_REFERENCES", "LLC_MISSES", "DTLB_LOAD_MISSES:MISS_CAUSES_A_WALK"};
char **EventName;
int NumEvent;
long_long global_values[4];

void init_counters(int set)
{
// #ifdef PAPI_COUNTERS
	int native;
	
	EventSet = PAPI_NULL;

	if (set == 0)
	{
		NumEvent = 3;
		EventName = EventName1;
	}
	else if (set == 1)
	{
		NumEvent = 4;
		EventName = EventName2;
	}

	for (int i = 0; i < 4; i++)
	{
		global_values[i] = 0;
	}

	if ((retval = PAPI_library_init(PAPI_VER_CURRENT)) != PAPI_VER_CURRENT)
		assert(0);
	if ((retval = PAPI_create_eventset(&EventSet)) != PAPI_OK)
		assert(0);

	for (int i = 0; i < NumEvent; i++)
	{
		retval = PAPI_event_name_to_code(EventName[i], &native);
		//printf("set: %d -- NumEvent: %d ###### retval = %d\n", set, i, retval);
		if (retval != PAPI_OK)
			assert(0);
		if ((retval = PAPI_add_event(EventSet, native)) != PAPI_OK)
			assert(0);
		//printf("native: 0x%x\n", native);
	}
// #endif
}

void start_counters()
{
	if ((retval = PAPI_start(EventSet)) != PAPI_OK)
		assert(0);
}

void stop_counters()
{
	if ((retval = PAPI_stop(EventSet, global_values)) != PAPI_OK)
		assert(0);
}

void fini_counters(int set)
{
	for (int i = 0; i < NumEvent; i++)
		printf("%s %lli\n", EventName[i], global_values[i]);
	if (set == 0)
	{
		if (global_values[2] != 0)
		{
			// printf("\n\navg_tlb_miss: %lli\n\n", global_values[1]/global_values[2]);
			printf("avg_tlb_miss: %lli\n", global_values[1] / loopCount);
		}
		else
		{
			printf("avg_tlb_miss: 0\n");
		}
	}
	PAPI_destroy_eventset(&EventSet);
}

int main(int argc, char *argv[])
{

	uint64_t numPages = 1;
	uint64_t numAccesses = 1;
	uint64_t pageStride = 1;
	uint64_t accessOffset = 1;

	int c;
	while ((c = getopt(argc, argv, ":p:a:s:o:")) != EOF)
	{
		switch (c)
		{
		case 'p':
			numPages = atoi(optarg);
			break;
		case 'a':
			numAccesses = atoi(optarg);
			break;
		case 's':
			pageStride = atoi(optarg);
			break;
		case 'o':
			accessOffset = atoi(optarg);
			break;
		case ':':
			printf("Missing option.\n");
			exit(1);
			break;
		}
	}

	printf("numPages: %lu\n", numPages);
	printf("numAccesses: %lu\n", numAccesses);
	printf("pageStride: %lu\n", pageStride);
	printf("accessOffset: %lu\n", accessOffset);

	// Create array of pointers to the allocated pages
	uintptr_t **pages = new uintptr_t *[numPages];
    //buffer alloc
	char *buf = new char[numPages * PAGE_SIZE];
	if (buf == NULL)
	{
		printf("could not allocate memory..\n");
		exit(1);
	}
    //buffer split according to stride
	for (uint64_t i = 0; i < (numPages * PAGE_SIZE); i += (PAGE_SIZE * pageStride))
	{   
        //printf("i is %ld, and its size is %zu\n",i, sizeof(i));
		buf[i] = i;
	}
	printf("buf: %p\n", buf);
	printf("size: 0x%lx\n", numPages * PAGE_SIZE);
	

	for (uint64_t i = 0; i < numPages; i++)
	{
		pages[i] = (uintptr_t *)(buf + i * pageStride * PAGE_SIZE);
		//printf("pages[%lu] = %p\n",i,pages[i]);
	}

	// Cache line size is considered in units of pointer size
	uint64_t numOffsets = PAGE_SIZE / LINE_SIZE;
	// Create array of offsets in a page
	uint64_t *offsets = new uint64_t[numOffsets];
	for (uint64_t i = 0; i < numOffsets; i++)
	{
		offsets[i] = i * LINE_SIZE;
		//printf("offset_size[%lu] = 0x%lx\n", i, offsets[i]);
	}

	// Create the pointer walk from pointers and offsets
	uintptr_t *start = pages[0];
	start += accessOffset;

	uintptr_t **ptr = (uintptr_t **)start;
	for (uint64_t i = 0; i < numAccesses; i++)
	{
		// uintptr_t *next = pages[i % numPages];
		// next = ptr[i];
		pages[i] += accessOffset;
		//why commented out? pages will overflow
		uintptr_t *next = pages[i % numPages];
		// uintptr_t *next = pages[i];
		(*ptr) = next;
		ptr = (uintptr_t **)next;
	}

	/*
	for (uint64_t i = 0; i < numPages; i++) {
		//pages[i] = (uintptr_t *) (buf + pageStride * PAGE_SIZE);
		//printf("pages[%lu] = %p\n",i,pages[i]);
	}
	*/

	// Wrap the pointer walk
	(*ptr) = start;
	for (uint64_t i = 0; i < numAccesses; i++)
	{
		ptr = (uintptr_t **)(*ptr);
	}

	//(*ptr) = start;
	printf("benchmark starts...\n\n");
	int papi_sets = 2;

	for (int set = 0; set < papi_sets; set++)
	{
		// init_counters(EventName);
		init_counters(set);
		start_counters();
		start_time = rdtsc();

		for (uint64_t i = 0; i < loopCount; i++)
		{
			ptr = (uintptr_t **)(*ptr);
		}

		stop_time = rdtsc();
		stop_counters();
		fini_counters(set);
	}
	printf("\nbenchmark finishes...\n\n");

	/*
	printf("total_cycles: %lli\n", stop_time - start_time);
	printf("avg_access_time: %lli\n", (stop_time - start_time)/(3*loopCount));
	*/
}
