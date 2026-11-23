//
// Transitional: the typed tables must hold the words of the original's
// image (src/tables_data.cpp) that the passes read. Goes with the dump.
//
#include <gtest/gtest.h>

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
