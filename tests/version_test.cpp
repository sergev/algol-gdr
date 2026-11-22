#include <gtest/gtest.h>

#include <cstring>

#include "version.h"

TEST(version, not_empty)
{
    EXPECT_GT(std::strlen(version_string()), 0u);
}
