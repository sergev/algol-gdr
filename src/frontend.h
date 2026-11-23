//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#ifndef ALGOL_FRONTEND_H
#define ALGOL_FRONTEND_H

#include <optional>
#include <string>

#include "ast.h"

class Diagnostics;
class Listing;

//
// Parse and check a program. With a listing: the banner, each line as it is
// read, an empty line at the end. Nothing when there are errors.
//
std::optional<ast::Program> front_end(const std::string &source, Diagnostics &diag,
                                      Listing *listing = nullptr);

#endif
