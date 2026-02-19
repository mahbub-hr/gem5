/**
 * @copyright (c) 2024, Mahbub Hossain
 * All rights reserved.
 * @author Mahbub Hossain
 * @date 2026-02-18
 * @brief This code demonstrates how cache partitioning can reduce
 *  silent data corruption in a matrix multiplication program.
 */


#include<stdio.h>
#include<stdlib.h>

#define N 16

// Each matrix is placed in its own named section and aligned to a page boundary
double A[N][N] __attribute__((section(".data_primary")));
double B[N][N] __attribute__((section(".data_primary")));
double C[N][N] __attribute__((section(".data_primary")));

double A_shadow[N][N] __attribute__((section(".data_shadow")));
double B_shadow[N][N] __attribute__((section(".data_shadow")));
double C_shadow[N][N] __attribute__((section(".data_shadow")));

void error(){
    printf("Error: Mismatch between C and C_shadow\n");
    exit(-1);
}

void init_array() {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            A[i][j] = i + j;
            B[i][j] = i - j;
            C[i][j] = 0;
            A_shadow[i][j] = i + j;
            B_shadow[i][j] = i - j;
            C_shadow[i][j] = 0;
        }
    }
}

void matrix_mul() {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            for (int k = 0; k < N; k++) {
                if(A[i][k] != A_shadow[i][k] || B[k][j] != B_shadow[k][j]) {
                    error();
                }
                // Todo: performance improvement with CLFLUSH()
                C[i][j] += A[i][k] * B[k][j];
                C_shadow[i][j] += A_shadow[i][k] * B_shadow[k][j];
            }
        }
    }
}

void print_result() {
    // Print the result
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            if(C[i][j] != C_shadow[i][j]) {
                error();
            }
            printf("%f ", C[i][j]);
        }
        printf("\n");
    }
}

int main() {
    init_array();
    matrix_mul();
    print_result();
    return 0;
}