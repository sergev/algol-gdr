//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#ifndef ALGOL_PRINTM_H
#define ALGOL_PRINTM_H

#include <string>
#include <vector>

#include "stdarray.h"

//
// A/PRINTM (sub-overlay at 30355): the object listing of *FULL, as rows of
// one column. The original lays them out in pages of 4 columns of 64 rows;
// see doc/passes.md.
//
std::vector<std::string> printm_rows(const StdArray &obj);

// The value of a constant word as A/PRINTM prints it.
std::string printm_real(Word w); // AG31544: E format, 12 digits
std::string printm_text(Word w); // ISO, 6 characters

#endif
