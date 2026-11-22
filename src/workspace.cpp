#include "workspace.h"

#include <stdexcept>
#include <string>

#include "tables.h"

void Workspace::reset()
{
    low_.fill(0);
    comtab_.fill(0);
    drums_.clear();
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

Word &Workspace::drum(unsigned unit, unsigned zone, unsigned offset)
{
    if (offset >= 1024)
        throw std::out_of_range("drum offset " + std::to_string(offset) + " out of range");
    return drums_[{ unit, zone }][offset]; // value-initialized on insertion
}
