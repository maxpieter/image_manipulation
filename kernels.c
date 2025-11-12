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

student_t student = {
    "mbez",            	 	/* ITU alias */
    "Max Pieter Bezemer",    	/* Full name */
    "mbez@itu.dk", 		    /* Email address */
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

// register_rotate_t_functions 

void register_rotate_t_functions()
{
    add_rotate_t_function(&rotate_t, rotate_t_descr);
}

/******************************************************************************
 * SMOOTH KERNEL
 *****************************************************************************/

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

char smooth_descr[] = "Loop Peeling, Function Inlining, Manual Caching";
void smooth(int dim, pixel *src, pixel *dst)
{
    int i, k, k_base;
    // top-left corner (0,0)
    dst[0].blue = (src[0].blue + src[1].blue + src[dim].blue + src[dim + 1].blue) >> 2;
    dst[0].green = (src[0].green + src[1].green + src[dim].green + src[dim + 1].green) >> 2;
    dst[0].red = (src[0].red + src[1].red + src[dim].red + src[dim + 1].red) >> 2;
    dst[0].alpha = (src[0].alpha + src[1].alpha + src[dim].alpha + src[dim + 1].alpha) >> 2;

    // top right corner (0, dim-1)
    dst[dim - 1].blue = (src[dim - 1].blue + src[dim - 2].blue + src[dim + dim - 2].blue + src[dim + dim - 1].blue) >> 2;
    dst[dim - 1].green = (src[dim - 1].green + src[dim - 2].green + src[dim + dim - 2].green + src[dim + dim - 1].green) >> 2;
    dst[dim - 1].red = (src[dim - 1].red + src[dim - 2].red + src[dim + dim - 2].red + src[dim + dim - 1].red) >> 2;
    dst[dim - 1].alpha = (src[dim - 1].alpha + src[dim - 2].alpha + src[dim + dim - 2].alpha + src[dim + dim - 1].alpha) >> 2;

    // bottom left corner (dim-1,0)
    dst[RIDX(dim - 1, 0, dim)].blue = (src[RIDX(dim - 1, 0, dim)].blue + src[RIDX(dim - 1, 1, dim)].blue + src[RIDX(dim - 2, 0, dim)].blue + src[RIDX(dim - 2, 1, dim)].blue) >> 2;
    dst[RIDX(dim - 1, 0, dim)].green = (src[RIDX(dim - 1, 0, dim)].green + src[RIDX(dim - 1, 1, dim)].green + src[RIDX(dim - 2, 0, dim)].green + src[RIDX(dim - 2, 1, dim)].green) >> 2;
    dst[RIDX(dim - 1, 0, dim)].red = (src[RIDX(dim - 1, 0, dim)].red + src[RIDX(dim - 1, 1, dim)].red + src[RIDX(dim - 2, 0, dim)].red + src[RIDX(dim - 2, 1, dim)].red) >> 2;
    dst[RIDX(dim - 1, 0, dim)].alpha = (src[RIDX(dim - 1, 0, dim)].alpha + src[RIDX(dim - 1, 1, dim)].alpha + src[RIDX(dim - 2, 0, dim)].alpha + src[RIDX(dim - 2, 1, dim)].alpha) >> 2;

    // bottom right corner (dim-1, dim-1)
    dst[RIDX(dim - 1, dim - 1, dim)].blue = (src[RIDX(dim - 1, dim - 1, dim)].blue + src[RIDX(dim - 1, dim - 2, dim)].blue + src[RIDX(dim - 2, dim - 2, dim)].blue + src[RIDX(dim - 2, dim - 1, dim)].blue) >> 2;
    dst[RIDX(dim - 1, dim - 1, dim)].green = (src[RIDX(dim - 1, dim - 1, dim)].green + src[RIDX(dim - 1, dim - 2, dim)].green + src[RIDX(dim - 2, dim - 2, dim)].green + src[RIDX(dim - 2, dim - 1, dim)].green) >> 2;
    dst[RIDX(dim - 1, dim - 1, dim)].red = (src[RIDX(dim - 1, dim - 1, dim)].red + src[RIDX(dim - 1, dim - 2, dim)].red + src[RIDX(dim - 2, dim - 2, dim)].red + src[RIDX(dim - 2, dim - 1, dim)].red) >> 2;
    dst[RIDX(dim - 1, dim - 1, dim)].alpha = (src[RIDX(dim - 1, dim - 1, dim)].alpha + src[RIDX(dim - 1, dim - 2, dim)].alpha + src[RIDX(dim - 2, dim - 2, dim)].alpha + src[RIDX(dim - 2, dim - 1, dim)].alpha) >> 2;

    for (int j = 1; j <= dim - 2; j++)
    {
        // top
        i = j;
        dst[j].blue = (src[j].blue + src[j + dim].blue + src[j - 1].blue + src[j + 1].blue + src[j + dim - 1].blue + src[j + dim + 1].blue) / 6;
        dst[j].green = (src[j].green + src[j + dim].green + src[j - 1].green + src[j + 1].green + src[j + dim - 1].green + src[j + dim + 1].green) / 6;
        dst[j].red = (src[j].red + src[j + dim].red + src[j - 1].red + src[j + 1].red + src[j + dim - 1].red + src[j + dim + 1].red) / 6;
        dst[j].alpha = (src[j].alpha + src[j + dim].alpha + src[j - 1].alpha + src[j + 1].alpha + src[j + dim - 1].alpha + src[j + dim + 1].alpha) / 6;
        // bottom
        i = dim * dim - dim + j;
        dst[i].blue = (src[i].blue + src[i - 1].blue + src[i + 1].blue + src[i - dim].blue + src[i - dim - 1].blue + src[i - dim + 1].blue) / 6;
        dst[i].green = (src[i].green + src[i - 1].green + src[i + 1].green + src[i - dim].green + src[i - dim - 1].green + src[i - dim + 1].green) / 6;
        dst[i].red = (src[i].red + src[i - 1].red + src[i + 1].red + src[i - dim].red + src[i - dim - 1].red + src[i - dim + 1].red) / 6;
        dst[i].alpha = (src[i].alpha + src[i - 1].alpha + src[i + 1].alpha + src[i - dim].alpha + src[i - dim - 1].alpha + src[i - dim + 1].alpha) / 6;
        // left edge
        i = j * dim;
        dst[i].blue = (src[i].blue + src[i - dim].blue + src[i - dim + 1].blue + src[i + 1].blue + src[i + dim].blue + src[i + dim + 1].blue) / 6;
        dst[i].green = (src[i].green + src[i - dim].green + src[i - dim + 1].green + src[i + 1].green + src[i + dim].green + src[i + dim + 1].green) / 6;
        dst[i].red = (src[i].red + src[i - dim].red + src[i - dim + 1].red + src[i + 1].red + src[i + dim].red + src[i + dim + 1].red) / 6;
        dst[i].alpha = (src[i].alpha + src[i - dim].alpha + src[i - dim + 1].alpha + src[i + 1].alpha + src[i + dim].alpha + src[i + dim + 1].alpha) / 6;
        // right edge
        i = j * dim + dim - 1;
        dst[i].blue = (src[i].blue + src[i - dim].blue + src[i - dim - 1].blue + src[i - 1].blue + src[i + dim].blue + src[i + dim - 1].blue) / 6;
        dst[i].green = (src[i].green + src[i - dim].green + src[i - dim - 1].green + src[i - 1].green + src[i + dim].green + src[i + dim - 1].green) / 6;
        dst[i].red = (src[i].red + src[i - dim].red + src[i - dim - 1].red + src[i - 1].red + src[i + dim].red + src[i + dim - 1].red) / 6;
        dst[i].alpha = (src[i].alpha + src[i - dim].alpha + src[i - dim - 1].alpha + src[i - 1].alpha + src[i + dim].alpha + src[i + dim - 1].alpha) / 6;
    }
    for (int i = 1; i <= dim - 2; i++)
    {
        k_base = i * dim;
        for (int j = 1; j <= dim - 2; j++)
        {
            k = k_base + j;
            pixel p_tl = src[k - dim - 1];
            pixel p_tm = src[k - dim]; 
            pixel p_tr = src[k - dim + 1];
            pixel p_ml = src[k - 1];
            pixel p_mm = src[k]; 
            pixel p_mr = src[k + 1];
            pixel p_bl = src[k + dim - 1];
            pixel p_bm = src[k + dim];
            pixel p_br = src[k + dim + 1];

            dst[k].blue = (p_tl.blue + p_tm.blue + p_tr.blue +
                           p_ml.blue + p_mm.blue + p_mr.blue +
                           p_bl.blue + p_bm.blue + p_br.blue) /
                          9;

            dst[k].green = (p_tl.green + p_tm.green + p_tr.green +
                            p_ml.green + p_mm.green + p_mr.green +
                            p_bl.green + p_bm.green + p_br.green) /
                           9;

            dst[k].red = (p_tl.red + p_tm.red + p_tr.red +
                          p_ml.red + p_mm.red + p_mr.red +
                          p_bl.red + p_bm.red + p_br.red) /
                         9;

            dst[k].alpha = (p_tl.alpha + p_tm.alpha + p_tr.alpha +
                            p_ml.alpha + p_mm.alpha + p_mr.alpha +
                            p_bl.alpha + p_bm.alpha + p_br.alpha) /
                           9;
        }
    }
}

/*
 * register_smooth_functions
 */

void register_smooth_functions()
{
    add_smooth_function(&smooth, smooth_descr);
}

/******************************************************************************
 * SMOOTH_N KERNEL
 *****************************************************************************/

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
