// Copyright 2022 Synnada, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef UTILS_H
#define UTILS_H
#include "array.h"


#define SWAP(a, b) do { \
    typeof(a) temp = a; \
    a = b;              \
    b = temp;           \
} while (0)

typedef void (*Op)(float *output, float input);

int *broadcastStride(const Array *t1, const int *shape, const int ndim) 
{
    int *newStrides = (int *)malloc(ndim * sizeof(int));
    memset(newStrides, 0, ndim * sizeof(int));

    if (t1->ndim == 1) {
        // Handle 1D arrays
        int target_dim = ndim - 1; // Target the last dimension of the output
        int input_size = t1->shape[0];
        int output_size = shape[target_dim];

        if (input_size == 1 && output_size > 1) {
            newStrides[target_dim] = 0;
        } else {
            newStrides[target_dim] = t1->strides[0];
        }
    } else {
        int diff = ndim - t1->ndim;
        for (int j = 0; j < t1->ndim; j++) {
            int target_dim = j + diff;
            int input_dim_size = t1->shape[j];
            int output_dim_size = shape[target_dim];

            if (input_dim_size == output_dim_size) {
                newStrides[target_dim] = t1->strides[j];
            } else if (input_dim_size == 1) {
                newStrides[target_dim] = (output_dim_size > 1) ? 0 : t1->strides[j];
            } else {
                fprintf(stderr, "Invalid broadcast from shape %d to %d\n", input_dim_size, output_dim_size);
                free(newStrides);
                exit(EXIT_FAILURE);
            }

        }
}
    return newStrides;
}

size_t loc(size_t idx, const int *shapes, const int *strides, const int ndim) 
{
    size_t loc = 0;
    for (int i = ndim - 1; i >= 0; i--) {
        int dim_size = shapes[i];
        int coord = idx % dim_size;
        loc += coord * strides[i];  // Stride is 0 for broadcasted dims
        idx /= dim_size;
    }
    return loc;
}

/* Handles binary operations on two arrays */
void binary_array_iterator(const Array *left, const Array *right, Array *out, float (*op)(float, float))
{
    const float *left_data = left->data;
    const float *right_data = right->data;

    int *left_b_strides = broadcastStride(left, out->shape, out->ndim);
    int *right_b_strides = broadcastStride(right, out->shape, out->ndim);

    for (size_t i = 0; i < out->size; i++)
    {
        // TODO: Use loc only when the Tensor is not contiguous
        size_t left_idx = loc(i, out->shape, left_b_strides, out->ndim);
        size_t right_idx = loc(i, out->shape, right_b_strides, out->ndim);
        if (left_idx >= left->size) {
            printf("Left index out of bounds: %zu >= %d\n", left_idx, left->size);
            exit(1);
        }
        if (right_idx >= right->size) {
            printf("Right index out of bounds: %zu >= %d\n", right_idx, right->size);
            exit(1);
        }
        out->data[i] = op(left_data[left_idx], right_data[right_idx]);
    }

    free(left_b_strides);
    free(right_b_strides);
}





/* If the input Array  is contiguous, and the reduction type is all, reduce the array directly */
void reduce_contiguous_all(const Array *input, Array *out, float init_val, Op op)
{
    const float *input_data = input->data;
    float *output_data = out->data;
    *output_data = init_val;

    for (size_t i = 0; i < input->size; i++)
    {
        op(output_data, input_data[i]);
    }
}

void reduce_contiguous_dim(const float *input_data, float *output_data, const int *reduction_size, const int *reduction_strides, size_t offset, size_t dim, size_t max_dim, Op op)
{
    if (dim == max_dim - 1)
    {
        for (size_t i = 0; i < reduction_size[dim]; i++)
        {
            op(output_data, input_data[offset + i * reduction_strides[dim]]);
        }
    }
    else
    {
        for (size_t i = 0; i < reduction_size[dim]; i++)
        {
            reduce_contiguous_dim(input_data, output_data, reduction_size, reduction_strides, offset + i * reduction_strides[dim], dim + 1, max_dim, op);
        }
    }
}

/* If the input Array is contiguous, and reduction only some of the axes will be reduced */
void reduce_contiguous(const Array *input, Array *out, const int *axes, size_t num_axes, float init_val, Op op)
{
    const int *in_shapes = input->shape;
    const int *in_strides = input->strides;

    int *reduction_size = (int *)malloc(num_axes * sizeof(int));
    int *reduction_strides = (int *)malloc(num_axes * sizeof(int));

    reduction_size[0] = in_shapes[axes[0]];
    reduction_strides[0] = in_strides[axes[0]];

    float *output_data = out->data;

    for (size_t i = 1; i < num_axes; i++)
    {
        if (axes[i] - 1 == axes[i - 1])
        {
            reduction_size[i - 1] *= in_shapes[axes[i]];
            reduction_strides[i - 1] = in_strides[axes[i]];
        }
        else
        {
            reduction_size[i] = in_shapes[axes[i]];
            reduction_strides[i] = in_strides[axes[i]];
        }
    }

    for (size_t i = 0; i < out->size; i++, output_data++)
    {
        *output_data = init_val;
        reduce_contiguous_dim(input->data, output_data, reduction_size, reduction_strides, i, 0, num_axes, op);
    }

    free(reduction_size);
    free(reduction_strides);
}

int* pad_shape(const Array *arr, int target_ndim) 
{
    int *shape = (int *)malloc(target_ndim * sizeof(int));
    if (!shape) {
        fprintf(stderr, "Memory allocation failed in pad_shape\n");
        exit(EXIT_FAILURE);
    }
    int offset = target_ndim - arr->ndim;
    
    // Initialize leading dimensions to 1 for broadcasting
    for(int i=0; i<offset; i++) {
        shape[i] = 1;
    }
    
    // Copy original dimensions
    for(int i=0; i<arr->ndim; i++) {
        shape[offset + i] = arr->shape[i];
    }
    return shape;
}

/* Compute row-major strides for a given shape */
int* compute_strides(const int *shape, int ndim)
{
    int *strides = (int *)malloc(ndim * sizeof(int));
    if (!strides) {
        fprintf(stderr, "Memory allocation failed in compute_stride\n");
        exit(EXIT_FAILURE);
    }
    strides[ndim - 1] = 1;
    for (int i = ndim - 2; i >= 0; i--) {
        strides[i] = strides[i + 1] * shape[i + 1];
    }
    return strides;
}

size_t get_broadcast_offset(size_t b_idx, const int *shape, const int *strides, int num_dims) 
{
    size_t offset = 0;
    size_t temp_idx = b_idx;
    for (int i = 0; i < num_dims; i++) {
        int dim_size = shape[i];
        size_t coord = temp_idx % dim_size;
        offset += coord * strides[i]; // stride=0 for broadcasted dims
        temp_idx /= dim_size;
    }
    return offset;
}

static int prod(const int *arr, int len) 
{
    int p = 1;
    for (int i = 0; i < len; i++) {
        p *= arr[i];
    }
    return p;
}

void invert_permutation(const int *axes, int *inv_axes, int ndim) 
{
    for (int i = 0; i < ndim; i++) {
        inv_axes[axes[i]] = i;
    }
}
#endif