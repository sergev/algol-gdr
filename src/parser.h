//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#ifndef ALGOL_PARSER_H
#define ALGOL_PARSER_H

#include <string>

#include "syntax.h"

class Diagnostics;
class Listing;

//
// Parse a program. Syntax errors are reported; after one the parser goes
// on at the next statement or declaration, leaving an Error statement.
// With a listing, each card read is printed in it.
//
syn::Program parse(const std::string &source, Diagnostics &diag, Listing *listing = nullptr);

#endif
