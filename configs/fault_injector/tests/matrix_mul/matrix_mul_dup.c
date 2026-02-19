#include<stdio.h>
#include<stdlib.h>

#define N 16

// Todo: add checkpoint for speedup.
// Each matrix is placed in its own named section and aligned to a page boundary
int A[N][N];
int B[N][N];
int C[N][N];

int A_shadow[N][N];
int B_shadow[N][N];
int C_shadow[N][N];

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
            printf("%d ", C[i][j]);
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