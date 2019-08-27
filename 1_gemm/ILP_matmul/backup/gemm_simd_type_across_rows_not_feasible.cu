#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sstream>
#include <string>
#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <utility>
//#include "simd_scalar.h"
//#include "impl/Kokkos_Timer.hpp"
#include "gpu.h"


/*
#define X 1600
#define Y 1600
*/
int X, Y, verification;

//#define BLOCK_SIZE 16
//#define TILE_WIDTH BLOCK_SIZE

#define GETCOORDS(row, col) (row) * (Y) + (col)
#define CEIL(x,y) (((x)-1) / (y)) + 1

typedef double value_type;

void verify(value_type* M, value_type* N, value_type* P, int width){
#pragma omp parallel for collapse(2)
    for(int i=0; i < width; i++){
        for(int j=0; j < width; j++){
           value_type sum = 0;
           for(int k = 0; k < width; k++){
               sum += M[ GETCOORDS(i,k) ] * N[ GETCOORDS(k,j) ];
           }
           if(std::fabs(P[ GETCOORDS(i,j) ] - sum) > 0.1){
               std::cout << "error!\n";
	       std::cout << "caution: boundary conditions are not handled in the kernel. hence it does not work if X or Y (matrix size) are not power of 2.\n";
	       exit(1);
  	   }
        }
    } 
}

template<int BLOCK_SIZE, int TILE_WIDTH=BLOCK_SIZE>
__global__ void MatrixMulKernel(value_type* Md, value_type* Nd, value_type* Pd, int width){

    int bx = blockIdx.x, by = blockIdx.y, tx = threadIdx.x, ty = threadIdx.y;

    //pad 1 cell to avoid bank conflicts.. improving performance by almost 150 gflops
    __shared__ value_type Mds[TILE_WIDTH][TILE_WIDTH+1];
    __shared__ value_type Nds[TILE_WIDTH][TILE_WIDTH+1];
    value_type res[ILP]; 

    int row = by * BLOCK_SIZE + ty;
    int col = bx * BLOCK_SIZE + tx;
    if( row >= width || col >= width ) return;

    constexpr int ilp = ILP;	//somehow ILP macro does not work for #pragma unroll. hence using constexpr
#pragma unroll (ilp)
	for(int i=0; i<ILP; i++)
		res[i]=0.0f;
		

    for(int p=0; p < width/TILE_WIDTH; p++){		//boundary conditions are not handled here. hence it does not work if X or Y (matrix size) are not power of 2.

#pragma unroll (ilp)
	for(int i=0; i<ILP; i++){
        	Mds[ty+ i*BLOCK_SIZE/ILP][tx] = Md[(row + i*BLOCK_SIZE/ILP) * width + (tx + p*TILE_WIDTH) ];
	        Nds[ty+ i*BLOCK_SIZE/ILP][tx] = Nd[(p*TILE_WIDTH + ty + i*BLOCK_SIZE/ILP)*width + col ];
	}
        __syncthreads();


        //dot product from shared memory
#pragma unroll (TILE_WIDTH)
        for(int k=0; k < TILE_WIDTH; k++){		//boundary conditions are not handled here. hence it does not work if X or Y (matrix size) are not power of 2.

#pragma unroll (ilp)
	for(int i=0; i<ILP; i++)
	     res[i] += Mds[ty + i*BLOCK_SIZE/ILP][k] * Nds[k][tx];

        }  
        __syncthreads();
    }
   
#pragma unroll (ilp)
	for(int i=0; i<ILP; i++)
    		Pd[(row + i*BLOCK_SIZE/ILP)*width + col] = res[i];
}

double execution_time=0.0;


void print_stats(int blocks, int BLOCK_SIZE){
    execution_time=execution_time/30.0/1000.0; //in seconds
    double gflops = ((double)(2.0*X*X*X))/1024.0/1024.0/1024.0 / execution_time;
    printf("success\nN\tBlocks\tThreadsPBlock\ttime(s)\tGFLOPS\n");
    printf("%d\t%dx%d\t%dx%d\t%f\t%f\n",X, blocks, blocks, BLOCK_SIZE, BLOCK_SIZE/ILP, execution_time, gflops);
    execution_time = 0.0;
}

