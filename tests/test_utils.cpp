#include <gtest/gtest.h>

#include "utils.h"

TEST(UtilsTest, TrimWhitespaceOnlyStrings)
{
    EXPECT_EQ(utils::trimLeft(" \t"), "");
    EXPECT_EQ(utils::trimRight(" \t\r"), "");
    EXPECT_EQ(utils::trim(" \t\r"), "");
}

TEST(UtilsTest, TrimKeepsTextBetweenWhitespace)
{
    EXPECT_EQ(utils::trim(" \t command --flag \t\r"), "command --flag");
}
