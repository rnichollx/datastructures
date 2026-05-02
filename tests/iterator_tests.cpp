// Copyright (c) 2026 Ryan P. Nicholl
// SPDX-License-Identifier: Apache-2.0

#include "gtest/gtest.h"

#include <cstddef>
#include <list>
#include <numeric>
#include <string>
#include <vector>

#include "rpnx/dyn_iterator.hpp"
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



TEST(bidirectional_bounded_iterator, basic_traversal)
{
    std::vector<int> v{1, 2, 3, 4, 5};
    auto it = rpnx::make_bounded_iterator(v.begin(), v.begin(), v.end());
    auto end = rpnx::make_bounded_iterator(v.end(), v.begin(), v.end());

    int sum = 0;
    while (it != end)
    {
        sum += *it;
        ++it;
    }
    EXPECT_EQ(sum, 15);
}

TEST(bidirectional_bounded_iterator, reverse_traversal)
{
    std::vector<int> v{1, 2, 3};
    auto it = rpnx::make_bounded_iterator(v.end(), v.begin(), v.end());
    auto begin = rpnx::make_bounded_iterator(v.begin(), v.begin(), v.end());

    int sum = 0;
    while (it != begin)
    {
        --it;
        sum += *it;
    }
    EXPECT_EQ(sum, 6);
}

TEST(dyn_iterator, range_input)
{
    std::vector<std::byte> v{std::byte(1), std::byte(2), std::byte(3), std::byte(4)};
    rpnx::dyn_input_range<std::byte> range(v.begin(), v.end());

    std::vector<std::byte> v2;
    rpnx::dyn_output_iter<std::byte> out(std::back_inserter(v2));

    for (auto x : range)
    {
        *out++ = x;
    }

    ASSERT_EQ(v, v2);
    v2.push_back(std::byte(5));
    ASSERT_NE(v, v2);
}

TEST(dyn_iterator, input_iterator_copy_constructor_and_assignment)
{
    std::vector<int> v{1, 2, 3, 4};
    rpnx::dyn_input_iter<int> iter1(v.begin());
    rpnx::dyn_input_iter<int> iter2(iter1);
    ASSERT_EQ(*iter1, *iter2);

    ++iter1;
    ASSERT_NE(*iter1, *iter2);

    iter2 = iter1;
    ASSERT_EQ(*iter1, *iter2);
}

TEST(dyn_iterator, comparable_input_iterator_comparison)
{
    std::vector<int> v{1, 2, 3, 4};
    rpnx::dyn_comparable_input_iter<int> iter1(v.begin());
    rpnx::dyn_comparable_input_iter<int> iter2(v.begin() + 1);
    ASSERT_TRUE(iter1 < iter2);
    ASSERT_FALSE(iter2 < iter1);
    ASSERT_TRUE(iter1 != iter2);
    ASSERT_FALSE(iter1 == iter2);

    ++iter1;
    ASSERT_FALSE(iter1 < iter2);
    ASSERT_FALSE(iter2 < iter1);
    ASSERT_TRUE(iter1 == iter2);
    ASSERT_FALSE(iter1 != iter2);
}

TEST(dyn_iterator, input_iterator_advance)
{
    std::vector<int> v{1, 2, 3, 4};
    rpnx::dyn_input_iter<int> iter(v.begin());
    ASSERT_EQ(*iter, 1);

    ++iter;
    ASSERT_EQ(*iter, 2);

    iter++;
    ASSERT_EQ(*iter, 3);
}

TEST(dyn_bidirectional_input_iter, construct_comparison)
{
    std::vector<int> vec{1, 2, 3, 4, 5};
    rpnx::dyn_bidirectional_input_iter<int> iter(vec.begin());
    EXPECT_EQ(*iter, 1);
}

TEST(dyn_bidirectional_input_iter, copy)
{
    std::list<std::string> lst{"Hello", "World"};
    rpnx::dyn_bidirectional_input_iter<std::string> iter1(lst.begin());
    rpnx::dyn_bidirectional_input_iter<std::string> iter2(iter1);
    EXPECT_EQ(*iter1, "Hello");
    EXPECT_EQ(*iter2, "Hello");
}

TEST(dyn_bidirectional_input_iter, dereference)
{
    std::vector<int> vec{1, 2, 3, 4, 5};
    rpnx::dyn_bidirectional_input_iter<int> iter(vec.begin());
    EXPECT_EQ(*iter, 1);
}

