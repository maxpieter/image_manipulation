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
#include <pthread.h>


/* 
 * Please fill in the following struct
 */
student_t student = {
    "mbez",            	 	/* ITU alias */
    "Max Pieter Bezemer",    	/* Full name */
    "mbez@itu.dk", 		/* Email address */
};

/******************************************************************************
 * ROTATE KERNEL
 *****************************************************************************/

// helper function to calculate the lesser integer
static inline int min_int(int a, int b) {return a < b ? a : b;}

#define BLOCK_SIZE_ROTATE 32

/* 
 * rotate: loop unrolling & block processing
 * */
char rotate_descr[] = "rotate_4: loop unrolling & block processing";
void rotate(int dim, pixel *src, pixel *dst)
{
    const int N = dim;

    for (int jj = 0; jj < N; jj += BLOCK_SIZE_ROTATE) {
        const int j_max = min_int(jj + BLOCK_SIZE_ROTATE, N);

        for (int ii = 0; ii < N; ii += BLOCK_SIZE_ROTATE) {
            const int i_max = min_int(ii + BLOCK_SIZE_ROTATE,N);
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

void register_rotate_functions() 
{
    add_rotate_function(&rotate, rotate_descr);
}

/******************************************************************************
 * ROTATE_T KERNEL
 *****************************************************************************/

// Your different versions of the rotate_t kernel go here
// (i.e. rotate with multi-threading)
#define THREAD_COUNT 8
#define BLOCK_WIDTH 16

typedef struct
{
    int dim;
    pixel *src;
    pixel *dst;
    int start;
    int end;
} rotate_thread_args;

void *multi_rotate_worker(void *args)
{
    rotate_thread_args *data = (rotate_thread_args *)args;
    int i, j, m, n;
    int dim = data->dim;
    int limit = dim - 1;
    pixel *dst = data->dst;
    pixel *src = data->src;
    int src_inc = 8;
    int dst_inc = dim * 8;
    int BLOCK_HEIGHT = (dim == 2048) ? 4 : 8;

    for (i = data->start; i < data->end; i += BLOCK_HEIGHT)
    {
        for (j = 0; j < dim; j += BLOCK_WIDTH)
        {
            int n_end = j + BLOCK_WIDTH;
            for (m = i; m < i + BLOCK_HEIGHT && m < dim; m++)
            {

                int src_ptr = RIDX(m, j, dim);
                int dst_ptr = RIDX(limit - j, m, dim);

                for (n = j; n < n_end; n += 8)
                {

                    __builtin_prefetch(&src[src_ptr + 8], 0, 1);

                    dst[dst_ptr] = src[src_ptr];
                    dst[dst_ptr - dim] = src[src_ptr + 1];
                    dst[dst_ptr - dim * 2] = src[src_ptr + 2];
                    dst[dst_ptr - dim * 3] = src[src_ptr + 3];
                    dst[dst_ptr - dim * 4] = src[src_ptr + 4];
                    dst[dst_ptr - dim * 5] = src[src_ptr + 5];
                    dst[dst_ptr - dim * 6] = src[src_ptr + 6];
                    dst[dst_ptr - dim * 7] = src[src_ptr + 7];

                    src_ptr += src_inc;
                    dst_ptr -= dst_inc;
                }
            }
        }
    }
    return NULL;
}

/*
 * rotate_t - Your current working version of rotate_t
 * IMPORTANT: This is the version you will be graded on
 */
char rotate_t_descr[] = "rotate_t: Current working version";
void rotate_t(int dim, pixel *src, pixel *dst)
{
    if (dim > 256)
    {
        int t;
        pthread_t threads[THREAD_COUNT];
        rotate_thread_args thread_args_list[THREAD_COUNT];
        int per_thread = dim / THREAD_COUNT;

        for (t = 0; t < THREAD_COUNT; t++)
        {
            thread_args_list[t].dim = dim;
            thread_args_list[t].src = src;
            thread_args_list[t].dst = dst;
            thread_args_list[t].start = t * per_thread;
            thread_args_list[t].end = (t == THREAD_COUNT - 1) ? dim : (t + 1) * per_thread;

            pthread_create(&threads[t], NULL, multi_rotate_worker, (void *)&thread_args_list[t]);
        }

        for (t = 0; t < THREAD_COUNT; t++)
        {
            pthread_join(threads[t], NULL);
        }
    }
    else
        rotate(dim, src, dst);
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
