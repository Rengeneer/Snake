// Минимальный HIP-пример для проверки связки clang+HIP+lld-link на Windows
#include <hip/hip_runtime.h>
#include <stdio.h>

__global__ void test_kernel(int* data)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx == 0)
        data[0] = 42;
}

int main()
{
    hipError_t err;
    int* d_data = nullptr;
    int h_data = 0;

    err = hipMalloc(&d_data, sizeof(int));
    if (err != hipSuccess) {
        printf("hipMalloc failed: %s\n", hipGetErrorString(err));
        return 1;
    }

    err = hipMemset(d_data, 0, sizeof(int));
    if (err != hipSuccess) {
        printf("hipMemset failed: %s\n", hipGetErrorString(err));
        return 1;
    }

    hipLaunchKernelGGL(test_kernel,
                       dim3(1),   // grid
                       dim3(1),   // block
                       0, 0,      // shared mem, stream
                       d_data);

    err = hipDeviceSynchronize();
    if (err != hipSuccess) {
        printf("hipDeviceSynchronize failed: %s\n", hipGetErrorString(err));
        return 1;
    }

    err = hipMemcpy(&h_data, d_data, sizeof(int), hipMemcpyDeviceToHost);
    if (err != hipSuccess) {
        printf("hipMemcpy failed: %s\n", hipGetErrorString(err));
        return 1;
    }

    printf("HIP minimal test, value = %d\n", h_data);

    hipFree(d_data);
    return 0;
}


