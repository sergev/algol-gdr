//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include "listing.h"

#include <ostream>

std::string rtrim(std::string s)
{
    s.erase(s.find_last_not_of(' ') + 1);
    return s;
}

void Listing::print(const std::string &text)
{
    out_ << rtrim(text) << '\n';
}

void Listing::blank()
{
    out_ << '\n';
}

void Listing::finish()
{
    out_ << '\n';
}
