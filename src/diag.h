//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#ifndef ALGOL_DIAG_H
#define ALGOL_DIAG_H

#include <iosfwd>
#include <string>

//
// Compiler messages, GNU style: "file:line: error: Message".
//
class Diagnostics {
public:
    Diagnostics(std::string file, std::ostream &out) : file_(std::move(file)), out_(out) {}

    void error(int line, const std::string &message);
    void warning(int line, const std::string &message);

    unsigned errors() const { return errors_; }

private:
    std::string file_;
    std::ostream &out_;
    unsigned errors_{};

    void report(int line, const char *kind, const std::string &message);
};

#endif
