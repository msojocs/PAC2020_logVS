﻿#include <stdio.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <omp.h>
#include <immintrin.h>

#define DOU float
#define INT int
#define DOU_VEC __m256

using namespace std;

typedef chrono::high_resolution_clock Clock;

const int m = 1638400;	// DO NOT CHANGE!!
const int K = 100000;	// DO NOT CHANGE!!

DOU logDataVSPrior(const DOU*  val, const INT*  begin, const INT*  end, DOU_VEC*  res,
	const DOU_VEC disturb0, const INT thread_num);

int main(int argc, char *argv[])
{
	int length = m >> 3;//(m >> 4) + (((m & 15) + 15) >> 4);
	register int thread_num = omp_get_num_procs() - 1;

	int MM = (length << 3);
	//aligned_alloc(64,sizeof(DOU) * 6 * MM);
	DOU * val = (DOU*)aligned_alloc(64, sizeof(DOU) * 6 * MM );
	DOU * disturb = (DOU*)aligned_alloc(64, sizeof(DOU)*K);
	DOU * result = (DOU*)aligned_alloc(64, sizeof(DOU)*K);
	INT * begin = (INT*)aligned_alloc(64, sizeof(INT)*thread_num);
	INT * end = (INT*)aligned_alloc(64, sizeof(INT)*thread_num);
	DOU_VEC * res = (DOU_VEC*)aligned_alloc(64, sizeof(DOU_VEC)*thread_num);

	DOU dat0, dat1, pri0, pri1, ctf0, sigRcp0;

	register int tmp1, i;
	register int num_per_thread = length / thread_num;
	begin[0] = 0;

#pragma unroll(32)
	for (i = 0; i < thread_num - 1; i++)
	{
		tmp1 = begin[i] + num_per_thread;
		end[i] = tmp1;
		begin[i + 1] = tmp1;
	}
	end[thread_num - 1] = length;

	/***************************
	* Read data from input.dat
	* *************************/

	ifstream fin;

	fin.open("input.dat");
	if (!fin.is_open())
	{
		cout << "Error opening file input.dat" << endl;
		exit(1);
	}

	i = 0;
	int j;
	while (!fin.eof())
	{
		fin >> dat0 >> dat1 >> pri0 >> pri1 >> ctf0 >> sigRcp0;
		if (!(i & 7)) // i == 8   7的二进制0111
		/*
		a x 6
		= a x 2 x (2 + 1)
		= a x 2 x 2 + a x 2
		*/
			j = (i << 2) + (i << 1) - 1; // 乘以6 减1(下面有个j++)
		
		j++;

		/*
		为什么8个一组？
		_mm256_load_ps 等函数是一次处理8个元素
		*/
		val[j] = pri0;
		val[j + 8] = dat0;
		val[j + 16] = pri1;
		val[j + 24] = dat1;
		val[j + 32] = sigRcp0;
		val[j + 40] = ctf0;
		i++;
		if (i == m) break;
	}

	// 多出来的给0
	for (; i < MM; i++)
	{
		if (!(i & 7))
			j = (i << 2) + (i << 1) - 1;
		j++;
		val[j] = 0.00;
		val[j + 8] = 0.00;
		val[j + 16] = 0.00;
		val[j + 24] = 0.00;
		val[j + 32] = 0.00;
		val[j + 40] = 0.00;
	}

	fin.close();

	fin.open("K.dat");
	if (!fin.is_open())
	{
		cout << "Error opening file K.dat" << endl;
		exit(1);
	}
	i = 0;
	while (!fin.eof())
	{
		fin >> disturb[i];
		i++;
		if (i == K) break;
	}
	fin.close();

	/***************************
	* main computation is here
	* ************************/

	auto startTime = Clock::now();

	FILE * fp;
	fp = fopen("result.dat", "w");

	if (!fp)
	{
		cout << "Error opening file for result" << endl;
		exit(1);
	}

	// 计算
	unsigned INT t;
	DOU_VEC  disturb0;
	for (t = 0; t < K; t++)
	{
		disturb0 = _mm256_set1_ps(disturb[t]);
		result[t] = logDataVSPrior(val, begin, end, res, disturb0, thread_num);
	}

// 单独花时间写文件
#pragma unroll(32)
	for (t = 0; t < K; t++)
	{
		fprintf(fp, "%d: %.5e\n", t + 1, result[t]);
	}

	fclose(fp);

	auto endTime = Clock::now();

	auto compTime = chrono::duration_cast<chrono::microseconds>(endTime - startTime);
	cout << "Computing time=" << compTime.count() << " microseconds" << endl;

	free(val);
	free(result);
	free(disturb);
	free(begin);
	free(end);

	return EXIT_SUCCESS;
}

