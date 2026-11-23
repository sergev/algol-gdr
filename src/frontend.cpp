//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include "frontend.h"

#include "checker.h"
#include "diag.h"
#include "listing.h"
#include "parser.h"

std::optional<ast::Program> front_end(const std::string &source, Diagnostics &diag,
                                      Listing *listing)
{
    if (listing) {
        listing->print("A L G O L - Г Д P");
        listing->print("    (7.01.82)");
        listing->blank();
    }
    auto tree    = parse(source, diag, listing);
    auto program = check(tree, diag);
    if (listing)
        listing->finish();
    if (diag.errors())
        return std::nullopt;
    return program;
}
