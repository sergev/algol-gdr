//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include "diag.h"

#include <ostream>

void Diagnostics::report(Pos pos, const char *kind, const std::string &message)
{
    out_ << file_;
    if (pos.line > 0)
        out_ << ':' << pos.line;
    if (pos.line > 0 && pos.col > 0)
        out_ << ':' << pos.col;
    out_ << ": " << kind << ": " << message << '\n';
}

void Diagnostics::error(Pos pos, const std::string &message)
{
    errors_++;
    report(pos, "error", message);
}

void Diagnostics::warning(Pos pos, const std::string &message)
{
    report(pos, "warning", message);
}
