//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#ifndef ALGOL_CHECKER_H
#define ALGOL_CHECKER_H

#include "ast.h"
#include "syntax.h"

class Diagnostics;

//
// The AST of a parsed program: names resolved by the scopes of Algol, the
// types checked, the conversions made explicit. Errors are reported; the
// AST is complete only when there were none.
//
ast::Program check(const syn::Program &program, Diagnostics &diag);

#endif