DOU logDataVSPrior(const DOU*  val, const INT*  begin, const INT*  end, DOU_VEC*  res,
	const DOU_VEC tmp, const INT thread_num)
{
	register DOU_VEC res1, ans1, ans2, ans3, tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6;
	register int i, i_begin, i_end;
#pragma omp parallel private(i,i_begin,i_end,res1,ans1,ans2,ans3,tmp0,tmp1,tmp2,tmp3,tmp4,tmp5,tmp6)
	{
#pragma omp for schedule(static) nowait
		for (int j = 0; j < thread_num; j++)
		{
			res1 = _mm256_setzero_ps();
			i_begin = *(begin + j);
			i_end = *(end + j);
#pragma unroll(32)
			for (i = i_begin; i < i_end; ++i)
			{
				// i x 32 + i x 16 = i x 48
				register int index = (i << 5) + (i << 4);
				// pri0
				tmp1 = _mm256_load_ps(val + index);
				// dat0
				tmp2 = _mm256_load_ps(val + index + 8);
				// pri1
				tmp3 = _mm256_load_ps(val + index + 16);
				// dat1
				tmp4 = _mm256_load_ps(val + index + 24);
				// sig
				tmp5 = _mm256_load_ps(val + index + 32);
				// ctf
				tmp6 = _mm256_load_ps(val + index + 40);
				tmp0 = _mm256_mul_ps(tmp5, tmp);
				ans1 = _mm256_fnmadd_ps(tmp1, tmp6, tmp2);
				ans2 = _mm256_fnmadd_ps(tmp3, tmp6, tmp4);
				ans3 = _mm256_fmadd_ps(ans1, ans1, _mm256_mul_ps(ans2, ans2));
				res1 = _mm256_fmadd_ps(ans3, tmp0, res1);
			}
			*(res + j) = res1;
		}
	}
	res1 = res[0];
#pragma unroll(32)
	for (i = 1; i < thread_num; i++)
	{
		res1 = _mm256_add_ps(res1, *(res + i));
	}
	res1 = _mm256_hadd_ps(res1, res1);
	res1 = _mm256_hadd_ps(res1, res1);
	float *fin = new float[8];
	_mm256_store_ps(fin, res1);
	fin[0] += fin[4];
	return fin[0];
}