TEST(dyn_bidirectional_input_iter, preincrement)
{
    std::vector<int> vec{1, 2, 3, 4, 5};
    rpnx::dyn_bidirectional_input_iter<int> iter(vec.begin());
    EXPECT_EQ(*iter, 1);
    ++iter;
    EXPECT_EQ(*iter, 2);
}

TEST(dyn_bidirectional_input_iter, postincrement)
{
    std::vector<int> vec{1, 2, 3, 4, 5};
    rpnx::dyn_bidirectional_input_iter<int> iter(vec.begin());
    EXPECT_EQ(*iter, 1);
    rpnx::dyn_bidirectional_input_iter<int> iter2 = iter++;
    EXPECT_EQ(*iter, 2);
    EXPECT_EQ(*iter2, 1);
}

TEST(dyn_bidirectional_input_iter, predecrement)
{
    std::vector<int> vec{1, 2, 3, 4, 5};
    rpnx::dyn_bidirectional_input_iter<int> iter(vec.end());
    --iter;
    EXPECT_EQ(*iter, 5);
}

TEST(dyn_bidirectional_input_iter, postdecrement)
{
    std::vector<int> vec{1, 2, 3, 4, 5};
    rpnx::dyn_bidirectional_input_iter<int> iter(vec.end());
    rpnx::dyn_bidirectional_input_iter<int> iter2 = iter--;
    EXPECT_EQ(*iter, 5);
    EXPECT_EQ(iter2, vec.end());
    --iter2;
    EXPECT_EQ(*--iter2, 4);
}

TEST(dyn_bidirectional_input_iter, noteq)
{
    std::vector<int> vec{1, 2, 3, 4, 5};
    rpnx::dyn_bidirectional_input_iter<int> iter1(vec.begin());
    rpnx::dyn_bidirectional_input_iter<int> iter2(vec.begin() + 2);
    EXPECT_TRUE(iter1 != iter2);
}

TEST(dyn_bidirectional_input_iter, eq)
{
    std::vector<int> vec{1, 2, 3, 4, 5};
    rpnx::dyn_bidirectional_input_iter<int> iter1(vec.begin());
    rpnx::dyn_bidirectional_input_iter<int> iter2(vec.begin());
    rpnx::dyn_bidirectional_input_iter<int> iter3(vec.begin() + 1);
    EXPECT_TRUE(iter1 == iter2);
    EXPECT_FALSE(iter1 == iter3);
}

TEST(dyn_bidirectional_input_iter, inequality_comparison)
{
    std::vector<int> vec{1, 2, 3, 4, 5};
    rpnx::dyn_bidirectional_input_iter<int> iter1(vec.begin());
    rpnx::dyn_bidirectional_input_iter<int> iter2(vec.begin());
    rpnx::dyn_bidirectional_input_iter<int> iter3(vec.begin() + 1);
    EXPECT_FALSE(iter1 != iter2);
    EXPECT_TRUE(iter1 != iter3);
}

TEST(dyn_bidirectional_input_iter, iterators_with_different_underlying_types)
{
    std::vector<int> vec{1, 2, 3, 4, 5};
    std::list<int> lst{1, 2, 3, 4, 5};
    rpnx::dyn_bidirectional_input_iter<int> iter1(vec.begin());
    rpnx::dyn_bidirectional_input_iter<int> iter2(lst.begin());
    EXPECT_FALSE(iter1 == iter2);
    ++iter1;
    ++iter2;
    EXPECT_FALSE(iter1 == iter2);
    EXPECT_TRUE(*iter1 == *iter2);
}

TEST(dyn_bidirectional_input_iter, iterators_with_non_default_constructible_type)
{
    struct NonDefaultConstructible
    {
        NonDefaultConstructible() = delete;

        explicit NonDefaultConstructible(int x) : value(x)
        {
        }

        int value;
    };

    std::vector<NonDefaultConstructible> vec;
    vec.emplace_back(1);
    vec.emplace_back(2);
    vec.emplace_back(3);

    rpnx::dyn_bidirectional_input_iter<NonDefaultConstructible> iter(vec.begin());
    EXPECT_EQ((*iter).value, 1);
    ++iter;
    EXPECT_EQ((*iter).value, 2);
}
