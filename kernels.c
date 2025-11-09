/********************************************************
 * Kernels to be optimized for the OS&C prflab.
 * Acknowledgment: This lab is an extended version of the
 * CS:APP Performance Lab
 ********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "defs.h"
#include "smooth.h" // helper functions for naive_smooth
#include "blend.h"  // helper functions for naive_blend
#define BLOCK_SIZE 32
#include <pthread.h>

/* 
 * Please fill in the following struct
 */
student_t student = {
    "mbez",             /* ITU alias */
    "Max Pieter Bezemer",    /* Full name */
    "mbez@itu.dk", /* Email address */
};

/******************************************************************************
 * ROTATE KERNEL
 *****************************************************************************/

// Your different versions of the rotate kernel go here

/* 
 * naive_rotate - The naive baseline version of rotate 
 */
/* stride pattern, visualization (we recommend that you draw this for your functions):
    dst               src
    3 7 B F           0 1 2 3
    2 6 A E           4 5 6 7
    1 5 9 D           8 9 A B
    0 4 8 C           C D E F
 */
char naive_rotate_descr[] = "naive_rotate: Naive baseline implementation";
void naive_rotate(int dim, pixel *src, pixel *dst) 
{
    int i, j;

    for (i = 0; i < dim; i++)
	for (j = 0; j < dim; j++)
	    dst[RIDX(dim-1-j, i, dim)] = src[RIDX(i, j, dim)];
}



/* 
 * rotate_1: swtich loops and precalculate dst_col

 * */
char rotate_descr_1[] = "rotate: switch loop optimization";
void rotate_1(int dim, pixel *src, pixel *dst)
{
    const int N = dim;
    for (int j = 0; j < N; j++) {
    	int dst_col = N - 1 - j;
        for (int i = 0; i < N; i++) {
            dst[dst_col * N + i] = src[i * N + j];
        }
    }
}

// helper function to calculate the lesser integer
static inline int min_int(int a, int b) {return a < b ? a : b;}

/* 
 * rotate_2: divide image into blocks for cache sized processing
 * */
char rotate_descr_2[] = "rotate: divide image into blocks for cache sized processing";
void rotate_2(int dim, pixel *src, pixel *dst)
{
    inline int min_int(int a, int b) {return a < b ? a : b;}

    const int N = dim;

	    for (int ii = 0; ii < N; ii += BLOCK_SIZE) {
		    int i_max = min_int(ii + BLOCK_SIZE, N);
		    for (int jj = 0; jj < N; jj += BLOCK_SIZE) {
			    int j_max = min_int(jj + BLOCK_SIZE, N);

			    for (int j = jj; j < j_max; ++j) {
				    int d = (N - 1 - j) * N + ii;
				    int s = ii * N + j;
				    for (int i = ii; i < i_max; ++i, d++, s += N) {
					    dst[d] = src[s];
				    }
			    }
		    }
	    }
}


/* 
 * rotate_3: divided in blocks + separate processing of transpose and reverse
 * */
char rotate_descr_3[] = "rotate:  divided in blocks + separate processing of transpose and reverse";
void rotate_3(int dim, pixel *src, pixel *dst)
{

    const int N = dim;
    pixel tmp[N * N];
	
    // first transpose (src -> tmp)
    for (int ii = 0; ii < N; ii += BLOCK_SIZE) {
	    const int i_max = min_int(ii + BLOCK_SIZE, N);
	    for (int jj = 0; jj < N; jj += BLOCK_SIZE) {
		    const int j_max = min_int(jj + BLOCK_SIZE, N);
		    for (int i = ii; i < i_max; ++i) {
			    const int src_row = i * N;
			    for (int j = jj; j < j_max; ++j) {
				tmp[j * N + i] = src[src_row + j];
			    }
		    }
	    }
    }

    // then reverse columns (tmp -> dst
    for (int jj = 0; jj < N; jj += BLOCK_SIZE) {
	    const int j_max = min_int(jj + BLOCK_SIZE, N);
	    for (int ii = 0; ii < N; ii += BLOCK_SIZE) {
		    const int i_max = min_int(ii + BLOCK_SIZE, N);
		    for (int j = jj; j< j_max; ++j) {
			    int d = ( N - 1 - ii) * N + j;
			    int s = ii * N + j;
			    for (int i = ii; i < i_max; ++i, d -= N, s += N) {
				    dst[d] = tmp[s];
			    }
		    }

	    }
    }
}

/* 
 * rotate_4: loop unrolling & block processing
 * */
char rotate_descr_4[] = "rotate_4: loop unrolling & block processing";
void rotate_4(int dim, pixel *src, pixel *dst)
{
    const int N = dim;

    for (int jj = 0; jj < N; jj += BLOCK_SIZE) {
        const int j_max = min_int(jj + BLOCK_SIZE, N);

        for (int ii = 0; ii < N; ii += BLOCK_SIZE) {
            const int i_max = min_int(ii + BLOCK_SIZE,N);
            for (int j = jj; j < j_max; ++j) {
                // base pointers
                pixel *d = dst + (N - 1 - j) * N + ii; // start of destination row
                pixel *s = src + ii * N + j;           // start of source column

                int i = ii;

                // modest unrolli of 4, avoids register spills
                for (; i + 4 <= i_max; i += 4, d += 4, s += 4 * N) {
                    d[0] = s[0 * N];
                    d[1] = s[1 * N];
                    d[2] = s[2 * N];
                    d[3] = s[3 * N];
                }
                // rest of image if not multiple of 4
                for (; i < i_max; ++i, ++d, s += N) {
                    *d = *s;
                }
            }
        }
    }
}

