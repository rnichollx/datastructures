// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>

#include "gtest/gtest.h"
#include "test_utils.hpp"

using namespace rpnx::testing;

TEST(test_utils, copy_thrower_throws_at_limit)
{
    int count = 0;
    copy_thrower ct1(&count, 2);

    // First copy should succeed
    copy_thrower ct2 = ct1;
    EXPECT_EQ(count, 1);

    // Second copy should succeed
    copy_thrower ct3 = ct2;
    EXPECT_EQ(count, 2);

    // Third copy should throw
    EXPECT_THROW(copy_thrower ct4 = ct3, copy_limit_exceeded);
    EXPECT_EQ(count, 2);
}

TEST(test_utils, copy_thrower_assignment_throws_at_limit)
{
    int count = 0;
    copy_thrower ct1(&count, 1);
    copy_thrower ct2;

    // First copy (via assignment) should succeed
    ct2 = ct1;
    EXPECT_EQ(count, 1);

    // Second copy (via assignment) should throw
    copy_thrower ct3;
    EXPECT_THROW(ct3 = ct1, copy_limit_exceeded);
    EXPECT_EQ(count, 1);
}

TEST(test_utils, copy_thrower_move_does_not_throw)
{
    int count = 0;
    copy_thrower ct1(&count, 0); // Should throw on first copy

    // Move should succeed
    copy_thrower ct2 = std::move(ct1);
    EXPECT_EQ(count, 0);
}
