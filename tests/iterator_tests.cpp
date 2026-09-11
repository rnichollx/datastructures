// Copyright (c) 2026 Ryan P. Nicholl
// SPDX-License-Identifier: Apache-2.0

#include "gtest/gtest.h"

#include <cstddef>
#include <list>
#include <numeric>
#include <string>
#include <type_traits>
#include <vector>

#include "rpnx/dyn_iterator.hpp"
#include "rpnx/iterator.hpp"

namespace
{
    template < typename T >
    struct counting_iterator
    {
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        static int live_count;

        T* ptr = nullptr;

        counting_iterator() : ptr(nullptr)
        {
            ++live_count;
        }

        explicit counting_iterator(T* ptr) : ptr(ptr)
        {
            ++live_count;
        }

        counting_iterator(counting_iterator const& other) : ptr(other.ptr)
        {
            ++live_count;
        }

        counting_iterator(counting_iterator&& other) noexcept : ptr(other.ptr)
        {
            ++live_count;
            other.ptr = nullptr;
        }

        counting_iterator& operator=(counting_iterator const&) = default;
        counting_iterator& operator=(counting_iterator&&) noexcept = default;

        ~counting_iterator()
        {
            --live_count;
        }

        reference operator*() const
        {
            return *ptr;
        }

        counting_iterator& operator++()
        {
            ++ptr;
            return *this;
        }

        counting_iterator& operator--()
        {
            --ptr;
            return *this;
        }

        bool operator==(counting_iterator const& other) const
        {
            return ptr == other.ptr;
        }

        bool operator!=(counting_iterator const& other) const
        {
            return !(*this == other);
        }

        bool operator<(counting_iterator const& other) const
        {
            return ptr < other.ptr;
        }
    };

    template < typename T >
    int counting_iterator< T >::live_count = 0;
} // namespace

TEST(bounded_iterator, basic_traversal_with_neq_only)
{
    std::vector< int > v{1, 2, 3, 4, 5};

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
    std::vector< int > v{10};
    auto end = rpnx::make_bounded_iterator(v.end(), v.end());
    EXPECT_THROW((void)*end, std::out_of_range);
}

TEST(bounded_iterator, increment_at_end_throws)
{
    std::vector< int > v{10};
    auto end = rpnx::make_bounded_iterator(v.end(), v.end());
    EXPECT_THROW(++end, std::out_of_range);
}