/* 
 * rotate: final optimization
 * */
char rotate_descr[] = "rotate: final optimization";
void rotate(int dim, pixel *src, pixel *dst)
{
    rotate_4(dim, src, dst);
}

/*
 * register_rotate_functions - Register all of your different versions
 *     of the rotate kernel with the driver by calling the
 *     add_rotate_function() for each test function.
 */
void register_rotate_functions() 
{
    add_rotate_function(&naive_rotate, naive_rotate_descr);
    add_rotate_function(&rotate, rotate_descr);
}

/******************************************************************************
 * ROTATE_T KERNEL
 *****************************************************************************/

// Your different versions of the rotate_t kernel go here
// (i.e. rotate with multi-threading)

/* 
 * rotate_t - Your current working version of rotate_t
 * IMPORTANT: This is the version you will be graded on
 */
char rotate_t_descr[] = "rotate_t: Current working version";
void rotate_t(int dim, pixel *src, pixel *dst)
{
    naive_rotate(dim, src, dst);
}

/*********************************************************************
 * register_rotate_t_functions - Register all of your different versions
 *     of the rotate_t kernel with the driver by calling the
 *     add_rotate_t_function() for each test function. When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.  
 *********************************************************************/

void register_rotate_t_functions() 
{
    add_rotate_t_function(&rotate_t, rotate_t_descr);
    /* ... Register additional test functions here */
}

/******************************************************************************
 * SMOOTH KERNEL
 *****************************************************************************/

// Your different versions of the smooth kernel go here

/*
 * naive_smooth - The naive baseline version of smooth 
 */
char naive_smooth_descr[] = "naive_smooth: Naive baseline implementation";
void naive_smooth(int dim, pixel *src, pixel *dst) 
{
    int i, j;

    for (i = 0; i < dim; i++)
	for (j = 0; j < dim; j++)
	    dst[RIDX(i, j, dim)] = avg(dim, i, j, src); // `avg` defined in smooth.c
}

char smooth_descr[] = "smooth: Current working version";
void smooth(int dim, pixel *src, pixel *dst)
{
  naive_smooth(dim, src, dst);
}

/*
 * register_smooth_functions - Register all of your different versions
 *     of the smooth kernel with the driver by calling the
 *     add_smooth_function() for each test function.
 */

void register_smooth_functions() {
    add_smooth_function(&smooth, smooth_descr);
    /* ... Register additional test functions here */
}

/******************************************************************************
 * SMOOTH_N KERNEL
 *****************************************************************************/

// Your different versions of the smooth_n kernel go here
// (i.e. where anything goes, including multithreading and SIMD).

char smooth_n_descr[] = "smooth_n: Current working version";
void smooth_n(int dim, pixel *src, pixel *dst)
{
    naive_smooth(dim, src, dst);
}

/*
 * register_smooth_n_functions - Register all of your different versions
 *     of the smooth_n kernel with the driver by calling the
 *     add_smooth_n_function() for each test function.
 */
void register_smooth_n_functions() {
    add_smooth_n_function(&smooth_n, smooth_n_descr);
    /* ... Register additional test functions here */
}

/******************************************************************************
 * BLEND KERNEL
 *****************************************************************************/

// Your different versions of the blend kernel go here.

char naive_blend_descr[] = "naive_blend: Naive baseline implementation";
void naive_blend(int dim, pixel *src, pixel *dst) // reads global variable `pixel bgc`
{
    int i, j;

    for (i = 0; i < dim; i++)
	for (j = 0; j < dim; j++)
	    blend_pixel(&src[RIDX(i, j, dim)], &dst[RIDX(i, j, dim)], &bgc); // `blend_pixel` defined in blend.c
}

char blend_descr[] = "blend: Current working version";
void blend(int dim, pixel *src, pixel *dst)
{
    naive_blend(dim, src, dst);
}

/*
 * register_blend_functions - Register all of your different versions
 *     of the blend kernel with the driver by calling the
 *     add_blend_function() for each test function.
 */
void register_blend_functions() {
    add_blend_function(&blend, blend_descr);
    /* ... Register additional test functions here */
}

/******************************************************************************
 * BLEND_V KERNEL
 *****************************************************************************/

// Your different versions of the blend_v kernel go here
// (i.e. with vectorization, aka. SIMD).

char blend_v_descr[] = "blend_v: Current working version";
void blend_v(int dim, pixel *src, pixel *dst)
{
    naive_blend(dim, src, dst);
}

/*
 * register_blend_v_functions - Register all of your different versions
 *     of the blend_v kernel with the driver by calling the
 *     add_blend_function() for each test function.
 */
void register_blend_v_functions() {
    add_blend_v_function(&blend_v, blend_v_descr);
    /* ... Register additional test functions here */
}
