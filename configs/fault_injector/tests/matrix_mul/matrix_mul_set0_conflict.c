/**
 * @author Mahbub Hossain
 * @date 2024-06-01
 * @brief This code demonstrates how cache block conflicts can lead to 
 * an increase in silent data corruption in a matrix multiplication program.
 */


#include<stdio.h>
#include<stdlib.h>

#define N 16

// Each matrix is placed in its own named section and aligned to a page boundary
double A[N][N] __attribute__((section(".data_primary"), aligned(4096)));
double B[N][N] __attribute__((section(".data_primary"), aligned(4096)));
double C[N][N] __attribute__((section(".data_primary"), aligned(4096)));

double A_shadow[N][N] __attribute__((section(".data_shadow"), aligned(4096)));
double B_shadow[N][N] __attribute__((section(".data_shadow"), aligned(4096)));
double C_shadow[N][N] __attribute__((section(".data_shadow"), aligned(4096)));

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