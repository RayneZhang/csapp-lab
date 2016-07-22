/*name: Zhang Lei
 *loginID: 5140219237
 */

/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, t, n, tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
	if (M == 32) {
		for(i=0;i<4;i++)
		for(j=0;j<4;j++){
			if(i==j){
				for(t=8*i;t<8*i+8;t++){
					for(n=8*j;n<8*j+8;n++){
						if(t!=n) 
					  	  B[n][t]=A[t][n];
					}
					B[t][t]=A[t][t];
				}
			}
			else{
				for(t=8*i;t<8*i+8;t++)
				for(n=8*j;n<8*j+8;n++){
					if(t!=n) 
					   B[n][t]=A[t][n];
				}
			}
		}
	}
	else if (M == 64) {
		for (i = 0; i<64; i += 8)
			for (j = 0; j<64; j += 8)
			{
				for (t = j; t<j + 4; t++)
				{
					tmp0 = A[t][i];
					tmp1 = A[t][i + 1];
					tmp2 = A[t][i + 2];
					tmp3 = A[t][i + 3];
					tmp4 = A[t][i + 4];
					tmp5 = A[t][i + 5];
					tmp6 = A[t][i + 6];
					tmp7 = A[t][i + 7];
					B[i][t] = tmp0;
					B[i][t + 4] = tmp4;
					B[i + 1][t] = tmp1;
					B[i + 1][t + 4] = tmp5;
					B[i + 2][t] = tmp2;
					B[i + 2][t + 4] = tmp6;
					B[i + 3][t] = tmp3;
					B[i + 3][t + 4] = tmp7;
				}
				for (t = i; t<i + 4; t++)
				{
					tmp0 = A[j + 4][t];
					tmp1 = A[j + 5][t];
					tmp2 = A[j + 6][t];
					tmp3 = A[j + 7][t];
					tmp4 = B[t][j + 4];
					tmp5 = B[t][j + 5];
					tmp6 = B[t][j + 6];
					tmp7 = B[t][j + 7];
					B[t][j + 4] = tmp0;
					B[t][j + 5] = tmp1;
					B[t][j + 6] = tmp2;
					B[t][j + 7] = tmp3;
					B[t + 4][j] = tmp4;
					B[t + 4][j + 1] = tmp5;
					B[t + 4][j + 2] = tmp6;
					B[t + 4][j + 3] = tmp7;
				}
				for (t = i + 4; t<i + 8; t++)
				{
					tmp0 = A[j + 4][t];
					tmp1 = A[j + 5][t];
					tmp2 = A[j + 6][t];
					tmp3 = A[j + 7][t];
					B[t][j + 4] = tmp0;
					B[t][j + 5] = tmp1;
					B[t][j + 6] = tmp2;
					B[t][j + 7] = tmp3;
				}
			}
	}
	else if (M == 61)
	{
		for(i=0;i<N;i+=16)
		for(j=0;j<M;j+=16)
		for(t=i;t<i+16 && t<N;t++)
		for(n=j;n<j+16 && n<M;n++){
			B[n][t]=A[t][n];
		}
	}

}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

