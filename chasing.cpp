#include <iostream>
#include <cstdint> // For uintptr_t
#include <unistd.h>

int main() {
    // Step 1: Create an array of pointers
    const int SIZE = 5;
    uintptr_t *addresses[SIZE];

    // Step 2: Link each pointer to the next
    for (int i = 0; i < SIZE; ++i) {
        addresses[i] = (uintptr_t *)(i < SIZE - 1 ? &addresses[i + 1] : &addresses[0]);
        printf(" i is %d\n", i);
        printf("value in address[i] is : %d \n",addresses[i]);
        printf("value of address[i] is : %x \n",&addresses[i]);
        }

    // Step 3: Start at the first 'address'
    uintptr_t *b = addresses[0];
    sleep(10);
    // Step 4: Traverse the 'addresses'
    for (int i = 0; i < 10; ++i) { // Loop more times than SIZE to demonstrate looping
        std::cout << "Current index: " << (b - (uintptr_t *)addresses) << std::endl;
        b = (uintptr_t *)*b; // Move to the next 'address'
    }

    return 0;
}
