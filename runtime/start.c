//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// main of an Algol program: alone in its file, so that a program with a main
// of its own (in C) can link Algol units with the library.
//
#include "algol.h"

int main(void)
{
    algol_main();
    algol_stop();
}
