//
// The workspace before the passes run, as `ADDR VALUE` lines in octal (the
// nonzero words): the start memory of scripts/capture-streams.py.
//
#include <cstdio>

#include "workspace.h"

int main()
{
    Workspace ws;
    for (unsigned a = Workspace::LOW_BEGIN; a < Workspace::LOW_END; a++)
        if (ws[a])
            std::printf("%05o %016llo\n", a, (unsigned long long)ws[a]);
}
