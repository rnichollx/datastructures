// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// ai-generated tests

#include "rpnx/set.hpp"
#include "gtest/gtest.h"

#include <compare>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

static_assert(std::is_same_v< decltype(std::declval< rpnx::set< int > const& >() <=> std::declval< rpnx::set< int > const& >()), std::strong_ordering >);

TEST(set, initializer_list_sorts_and_deduplicates_values)
{
    rpnx::set< int > values{3, 1, 2, 2};
    std::vector< int > ordered(values.begin(), values.end());

    EXPECT_EQ(values.size(), 3U);
    EXPECT_EQ(ordered, (std::vector< int >{1, 2, 3}));
    EXPECT_TRUE(values.contains(2));
    EXPECT_FALSE(values.contains(4));
}

TEST(set, insert_find_and_erase_match_std_set_behavior)
{
    rpnx::set< std::string > values;

    std::pair< rpnx::set< std::string >::iterator, bool > inserted = values.insert("alpha");
    ASSERT_TRUE(inserted.second);
    EXPECT_EQ(*inserted.first, "alpha");

    inserted = values.insert("alpha");
    EXPECT_FALSE(inserted.second);

    values.emplace("beta");
    ASSERT_NE(values.find("beta"), values.end());
    EXPECT_EQ(values.erase("alpha"), 1U);
    EXPECT_EQ(values.count("alpha"), 0U);
}

TEST(set, custom_comparator_controls_iteration_order)
{
    rpnx::set< int, std::greater< int > > values{1, 3, 2};
    std::vector< int > ordered(values.begin(), values.end());

    EXPECT_EQ(ordered, (std::vector< int >{3, 2, 1}));
}

TEST(set, smaller_size_compares_less_regardless_of_values)
{
    rpnx::set< int > small{100};
    rpnx::set< int > large{1, 2};

    EXPECT_LT(small, large);
    EXPECT_GT(large, small);
    EXPECT_EQ(small <=> large, std::strong_ordering::less);
    EXPECT_EQ(large <=> small, std::strong_ordering::greater);
}

TEST(set, equal_size_sets_compare_lexicographically)
{
    rpnx::set< int > low{1, 5};
    rpnx::set< int > high{2, 3};
    rpnx::set< int > same{1, 5};

    EXPECT_LT(low, high);
    EXPECT_GT(high, low);
    EXPECT_EQ(low, same);
    EXPECT_LE(low, same);
    EXPECT_GE(low, same);
    EXPECT_EQ(low <=> high, std::strong_ordering::less);
    EXPECT_EQ(low <=> same, std::strong_ordering::equal);
}

TEST(set, equal_size_comparison_forwards_to_underlying_std_set)
{
    rpnx::set< int, std::greater< int > > low{3, 1};
    rpnx::set< int, std::greater< int > > high{2, 1};

    EXPECT_GT(low, high);
    EXPECT_LT(high, low);
    EXPECT_EQ(low <=> high, std::strong_ordering::greater);
}