template<int BLOCK_SIZE>
void inline MatrixMultDevice(value_type* M, value_type* N, value_type* P, int width){
    int size = width*width*sizeof(value_type);
    value_type *Md, *Nd, *Pd;

    dim3 dimBlock(BLOCK_SIZE, BLOCK_SIZE/ILP);
    //int numBlocks = (width + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int numBlocks = width / BLOCK_SIZE;
    dim3 dimGrid(numBlocks, numBlocks);
    
    //0. reserve memory on device
    cudaMalloc( (void**)&Md, size );
    cudaMalloc( (void**)&Nd, size );
    cudaMalloc( (void**)&Pd, size );

    for(int it=-3; it<30; it++){
	    //1. transfer M and N to device memory
	    cudaMemcpy(Md, M, size, cudaMemcpyHostToDevice);
	    cudaMemcpy(Nd, N, size, cudaMemcpyHostToDevice);

	    cudaMemset(Pd, 0, size);

	    //2. kernel invokation 
	    cudaEvent_t start,stop;
	    float elapsed_time;
	    cudaEventCreate(&start);
	    cudaEventCreate(&stop);
	    cudaEventRecord(start);
	    MatrixMulKernel<BLOCK_SIZE><<<dimGrid, dimBlock>>>(Md, Nd, Pd, width);
	    cudaEventRecord(stop);
	    cudaEventSynchronize(stop);
	    cudaEventElapsedTime(&elapsed_time,start, stop);
	    cudaEventDestroy (start);   
	    cudaEventDestroy (stop);

	    if(it > -1)
		execution_time += elapsed_time;
    }
    //3. copy P from device to host
    cudaMemcpy(P, Pd, size, cudaMemcpyDeviceToHost);
    cudaDeviceSynchronize();
    //4. Free Md, Nd, Pd
    cudaFree(Md);
    cudaFree(Nd);
    cudaFree(Pd);

    // M*N on host
    if(verification)
	verify(M, N, P, Y);

    print_stats(numBlocks, BLOCK_SIZE);

}

//*******************************************************************************************************************************************************************
//*******************************************************************************************************************************************************************
//*******************************************************************************************************************************************************************


typedef Vector<double> simd;

template<int BLOCK_SIZE, int TILE_WIDTH=BLOCK_SIZE>
__global__ void MatrixMulKernelSIMD(simd* Md, simd* Nd, simd* Pd, int width){

    int bx = blockIdx.x, by = blockIdx.y, tx = threadIdx.x, ty = threadIdx.y;

    //pad 1 cell to avoid bank conflicts.. improving performance by almost 150 gflops
    __shared__ simd Mds[TILE_WIDTH][TILE_WIDTH+1]; //for now keep as it is. Because even though simd process ILP num elements, length is still 1.
    __shared__ simd Nds[TILE_WIDTH][TILE_WIDTH+1];
    //simd res[ILP]; 
    simd res=0.0;

    int row = by * BLOCK_SIZE + ty;
    int col = bx * BLOCK_SIZE + tx;
    if( row >= width || col >= width ) return;

    constexpr int ilp = ILP;	//somehow ILP macro does not work for #pragma unroll. hence using constexpr
/*#pragma unroll (ilp)
	for(int i=0; i<ILP; i++)
		res[i]=0.0f;
*/		

    for(int p=0; p < width/TILE_WIDTH; p++){		//boundary conditions are not handled here. hence it does not work if X or Y (matrix size) are not power of 2.

/*#pragma unroll (ilp)
	for(int i=0; i<ILP; i++){
        	Mds[ty+ i*BLOCK_SIZE/ILP][tx] = Md[(row + i*BLOCK_SIZE/ILP) * width + (tx + p*TILE_WIDTH) ];
	        Nds[ty+ i*BLOCK_SIZE/ILP][tx] = Nd[(p*TILE_WIDTH + ty + i*BLOCK_SIZE/ILP)*width + col ];
	}
*/
	Mds[ty][tx] = Md[row * width + (p*TILE_WIDTH + tx)];
	Nds[ty][tx] = Nd[(p*TILE_WIDTH + ty) * width + col];
        __syncthreads();


        //dot product from shared memory
#pragma unroll (TILE_WIDTH)
        for(int k=0; k < TILE_WIDTH; k++){		//boundary conditions are not handled here. hence it does not work if X or Y (matrix size) are not power of 2.

/*#pragma unroll (ilp)
	for(int i=0; i<ILP; i++)
	     res[i] += Mds[ty + i*BLOCK_SIZE/ILP][k] * Nds[k][tx];
*/

	res += Mds[ty][k] * Nds[k][tx];

      }  
      __syncthreads();
    }
   
/*#pragma unroll (ilp)
	for(int i=0; i<ILP; i++)
    		Pd[(row + i*BLOCK_SIZE/ILP)*width + col] = res[i];
*/
	Pd[row*width + col] = res;
}




