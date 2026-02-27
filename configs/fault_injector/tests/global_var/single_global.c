#include<stdio.h>

volatile int a = 0xdeadbeef, b = 0xcabfed, c = 0x12345678, d = 0x87654321; // A single global variable to test fault injection
volatile int a_shadow = 0xdeadbeef, b_shadow = 0xcabfed, c_shadow = 0x12345678, d_shadow = 0x87654321; // Shadow variable for error detection

void error(){
    printf("Error: Mismatch between a and a_shadow\n");
    exit(-1);
}

void main() {
    if(a != a_shadow) {
        error();
    }
    if(b != b_shadow) {
        error();
    }
    if(c != c_shadow) {
        error();
    }
    if(d != d_shadow) {
        error();
    }

    for(int i = 0; i < 10; i++) {
        printf("Iteration %d: a = 0x%x\n", i, a);
        if(a != a_shadow) {
            error();
        }
        a += 1; // Increment the global variable to create a changing state 
    }

    printf("Global variable value: %d\n", a);
}