TEST(bidirectional_bounded_iterator, basic_traversal)
{
    std::vector< int > v{1, 2, 3, 4, 5};
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
    std::vector< int > v{1, 2, 3};
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

TEST(bidirectional_bounded_iterator, negative_advance_before_begin_throws)
{
    std::vector< int > v{1, 2, 3};
    auto it = rpnx::make_bounded_iterator(v.begin(), v.begin(), v.end());

    EXPECT_THROW(it += -1, std::out_of_range);
    EXPECT_EQ(*it, 1);
}

TEST(dyn_iterator, range_input)
{
    std::vector< std::byte > v{std::byte(1), std::byte(2), std::byte(3), std::byte(4)};
    rpnx::dyn_input_range< std::byte > range(v.begin(), v.end());

    std::vector< std::byte > v2;
    rpnx::dyn_output_iter< std::byte > out(std::back_inserter(v2));

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
    std::vector< int > v{1, 2, 3, 4};
    rpnx::dyn_input_iter< int > iter1(v.begin());
    rpnx::dyn_input_iter< int > iter2(iter1);
    ASSERT_EQ(*iter1, *iter2);

    ++iter1;
    ASSERT_NE(*iter1, *iter2);

    iter2 = iter1;
    ASSERT_EQ(*iter1, *iter2);
}

TEST(dyn_iterator, comparable_input_iterator_comparison)
{
    std::vector< int > v{1, 2, 3, 4};
    rpnx::dyn_comparable_input_iter< int > iter1(v.begin());
    rpnx::dyn_comparable_input_iter< int > iter2(v.begin() + 1);
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
    std::vector< int > v{1, 2, 3, 4};
    rpnx::dyn_input_iter< int > iter(v.begin());
    ASSERT_EQ(*iter, 1);

    ++iter;
    ASSERT_EQ(*iter, 2);

    iter++;
    ASSERT_EQ(*iter, 3);
}

TEST(dyn_iterator, input_iterator_releases_owned_iterator)
{
    int value = 7;
    counting_iterator< int >::live_count = 0;

    {
        counting_iterator< int > source(&value);
        EXPECT_EQ(counting_iterator< int >::live_count, 1);

        {
            rpnx::dyn_input_iter< int > iter(source);
            rpnx::dyn_input_iter< int > copy(iter);
            rpnx::dyn_input_iter< int > assigned;
            assigned = iter;
            EXPECT_EQ(*iter, 7);
            EXPECT_EQ(*copy, 7);
            EXPECT_EQ(*assigned, 7);
            EXPECT_EQ(counting_iterator< int >::live_count, 4);
        }

        EXPECT_EQ(counting_iterator< int >::live_count, 1);
    }

    EXPECT_EQ(counting_iterator< int >::live_count, 0);
}

TEST(dyn_iterator, comparable_input_iterator_releases_owned_iterator)
{
    int value = 11;
    counting_iterator< int >::live_count = 0;

    {
        counting_iterator< int > source(&value);
        {
            rpnx::dyn_comparable_input_iter< int > iter(source);
            rpnx::dyn_comparable_input_iter< int > copy(iter);
            rpnx::dyn_comparable_input_iter< int > assigned;
            assigned = iter;
            EXPECT_EQ(*copy, 11);
            EXPECT_EQ(*assigned, 11);
            EXPECT_EQ(counting_iterator< int >::live_count, 4);
        }

        EXPECT_EQ(counting_iterator< int >::live_count, 1);
    }

    EXPECT_EQ(counting_iterator< int >::live_count, 0);
}

TEST(dyn_bidirectional_input_iter, construct_comparison)
{
    std::vector< int > vec{1, 2, 3, 4, 5};
    rpnx::dyn_bidirectional_input_iter< int > iter(vec.begin());
    EXPECT_EQ(*iter, 1);
}

TEST(dyn_bidirectional_input_iter, copy)
{
    std::list< std::string > lst{"Hello", "World"};
    rpnx::dyn_bidirectional_input_iter< std::string > iter1(lst.begin());
    rpnx::dyn_bidirectional_input_iter< std::string > iter2(iter1);
    EXPECT_EQ(*iter1, "Hello");
    EXPECT_EQ(*iter2, "Hello");
}

TEST(dyn_bidirectional_input_iter, dereference)
{
    std::vector< int > vec{1, 2, 3, 4, 5};
    rpnx::dyn_bidirectional_input_iter< int > iter(vec.begin());
    EXPECT_EQ(*iter, 1);
}

TEST(dyn_bidirectional_input_iter, dereferences_proxy_iterators_by_value)
{
    std::vector< bool > vec{true, false};
    rpnx::dyn_bidirectional_input_iter< bool > iter(vec.begin());

    EXPECT_TRUE(*iter);
    ++iter;
    EXPECT_FALSE(*iter);
}

TEST(dyn_bidirectional_input_iter, preincrement)
{
    std::vector< int > vec{1, 2, 3, 4, 5};
    rpnx::dyn_bidirectional_input_iter< int > iter(vec.begin());
    EXPECT_EQ(*iter, 1);
    ++iter;
    EXPECT_EQ(*iter, 2);
}

TEST(dyn_bidirectional_input_iter, postincrement)
{
    std::vector< int > vec{1, 2, 3, 4, 5};
    rpnx::dyn_bidirectional_input_iter< int > iter(vec.begin());
    EXPECT_EQ(*iter, 1);
    rpnx::dyn_bidirectional_input_iter< int > iter2 = iter++;
    EXPECT_EQ(*iter, 2);
    EXPECT_EQ(*iter2, 1);
}

TEST(dyn_bidirectional_input_iter, predecrement)
{
    std::vector< int > vec{1, 2, 3, 4, 5};
    rpnx::dyn_bidirectional_input_iter< int > iter(vec.end());
    --iter;
    EXPECT_EQ(*iter, 5);
}

TEST(dyn_bidirectional_input_iter, postdecrement)
{
    std::vector< int > vec{1, 2, 3, 4, 5};
    rpnx::dyn_bidirectional_input_iter< int > iter(vec.end());
    rpnx::dyn_bidirectional_input_iter< int > iter2 = iter--;
    EXPECT_EQ(*iter, 5);
    EXPECT_EQ(iter2, vec.end());
    --iter2;
    EXPECT_EQ(*--iter2, 4);
}

TEST(dyn_bidirectional_input_iter, noteq)
{
    std::vector< int > vec{1, 2, 3, 4, 5};
    rpnx::dyn_bidirectional_input_iter< int > iter1(vec.begin());
    rpnx::dyn_bidirectional_input_iter< int > iter2(vec.begin() + 2);
    EXPECT_TRUE(iter1 != iter2);
}

TEST(dyn_bidirectional_input_iter, eq)
{
    std::vector< int > vec{1, 2, 3, 4, 5};
    rpnx::dyn_bidirectional_input_iter< int > iter1(vec.begin());
    rpnx::dyn_bidirectional_input_iter< int > iter2(vec.begin());
    rpnx::dyn_bidirectional_input_iter< int > iter3(vec.begin() + 1);
    EXPECT_TRUE(iter1 == iter2);
    EXPECT_FALSE(iter1 == iter3);
}

TEST(dyn_bidirectional_input_iter, inequality_comparison)
{
    std::vector< int > vec{1, 2, 3, 4, 5};
    rpnx::dyn_bidirectional_input_iter< int > iter1(vec.begin());
    rpnx::dyn_bidirectional_input_iter< int > iter2(vec.begin());
    rpnx::dyn_bidirectional_input_iter< int > iter3(vec.begin() + 1);
    EXPECT_FALSE(iter1 != iter2);
    EXPECT_TRUE(iter1 != iter3);
}

TEST(dyn_bidirectional_input_iter, iterators_with_different_underlying_types)
{
    std::vector< int > vec{1, 2, 3, 4, 5};
    std::list< int > lst{1, 2, 3, 4, 5};
    rpnx::dyn_bidirectional_input_iter< int > iter1(vec.begin());
    rpnx::dyn_bidirectional_input_iter< int > iter2(lst.begin());
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

    std::vector< NonDefaultConstructible > vec;
    vec.emplace_back(1);
    vec.emplace_back(2);
    vec.emplace_back(3);

    rpnx::dyn_bidirectional_input_iter< NonDefaultConstructible > iter(vec.begin());
    EXPECT_EQ((*iter).value, 1);
    ++iter;
    EXPECT_EQ((*iter).value, 2);
}

TEST(dyn_bidirectional_input_iter, releases_owned_iterator)
{
    int value = 13;
    counting_iterator< int >::live_count = 0;

    {
        counting_iterator< int > source(&value);
        {
            rpnx::dyn_bidirectional_input_iter< int > iter(source);
            rpnx::dyn_bidirectional_input_iter< int > copy(iter);
            rpnx::dyn_bidirectional_input_iter< int > assigned;
            assigned = iter;
            EXPECT_EQ(*copy, 13);
            EXPECT_EQ(*assigned, 13);
            EXPECT_EQ(counting_iterator< int >::live_count, 4);
        }

        EXPECT_EQ(counting_iterator< int >::live_count, 1);
    }

    EXPECT_EQ(counting_iterator< int >::live_count, 0);
}