template<int BLOCK_SIZE>
void inline MatrixMultDeviceSIMD(value_type* M, value_type* N, value_type* P, int width){
    int size = width*width*sizeof(value_type);
    value_type *Md, *Nd, *Pd;
    simd *Ms, *Ns, *Ps;

    dim3 dimBlock(BLOCK_SIZE, BLOCK_SIZE/ILP);
    //int numBlocks = (width + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int numBlocks = width / BLOCK_SIZE;
    dim3 dimGrid(numBlocks, numBlocks);
    
    //0. reserve memory on device
    cudaMalloc( (void**)&Md, size );
    cudaMalloc( (void**)&Nd, size );
    cudaMalloc( (void**)&Pd, size );

    Ms = reinterpret_cast<simd *>(Md);
    Ns = reinterpret_cast<simd *>(Nd);
    Ps = reinterpret_cast<simd *>(Pd);

    for(int it=-3; it<30; it++){
	    //1. transfer M and N to device memory
	    cudaMemcpy(Md, M, size, cudaMemcpyHostToDevice);
	    cudaMemcpy(Nd, N, size, cudaMemcpyHostToDevice);

	    cudaMemset(Pd, 0, size);

	    //2. kernel invokation 
	    cudaEvent_t start,stop;
	    float elapsed_time;
	    cudaEventCreate(&start);
	    cudaEventCreate(&stop);
	    cudaEventRecord(start);
	    MatrixMulKernelSIMD<BLOCK_SIZE><<<dimGrid, dimBlock>>>(Ms, Ns, Ps, width);
	    cudaEventRecord(stop);
	    cudaEventSynchronize(stop);
	    cudaEventElapsedTime(&elapsed_time,start, stop);
	    cudaEventDestroy (start);   
	    cudaEventDestroy (stop);

	    if(it > -1)
		execution_time += elapsed_time;
    }
    //3. copy P from device to host
    cudaMemcpy(P, Pd, size, cudaMemcpyDeviceToHost);
    cudaDeviceSynchronize();
    //4. Free Md, Nd, Pd
    cudaFree(Md);
    cudaFree(Nd);
    cudaFree(Pd);

    // M*N on host
    if(verification)
	verify(M, N, P, Y);

    print_stats(numBlocks, BLOCK_SIZE);

}






//*******************************************************************************************************************************************************************
//*******************************************************************************************************************************************************************
//*******************************************************************************************************************************************************************






int main(int argc, char* argv[]){
    X = atoi(argv[1]);
    //Y = atoi(argv[2]);
    verification = atoi(argv[2]);
    Y = X;
    srand(time(NULL));
    value_type *M, *N, *P_d;

    // allocate M, N, P
    M = (value_type*)malloc( sizeof(value_type)*X*Y );
    N = (value_type*)malloc( sizeof(value_type)*X*Y );
    P_d = (value_type*)malloc( sizeof(value_type)*X*Y );

#pragma omp parallel for collapse(2)
    for( int i = 0; i < X; i++) {
        for( int j = 0; j < Y; j++) {
            M[ GETCOORDS(i,j) ] = (rand() % 5) / (float)(rand() % 5 + 0.1);
	    N[ GETCOORDS(i,j) ] = (rand() % 5) / (float)(rand() % 5 + 0.1);
        }
    }

    // M*N on device
    /*MatrixMultDevice<4>(M, N, P_d, Y);
    MatrixMultDevice<8>(M, N, P_d, Y);
    MatrixMultDevice<16>(M, N, P_d, Y);
    MatrixMultDevice<32>(M, N, P_d, Y);*/

    MatrixMultDeviceSIMD<16>(M, N, P_d, Y);


    // Free M, N, P
    free(M);
    free(N);
    free(P_d);
    cudaDeviceReset();
    return 0;
}