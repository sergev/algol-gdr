#ifndef ALGOL_DUMP_H
#define ALGOL_DUMP_H

#include <iosfwd>

#include "stdarray.h"

//
// Print object module in readable form: header, code, constants, tables.
//
void dump_stdarray(const StdArray &obj, std::ostream &out);

#endif
