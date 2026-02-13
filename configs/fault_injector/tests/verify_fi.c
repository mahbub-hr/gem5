#include <stdint.h>
#include <stdio.h>

// A global variable we will target
volatile uint64_t target_var = 0xDEADBEEF;

int
main()
{
    printf("--- Start of Verification ---\n");
    printf("Original Value: 0x%lX\n", target_var);
    printf("Address (Virtual): %p\n", &target_var);

    // 2. BUSY WAIT LOOP
    // We need to waste time here to ensure the variable sits in the cache
    // while the simulation tick counter increases.
    // This gives us a window to inject the fault.
    volatile int i;
    for (i = 0; i < 100000; i++) {
        // Just wasting ticks...
    }

    // 3. READ BACK
    // If fault injection worked, this value will be different
    uint64_t read_back = target_var;

    printf("Final Value:    0x%lX\n", read_back);

    if (read_back != 0xDEADBEEF) {
        printf("SUCCESS: Fault Detected! (0x%lX != 0xDEADBEEF)\n", read_back);
    } else {
        printf("FAILURE: Value remained correct.\n");
    }

    return 0;
}
