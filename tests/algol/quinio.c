//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// The _code procedures of quinio for a native run: C for the Fortran and
// Madlen of quinio.run, following them line by line.
//
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// read (54, 200) N with format(I2): the first two columns of a line.
static int64_t read_i2(void)
{
    char buf[256];
    fflush(stdout);
    if (!fgets(buf, sizeof(buf), stdin)) {
        fprintf(stderr, "End of the input\n");
        exit(1);
    }
    char two[3] = { buf[0], buf[0] && buf[0] != '\n' ? buf[1] : 0, 0 };
    if (two[1] == '\n')
        two[1] = 0;
    return strtoll(two, NULL, 10);
}

int64_t xeen(const int64_t *mask)
{
    int64_t n;
    switch (*mask) {
    case 31:
        do {
            printf(" Enter column: \n");
            n = read_i2();
        } while (n <= 0 || n > 31);
        return n;
    case 992:
        do {
            printf(" Enter row: \n");
            n = read_i2();
        } while (n <= 0 || n > 31);
        return n * 32;
    case 33554432:
        printf(" Who plays first, you (1), or I (0)?\n");
        n = read_i2();
        if (n == 0 || n == 1)
            return n * 33554432;
        // As the Fortran: on to 3.
        // fall through
    case -1:
        do {
            printf(" Are you sure (1), or not (0)?\n");
            n = read_i2();
        } while (n != 0 && n != 1);
        return n - 1;
    default:
        printf(" Unexpected XEEN mask %10lld\n", (long long)*mask);
        exit(0);
    }
}

// Free memory in words, from the Madlen code: a field of 19.
int64_t available(void)
{
    return 15400;
}

// integer bord(nt, nt), indexed as the Algol program sees it: bord(i, j) is
// BORD[i, j]. algc stores arrays row-major; the original, column-major as
// Fortran, where the Fortran code had the same view.
void draw(int64_t *bord, const int64_t *nt, const int64_t *znr, const int64_t *color,
          const int64_t *row, const int64_t *col)
{
    static const char l[4] = { 'x', 'o', '.', '*' };
    int64_t n = *nt;
#define BORD(i, j) bord[((i) - 1) * n + ((j) - 1)]
    printf(" Move%3lld%50lld-%2lld\n", (long long)*znr, (long long)*row, (long long)*col);
    printf("         ");
    for (int64_t i = 1; i <= n; i++)
        printf("%3lld", (long long)i);
    printf("\n");
    BORD(*row, *col) = *color;
    for (int64_t i = 1; i <= n; i++) {
        printf("    %3lld  ", (long long)i);
        for (int64_t j = 1; j <= n; j++)
            printf("  %c", l[BORD(i, j)]);
        printf("\n");
    }
}