//#include <stdio.h>
//#include <iostream>
//#include <fstream>
//#include <chrono>
//#include <omp.h>
//#include <immintrin.h>
//#include <zmmintrin.h>
//#include <chrono>
//#include <omp.h>
//
//#define DOU float
//#define INT int
//#define DOU_VEC __m512
//
//using namespace std;
//
//typedef chrono::high_resolution_clock Clock;
//
//const int m = 1638400;	// DO NOT CHANGE!!
//const int K = 100000;	// DO NOT CHANGE!!
//
//DOU logDataVSPrior(const DOU*  val, const INT*   begin, const INT*   end, DOU_VEC*   res,
//	const DOU_VEC disturb0, const INT thread_num);
//
//int main(int argc, char *argv[])
//{
//	int length = (m >> 4) + (((m & 15) + 15) >> 4);
//	register int thread_num = omp_get_num_procs()/2;
//
//	int MM = (length << 4);
//	DOU * val = (DOU*)_aligned_malloc(sizeof(DOU) * 7 * MM,64 );//6
//	DOU * disturb = (DOU*)_aligned_malloc(sizeof(DOU)*K*2, 64);
//	DOU * result = (DOU*)_aligned_malloc(sizeof(DOU)*K*2, 64);
//	INT * begin = (INT*)_aligned_malloc(sizeof(INT)*thread_num, 64);
//	INT * end = (INT*)_aligned_malloc(sizeof(INT)*thread_num, 64);
//	DOU_VEC * res = (DOU_VEC*)_aligned_malloc(sizeof(DOU_VEC)*thread_num*2, 64);
//
//	DOU dat0, dat1, pri0, pri1, ctf0, sigRcp0;
//
//	register int tmp1, i;
//	register int num_per_thread = length / thread_num;
//	begin[0] = 0;
//
//#pragma unroll(32)
//	for (i = 0; i < thread_num - 1; i++)
//	{
//		tmp1 = begin[i] + num_per_thread;
//		end[i] = tmp1;
//		begin[i + 1] = tmp1;
//	}
//	end[thread_num - 1] = length;
//
//	/***************************
//	* Read data from input.dat
//	* *************************/
//
//	ifstream fin;
//
//	fin.open("input.dat");
//	if (!fin.is_open())
//	{
//		cout << "Error opening file input.dat" << endl;
//		exit(1);
//	}
//
//	i = 0;
//	int j;
//	while (!fin.eof())
//	{
//		fin >> dat0 >> dat1 >> pri0 >> pri1 >> ctf0 >> sigRcp0;
//		if (!(i & 15))
//			j = (i << 2) + (i << 1) - 1;//chengyi 6
//		j++;
//		val[j] = pri0;
//		val[j + 16] = dat0;
//		val[j + 32] = pri1;
//		val[j + 48] = dat1;
//		val[j + 64] = sigRcp0;
//		val[j + 80] = ctf0;
//		i++;
//		if (i == m) break;
//	}
//
//	for (; i < MM; i++)
//	{
//		if (!(i & 15))
//			j = (i << 2) + (i << 1) - 1;
//		j++;
//		val[j] = 0.00;
//		val[j + 16] = 0.00;
//		val[j + 32] = 0.00;
//		val[j + 48] = 0.00;
//		val[j + 64] = 0.00;
//		val[j + 80] = 0.00;
//	}
//
//	fin.close();
//
//	fin.open("K.dat");
//	if (!fin.is_open())
//	{
//		cout << "Error opening file K.dat" << endl;
//		exit(1);
//	}
//	i = 0;
//	while (!fin.eof())
//	{
//		fin >> disturb[i];
//		i++;
//		if (i == K) break;
//	}
//	fin.close();
//
//	/***************************
//	* main computation is here
//	* ************************/
//
//	auto startTime = Clock::now();
//
//	FILE * fp;
//	fp = fopen("result.dat", "w");
//
//	if (!fp)
//	{
//		cout << "Error opening file for result" << endl;
//		exit(1);
//	}
//
//	unsigned INT t;
//	DOU_VEC  disturb0;
//	for (t = 0; t < K; t++)
//	{
//		disturb0 = _mm512_set1_ps(disturb[t]);
//		result[t] = logDataVSPrior(val, begin, end, res, disturb0, thread_num);
//	}
//
//#pragma unroll(32)
//	for (t = 0; t < K; t++)
//	{
//		fprintf(fp, "%d: %.5e\n", t + 1, result[t]);
//	}
//
//	fclose(fp);
//
//	auto endTime = Clock::now();
//
//	auto compTime = chrono::duration_cast<chrono::microseconds>(endTime - startTime);
//	cout << "Computing time=" << compTime.count() << " microseconds" << endl;
//
//	_aligned_free(val);
//	_aligned_free(result);
//	_aligned_free(disturb);
//	_aligned_free(begin);
//	_aligned_free(end);
//
//	return EXIT_SUCCESS;
//}
//
//DOU logDataVSPrior(const DOU*   val, const INT*   begin, const INT*   end, DOU_VEC*   res,
//	const DOU_VEC tmp, const INT thread_num)
//{
//	register DOU_VEC res1, ans1, ans2, ans3, tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6;
//	register int i, i_begin, i_end;
//	//cout << "openmp" << endl;
//#pragma omp parallel private(i,i_begin,i_end,res1,ans1,ans2,ans3,tmp0,tmp1,tmp2,tmp3,tmp4,tmp5,tmp6)
//	{
//#pragma omp for schedule(static) nowait
//		for (int j = 0; j < thread_num; j++)
//		{
//			res1 = _mm512_setzero_ps();
//			i_begin = *(begin + j);
//			i_end = *(end + j);
//#pragma unroll(32)
//			for (i = i_begin; i < i_end; i++)
//			{
//				register int index = (i << 6) + (i << 5); // chengyi 96
//				tmp1 = _mm512_load_ps(val + index);
//				tmp2 = _mm512_load_ps(val + index + 16);
//				tmp3 = _mm512_load_ps(val + index + 32);
//				tmp4 = _mm512_load_ps(val + index + 48);
//				tmp5 = _mm512_load_ps(val + index + 64);
//				tmp6 = _mm512_load_ps(val + index + 80);
//				tmp0 = _mm512_mul_ps(tmp6, tmp);
//				ans1 = _mm512_fnmadd_ps(tmp1, tmp0, tmp2);
//				ans2 = _mm512_fnmadd_ps(tmp3, tmp0, tmp4);
//				ans3 = _mm512_fmadd_ps(ans1, ans1, _mm512_mul_ps(ans2, ans2));
//				res1 = _mm512_fmadd_ps(ans3, tmp5, res1);
//			}
//			*(res + j) = res1;
//		}
//	}
//	res1 = res[0];
//#pragma unroll(32)
//	for (i = 1; i < thread_num; i++)
//	{
//		res1 = _mm512_add_ps(res1, *(res + i));
//	}
//	return _mm512_reduce_add_ps(res1);
//}