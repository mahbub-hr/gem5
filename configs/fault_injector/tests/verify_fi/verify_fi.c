#include <stdint.h>
#include <stdio.h>

int
main()
{
    // A global variable we will target
    volatile uint64_t target_var = 0xDEADBEEF;

    // 2. BUSY WAIT LOOP
    // We need to waste time here to ensure the variable sits in the cache
    // while the simulation tick counter increases.
    // This gives us a window to inject the fault.
    volatile int i;
    for (i = 0; i < 100; i++) {
        // Just wasting ticks...
    }

    if (target_var != 0xDEADBEEF) {
        printf("SUCCESS: Fault Detected! (0x%lX != 0xDEADBEEF)\n", target_var);
    } else {
        printf("FAILURE: Value remained correct.\n");
    }

    return 0;
}
