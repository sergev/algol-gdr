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
// A place in the source: the line, and the column in characters from 1 (0:
// not known). It is the line where a number is wanted.
struct Pos {
    int line{};
    int col{};

    Pos() = default;
    // cppcheck-suppress noExplicitConstructor
    Pos(int l, int c = 0) : line(l), col(c) {}
    operator int() const { return line; }
};

class Diagnostics {
public:
    Diagnostics(std::string file, std::ostream &out) : file_(std::move(file)), out_(out) {}

    void error(Pos pos, const std::string &message);
    void warning(Pos pos, const std::string &message);

    unsigned errors() const { return errors_; }

private:
    std::string file_;
    std::ostream &out_;
    unsigned errors_{};

    void report(Pos pos, const char *kind, const std::string &message);
};

#endif
