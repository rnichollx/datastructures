// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>

#include "failing_allocator.hpp"
#include "rpnx/annex.hpp"
#include "tracking_allocator.hpp"
#include "gtest/gtest.h"

#include <compare>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    struct large_value
    {
        char bytes[1024] = {};
    };

    struct counted
    {
        static int live_count;

        int value = 0;

        counted(int value) : value(value)
        {
            ++live_count;
        }

        counted(counted const& other) : value(other.value)
        {
            ++live_count;
        }

        counted(counted&& other) noexcept : value(other.value)
        {
            ++live_count;
            other.value = -1;
        }

        counted& operator=(counted const&) = default;
        counted& operator=(counted&&) noexcept = default;

        ~counted()
        {
            --live_count;
        }
    };

    int counted::live_count = 0;
} // namespace

static_assert(sizeof(rpnx::annex< large_value >) < sizeof(std::optional< large_value >));
static_assert(std::is_same_v< decltype(*std::declval< rpnx::annex< int >& >()), int& >);
static_assert(std::is_same_v< decltype(*std::declval< rpnx::annex< int > const& >()), int const& >);
static_assert(std::is_same_v< decltype(std::declval< rpnx::annex< int >& >().value()), int& >);
static_assert(std::is_same_v< decltype(std::declval< rpnx::annex< int > const& >().value()), int const& >);
static_assert(std::is_same_v< decltype(std::declval< rpnx::annex< int >&& >().value()), int&& >);
static_assert(std::is_same_v< decltype(std::declval< rpnx::annex< int > const&& >().value()), int const&& >);
static_assert(std::is_same_v< decltype(std::declval< rpnx::annex< int > const& >() <=> std::declval< rpnx::annex< int > const& >()), std::strong_ordering >);

TEST(annex, default_constructed_is_empty)
{
    rpnx::annex< int > value;

    EXPECT_FALSE(value.has_value());
    EXPECT_FALSE(value);
    EXPECT_EQ(value, std::nullopt);
    EXPECT_THROW(value.value(), std::bad_optional_access);
    EXPECT_EQ(value.value_or(42), 42);
}

TEST(annex, constructs_and_accesses_value)
{
    rpnx::annex< std::string > value("abc");

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, "abc");
    EXPECT_EQ(value->size(), 3U);

    *value = "def";
    EXPECT_EQ(value.value(), "def");
}

TEST(annex, emplace_replaces_value_and_reset_destroys_it)
{
    counted::live_count = 0;
    rpnx::annex< counted > value(std::in_place, 7);

    EXPECT_EQ(counted::live_count, 1);
    EXPECT_EQ(value->value, 7);

    counted& replacement = value.emplace(9);
    EXPECT_EQ(counted::live_count, 1);
    EXPECT_EQ(replacement.value, 9);

    value.reset();
    EXPECT_FALSE(value.has_value());
    EXPECT_EQ(counted::live_count, 0);
}

TEST(annex, copy_constructs_and_move_transfers_pointer)
{
    rpnx::annex< std::string > original("copy");
    rpnx::annex< std::string > copied(original);

    ASSERT_TRUE(copied);
    EXPECT_EQ(*copied, "copy");

    rpnx::annex< std::string > moved(std::move(original));
    ASSERT_TRUE(moved);
    EXPECT_EQ(*moved, "copy");
    EXPECT_FALSE(original.has_value());
}

TEST(annex, supports_move_only_values)
{
    rpnx::annex< std::unique_ptr< int > > value(std::make_unique< int >(13));

    ASSERT_TRUE(value);
    EXPECT_EQ(**value, 13);

    std::unique_ptr< int > moved = std::move(value).value();
    ASSERT_TRUE(moved);
    EXPECT_EQ(*moved, 13);
}

TEST(annex, assignment_matches_optional_state_transitions)
{
    rpnx::annex< int > value;

    value = 11;
    ASSERT_TRUE(value);
    EXPECT_EQ(*value, 11);

    value = std::nullopt;
    EXPECT_FALSE(value);

    rpnx::annex< int > other(17);
    value = other;
    ASSERT_TRUE(value);
    EXPECT_EQ(*value, 17);

    other.reset();
    value = other;
    EXPECT_FALSE(value);
}

TEST(annex, comparisons_match_optional_ordering)
{
    rpnx::annex< int > empty;
    rpnx::annex< int > low(1);
    rpnx::annex< int > high(2);

    EXPECT_EQ(empty, std::nullopt);
    EXPECT_NE(low, std::nullopt);
    EXPECT_LT(empty, low);
    EXPECT_LT(low, high);
    EXPECT_GT(high, 1);
    EXPECT_LE(low, 1);
    EXPECT_GE(2, low);
}

TEST(annex, spaceship_comparisons_match_optional_ordering)
{
    rpnx::annex< int > empty;
    rpnx::annex< int > low(1);
    rpnx::annex< int > high(2);

    EXPECT_EQ(empty <=> std::nullopt, std::strong_ordering::equal);
    EXPECT_EQ(std::nullopt <=> empty, std::strong_ordering::equal);
    EXPECT_EQ(empty <=> low, std::strong_ordering::less);
    EXPECT_EQ(low <=> empty, std::strong_ordering::greater);
    EXPECT_EQ(low <=> high, std::strong_ordering::less);
    EXPECT_EQ(high <=> low, std::strong_ordering::greater);
    EXPECT_EQ(low <=> 1, std::strong_ordering::equal);
    EXPECT_EQ(low <=> 2, std::strong_ordering::less);
    EXPECT_EQ(2 <=> low, std::strong_ordering::greater);
}

TEST(annex, initializer_list_construction)
{
    rpnx::annex< std::vector< int > > value(std::in_place, {1, 2, 3});

    ASSERT_TRUE(value);
    EXPECT_EQ(value->size(), 3U);
    EXPECT_EQ((*value)[1], 2);
}

TEST(annex, allocator_is_used_for_storage)
{
    using allocator_type = testutils::tracking_allocator< int >;

    rpnx::annex< int, allocator_type > value(std::allocator_arg, allocator_type(7), std::in_place, 5);

    ASSERT_TRUE(value);
    EXPECT_EQ(*value, 5);
    EXPECT_EQ(value.get_allocator().id, 7);

    rpnx::annex< int, allocator_type > copied(value);
    EXPECT_EQ(copied.get_allocator().id, 107);
    EXPECT_EQ(*copied, 5);
}

TEST(annex, allocation_failure_leaves_object_empty)
{
    using allocator_type = testutils::failing_allocator< int >;

    allocator_type::allocation_count = 0;
    allocator_type::fail_at = 0;
    allocator_type::total_allocations = 0;

    rpnx::annex< int, allocator_type > value;

    EXPECT_THROW(value.emplace(3), std::bad_alloc);
    EXPECT_FALSE(value.has_value());
    EXPECT_EQ(allocator_type::total_allocations, 0);

    allocator_type::fail_at = 999;
}
