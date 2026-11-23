//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// The library procedures of bozp_ybyb for a native run (dubna has them in
// *library:1,3). rndm gives the values the original's generator gave in
// this run; BOZP and YBYB sort an array up and down. BOZP and YBYB are
// _algol procedures: the Algol convention of doc/codegen.md.
//
#include <stdint.h>
#include <stdlib.h>

double rndm(const double *seed)
{
    static const double values[] = {
        0.943, 0.976, 0.069, 0.428, 0.557, 0.324, 0.223, 0.854, 0.786, 0.850,
    };
    static int next;
    (void)seed;
    return values[next++ % 10];
}

typedef void *(*Thunk)(void *env);

// The cell of argument k: a pair {code, env} in args.
static void *cell(void **args, int k)
{
    return ((Thunk)args[2 * k])(args[2 * k + 1]);
}

static int up(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static int down(const void *a, const void *b)
{
    return up(b, a);
}

// BOZP(A, N): A[1:N] in increasing order; YBYB: decreasing.
static void sort(void **args, int (*order)(const void *, const void *))
{
    const int64_t *desc = cell(args, 0); // data, count, dims, bounds
    int64_t n           = *(const int64_t *)cell(args, 1);
    qsort((double *)(intptr_t)desc[0], (size_t)n, sizeof(double), order);
}

int64_t BOZP(void *sl, void **args)
{
    (void)sl;
    sort(args, up);
    return 0;
}

int64_t YBYB(void *sl, void **args)
{
    (void)sl;
    sort(args, down);
    return 0;
}
