//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include "diag.h"

#include <ostream>

void Diagnostics::report(int line, const char *kind, const std::string &message)
{
    out_ << file_;
    if (line > 0)
        out_ << ':' << line;
    out_ << ": " << kind << ": " << message << '\n';
}

void Diagnostics::error(int line, const std::string &message)
{
    errors_++;
    report(line, "error", message);
}

void Diagnostics::warning(int line, const std::string &message)
{
    report(line, "warning", message);
}
