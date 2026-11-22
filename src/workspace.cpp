#include "workspace.h"

#include <stdexcept>
#include <string>

#include "tables.h"

void Workspace::reset()
{
    low_.fill(0);
    comtab_.fill(0);
    for (size_t i = 0; i < tables::workspace_init_size; i++)
        (*this)[tables::workspace_init[i].addr] = tables::workspace_init[i].value;
}

Word *Workspace::word(unsigned addr)
{
    if (addr >= LOW_BEGIN && addr < LOW_END)
        return &low_[addr - LOW_BEGIN];
    if (addr >= COMTAB && addr < COMTAB_END)
        return &comtab_[addr - COMTAB];
    throw std::out_of_range("workspace address " + std::to_string(addr) + " out of range");
}
