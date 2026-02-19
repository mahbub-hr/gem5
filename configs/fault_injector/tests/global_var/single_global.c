#include<stdio.h>

volatile int a = 0xdeadbeef, b = 0xcabfed, c = 0x12345678, d = 0x87654321; // A single global variable to test fault injection
volatile int a_shadow = 0xdeadbeef; // Shadow variable for error detection




void error(){
    printf("Error: Mismatch between a and a_shadow\n");
    exit(-1);
}

void main() {
    if(a != a_shadow) {
        error();
    }



    printf("Global variable value: %d\n", a);
}