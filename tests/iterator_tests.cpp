// Copyright (c) 2026 Ryan P. Nicholl
// SPDX-License-Identifier: Apache-2.0

#include "gtest/gtest.h"

#include <vector>
#include <numeric>

#include "rpnx/iterator.hpp"

TEST(bounded_iterator, basic_traversal_with_neq_only)
{
    std::vector<int> v{1, 2, 3, 4, 5};

    auto it = rpnx::make_bounded_iterator(v.begin(), v.end());
    auto end = rpnx::make_bounded_iterator(v.end(), v.end());

    int sum = 0;
    while (it != end)
    {
        sum += *it;
        ++it;
    }
    EXPECT_EQ(sum, 15);
}

TEST(bounded_iterator, deref_at_end_throws)
{
    std::vector<int> v{10};
    auto end = rpnx::make_bounded_iterator(v.end(), v.end());
    EXPECT_THROW((void)*end, std::out_of_range);
}

TEST(bounded_iterator, increment_at_end_throws)
{
    std::vector<int> v{10};
    auto end = rpnx::make_bounded_iterator(v.end(), v.end());
    EXPECT_THROW(++end, std::out_of_range);
}
