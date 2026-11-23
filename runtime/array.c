//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// Arrays. A descriptor is int64 words: the data pointer, the number of
// elements, the number of dimensions, then the lower and upper bound of each
// dimension. Elements are
// 8-byte cells, row-major.
//
#include <stdlib.h>
#include <string.h>

#include "algol.h"

// Element types, as ast::Type.
enum { T_INTEGER, T_REAL, T_BOOLEAN, T_STRING, T_BITS };

void algol_array_alloc(int64_t *desc, int64_t dims)
{
    int64_t count = 1;
    desc[2] = dims;
    for (int64_t k = 0; k < dims; k++) {
        int64_t lo = desc[3 + 2 * k], hi = desc[4 + 2 * k];
        if (hi < lo)
            count = 0;
        else
            count *= hi - lo + 1;
    }
    void *data = calloc(count ? count : 1, 8);
    if (!data)
        algol_error("No memory for an array of %lld elements", (long long)count);
    desc[0] = (int64_t)(intptr_t)data;
    desc[1] = count;
}

void algol_array_free(int64_t *desc)
{
    free((void *)(intptr_t)desc[0]);
    desc[0] = 0;
}

void algol_check_index(const int64_t *desc, int64_t dim, int64_t i)
{
    int64_t lo = desc[3 + 2 * dim], hi = desc[4 + 2 * dim];
    if (i < lo || i > hi)
        algol_error("Subscript %lld out of bounds [%lld:%lld]", (long long)i, (long long)lo,
                    (long long)hi);
}

// A new array with the bounds and elements of src: a value parameter.
int64_t *algol_array_copy(const int64_t *src, int64_t dims)
{
    size_t words  = 2 + 2 * dims;
    int64_t *desc = malloc(words * 8);
    if (!desc)
        algol_error("No memory for an array");
    memcpy(desc, src, words * 8);
    algol_array_alloc(desc, dims);
    memcpy((void *)(intptr_t)desc[0], (void *)(intptr_t)src[0], src[1] * 8);
    return desc;
}

// A copy made by algol_array_copy, freed.
void algol_array_release(int64_t *desc)
{
    algol_array_free(desc);
    free(desc);
}

void algol_print_array(const int64_t *desc, int64_t type)
{
    const int64_t *data = (const int64_t *)(intptr_t)desc[0];
    for (int64_t i = 0; i < desc[1]; i++) {
        switch (type) {
        case T_INTEGER:
            algol_print_integer(data[i]);
            break;
        case T_REAL: {
            double v;
            memcpy(&v, &data[i], 8);
            algol_print_real(v);
            break;
        }
        case T_BOOLEAN:
            algol_print_boolean(*(const unsigned char *)&data[i] != 0);
            break;
        case T_STRING:
            algol_print_string((const char *)(intptr_t)data[i]);
            break;
        default:
            algol_print_bits(data[i]);
        }
    }
}
