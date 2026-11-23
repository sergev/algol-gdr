//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include "version.h"

#include <gtest/gtest.h>

#include <cstring>

TEST(version, not_empty)
{
    EXPECT_GT(std::strlen(version_string()), 0u);
}
