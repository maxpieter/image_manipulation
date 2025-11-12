/********************************************************
 * Kernels to be optimized for the OS&C prflab.
 * Acknowledgment: This lab is an extended version of the
 * CS:APP Performance Lab
 ********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

char smooth_descr[] = "smooth: Row-summed scalar implementation";
static inline void build_horizontal_sums(const pixel *row, int dim, pixel_sum *restrict out)
{
    for (int j = 0; j < dim; ++j) {
        pixel_sum sum;
        sum.red = row[j].red;
        sum.green = row[j].green;
        sum.blue = row[j].blue;
        sum.alpha = row[j].alpha;
        sum.num = 1;

        if (j > 0) {
            const pixel *left = row + j - 1;
            sum.red += left->red;
            sum.green += left->green;
            sum.blue += left->blue;
            sum.alpha += left->alpha;
            sum.num++;
        }
        if (j + 1 < dim) {
            const pixel *right = row + j + 1;
            sum.red += right->red;
            sum.green += right->green;
            sum.blue += right->blue;
            sum.alpha += right->alpha;
            sum.num++;
        }
        out[j] = sum;
    }
}

void smooth(int dim, pixel *src, pixel *dst)
{
    if (dim == 0) {
        return;
    }

    size_t row_bytes = (size_t)dim * sizeof(pixel_sum);
    pixel_sum *row_above = malloc(row_bytes);
    pixel_sum *row_curr = malloc(row_bytes);
    pixel_sum *row_below = malloc(row_bytes);

    if (!row_above || !row_curr || !row_below) {
        free(row_above);
        free(row_curr);
        free(row_below);
        naive_smooth(dim, src, dst);
        return;
    }

    build_horizontal_sums(src, dim, row_curr);
    memcpy(row_above, row_curr, row_bytes);
    if (dim > 1) {
        build_horizontal_sums(src + dim, dim, row_below);
    } else {
        memcpy(row_below, row_curr, row_bytes);
    }

    for (int i = 0; i < dim; ++i) {
        pixel *drow = dst + (size_t)i * dim;
        const pixel_sum *top = (i > 0) ? row_above : NULL;
        const pixel_sum *mid = row_curr;
        const pixel_sum *bot = (i < dim - 1) ? row_below : NULL;

        for (int j = 0; j < dim; ++j) {
            int total_r = mid[j].red;
            int total_g = mid[j].green;
            int total_b = mid[j].blue;
            int total_a = mid[j].alpha;
            int count = mid[j].num;

            if (top) {
                total_r += top[j].red;
                total_g += top[j].green;
                total_b += top[j].blue;
                total_a += top[j].alpha;
                count += top[j].num;
            }
            if (bot) {
                total_r += bot[j].red;
                total_g += bot[j].green;
                total_b += bot[j].blue;
                total_a += bot[j].alpha;
                count += bot[j].num;
            }

            drow[j].red = (unsigned short)(total_r / count);
            drow[j].green = (unsigned short)(total_g / count);
            drow[j].blue = (unsigned short)(total_b / count);
            drow[j].alpha = (unsigned short)(total_a / count);
        }

        if (i == dim - 1) {
            break;
        }

        pixel_sum *tmp = row_above;
        row_above = row_curr;
        row_curr = row_below;
        row_below = tmp;

        int load_row = i + 2;
        if (load_row >= dim) {
            memcpy(row_below, row_curr, row_bytes);
        } else {
            build_horizontal_sums(src + (size_t)load_row * dim, dim, row_below);
        }
    }

    free(row_above);
    free(row_curr);
    free(row_below);
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

#define SMOOTH_N_MAX_THREADS 8

typedef struct {
    int dim;
    int start_row;
    int end_row;
    pixel *src;
    pixel *dst;
} smooth_n_args;

static void* smooth_n_worker(void *arg)
{
    smooth_n_args *a = (smooth_n_args *)arg;
    const int dim = a->dim;
    int start = a->start_row;
    int end = a->end_row;

    if (start >= end) {
        return NULL;
    }

    const pixel *src = a->src;
    pixel *dst = a->dst;
    size_t row_bytes = (size_t)dim * sizeof(pixel_sum);

    pixel_sum *row_above = malloc(row_bytes);
    pixel_sum *row_curr = malloc(row_bytes);
    pixel_sum *row_below = malloc(row_bytes);
    if (!row_above || !row_curr || !row_below) {
        free(row_above);
        free(row_curr);
        free(row_below);
        return NULL;
    }

    int prev_row = (start == 0) ? 0 : start - 1;
    int curr_row = start;
    int next_row = (start + 1 <= dim - 1) ? start + 1 : dim - 1;

    build_horizontal_sums(src + (size_t)prev_row * dim, dim, row_above);
    build_horizontal_sums(src + (size_t)curr_row * dim, dim, row_curr);
    build_horizontal_sums(src + (size_t)next_row * dim, dim, row_below);

    for (int i = start; i < end; ++i) {
        pixel *drow = dst + (size_t)i * dim;
        const pixel_sum *top = (i > 0) ? row_above : NULL;
        const pixel_sum *mid = row_curr;
        const pixel_sum *bot = (i < dim - 1) ? row_below : NULL;

        for (int j = 0; j < dim; ++j) {
            int total_r = mid[j].red;
            int total_g = mid[j].green;
            int total_b = mid[j].blue;
            int total_a = mid[j].alpha;
            int count = mid[j].num;

            if (top) {
                total_r += top[j].red;
                total_g += top[j].green;
                total_b += top[j].blue;
                total_a += top[j].alpha;
                count += top[j].num;
            }
            if (bot) {
                total_r += bot[j].red;
                total_g += bot[j].green;
                total_b += bot[j].blue;
                total_a += bot[j].alpha;
                count += bot[j].num;
            }

            drow[j].red = (unsigned short)(total_r / count);
            drow[j].green = (unsigned short)(total_g / count);
            drow[j].blue = (unsigned short)(total_b / count);
            drow[j].alpha = (unsigned short)(total_a / count);
        }

        if (i + 1 >= end) {
            break;
        }

        pixel_sum *tmp = row_above;
        row_above = row_curr;
        row_curr = row_below;
        row_below = tmp;

        prev_row = curr_row;
        curr_row = next_row;
        if (next_row < dim - 1) {
            next_row++;
            build_horizontal_sums(src + (size_t)next_row * dim, dim, row_below);
        } else {
            memcpy(row_below, row_curr, row_bytes);
        }
    }

    free(row_above);
    free(row_curr);
    free(row_below);
    return NULL;
}

char smooth_n_descr[] = "smooth_n: Row-partitioned multithreaded smoothing";
void smooth_n(int dim, pixel *src, pixel *dst)
{
    if (dim < 128) {
        smooth(dim, src, dst);
        return;
    }

    int threads = (dim >= 2048) ? 8 : (dim >= 1024 ? 4 : 2);
    if (threads > SMOOTH_N_MAX_THREADS) {
        threads = SMOOTH_N_MAX_THREADS;
    }
    if (threads > dim) {
        threads = dim;
    }
    if (threads < 1) {
        threads = 1;
    }

    pthread_t workers[SMOOTH_N_MAX_THREADS];
    smooth_n_args args[SMOOTH_N_MAX_THREADS];

    int base = dim / threads;
    int rem = dim % threads;
    int row = 0;
    for (int t = 0; t < threads; ++t) {
        int take = base + (t < rem ? 1 : 0);
        args[t].dim = dim;
        args[t].src = src;
        args[t].dst = dst;
        args[t].start_row = row;
        args[t].end_row = row + take;
        row += take;
        pthread_create(&workers[t], NULL, smooth_n_worker, &args[t]);
    }

    for (int t = 0; t < threads; ++t) {
        pthread_join(workers[t], NULL);
    }
}

/*
 * register_smooth_n_functions
 */
void register_smooth_n_functions() {
    add_smooth_n_function(&smooth_n, smooth_n_descr);
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
