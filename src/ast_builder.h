//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#ifndef ALGOL_AST_BUILDER_H
#define ALGOL_AST_BUILDER_H

#include "ast.h"
#include "item.h"

struct Context;

//
// The AST of a program from the pass-2 stream and the tables after pass 2.
// Throws std::runtime_error on an item it does not know.
//
ast::Program build_ast(const Context &ctx, const Stream &stream);

#endif
