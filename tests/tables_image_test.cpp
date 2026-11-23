//
// Transitional: the typed tables must hold the words of the original's
// image (src/tables_data.cpp) that the passes read. Goes with the dump.
//
#include <gtest/gtest.h>

#include <map>

#include "pass5.h"
#include "tables.h"
#include "workspace.h"

TEST(tables_image, runtime_names)
{
    Workspace ws;
    restore_rtnames(ws);
    for (unsigned i = 0; i < tables::rtnames.words.size(); i++)
        EXPECT_EQ(ws[02400 + i], tables::rtnames.words[i]) << i;
}

TEST(tables_image, workspace)
{
    Workspace ws;
    std::map<unsigned, Word> init;
    for (size_t i = 0; i < tables::workspace_init_size; i++)
        init[tables::workspace_init[i].addr] = tables::workspace_init[i].value;
    for (unsigned a = Workspace::LOW_BEGIN; a < 06000; a++)
        EXPECT_EQ(ws[a], init.count(a) ? init[a] : 0) << std::oct << a;
}
