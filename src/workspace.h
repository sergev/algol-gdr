#ifndef ALGOL_WORKSPACE_H
#define ALGOL_WORKSPACE_H

#include <array>

#include "stdarray.h"

//
// Shared data of the passes as 48-bit words at their original addresses:
// 01736-05777 (tables, D-list) and 27245-27644 (the 'COMMON' and
// external-symbol table). See doc/tables.md.
//
class Workspace {
public:
    static constexpr unsigned LOW_BEGIN  = 01736;
    static constexpr unsigned LOW_END    = 06000;
    static constexpr unsigned COMTAB     = 027245;
    static constexpr unsigned COMTAB_END = 027645;

    Workspace() { reset(); }

    // Contents before the passes run: the SET directives of the driver.
    void reset();

    Word &operator[](unsigned addr) { return *word(addr); }
    Word operator[](unsigned addr) const { return *const_cast<Workspace *>(this)->word(addr); }

    static bool contains(unsigned addr)
    {
        return (addr >= LOW_BEGIN && addr < LOW_END) || (addr >= COMTAB && addr < COMTAB_END);
    }

private:
    std::array<Word, LOW_END - LOW_BEGIN> low_{};
    std::array<Word, COMTAB_END - COMTAB> comtab_{};

    Word *word(unsigned addr);
};

#endif
