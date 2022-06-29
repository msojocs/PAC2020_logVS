#include <iostream>
#include <immintrin.h>
#include <stdlib.h>

int main(void){
    __m256 a = _mm256_set_ps(8.0, 7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0);
    __m256 b = _mm256_set_ps(18.0, 17.0, 16.0, 15.0, 14.0, 13.0, 12.0, 11.0);
    __m256 c = _mm256_add_ps(a, b);
    float * fin = (float*)aligned_alloc(64, sizeof(float)*8);
    _mm256_store_ps(fin, c);
    for(int i=0; i < 8; i++)
        std::cout << fin[i] << std::endl;
    return 0;
}