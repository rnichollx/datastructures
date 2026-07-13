// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>

#include "rpnx/dynar.hpp"
#include "gtest/gtest.h"

#include <compare>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    struct counted_dynar_value
    {
        static int three_way_count;

        int value = 0;

        std::strong_ordering operator<=>(counted_dynar_value const& other) const
        {
            ++three_way_count;
            return value <=> other.value;
        }

        bool operator==(counted_dynar_value const& other) const
        {
            return value == other.value;
        }
    };

    int counted_dynar_value::three_way_count = 0;
} // namespace

static_assert(std::is_same_v< decltype(std::declval< rpnx::dynar< int > const& >() <=> std::declval< rpnx::dynar< int > const& >()), std::strong_ordering >);

TEST(dynar, initializer_list_preserves_order)
{
    rpnx::dynar< int > values{3, 1, 2};
    std::vector< int > copied(values.begin(), values.end());

    EXPECT_EQ(values.size(), 3U);
    EXPECT_EQ(copied, (std::vector< int >{3, 1, 2}));
    EXPECT_EQ(values.front(), 3);
    EXPECT_EQ(values.back(), 2);
}

TEST(dynar, vector_style_modifiers_work)
{
    rpnx::dynar< std::string > values;

    values.reserve(4);
    values.push_back("alpha");
    values.emplace_back("gamma");
    values.insert(values.begin() + 1, "beta");

    ASSERT_GE(values.capacity(), 4U);
    EXPECT_EQ(values.size(), 3U);
    EXPECT_EQ(values[1], "beta");

    values.erase(values.begin());
    EXPECT_EQ(values.front(), "beta");

    values.resize(3, "tail");
    EXPECT_EQ(values.back(), "tail");
}

TEST(dynar, supports_move_only_values)
{
    rpnx::dynar< std::unique_ptr< int > > values;

    values.push_back(std::make_unique< int >(7));
    values.emplace_back(std::make_unique< int >(11));

    ASSERT_EQ(values.size(), 2U);
    ASSERT_TRUE(values[0]);
    ASSERT_TRUE(values[1]);
    EXPECT_EQ(*values[0], 7);
    EXPECT_EQ(*values[1], 11);
}

TEST(dynar, smaller_size_compares_less_regardless_of_values)
{
    rpnx::dynar< int > small{100};
    rpnx::dynar< int > large{1, 2};

    EXPECT_LT(small, large);
    EXPECT_GT(large, small);
    EXPECT_EQ(small <=> large, std::strong_ordering::less);
    EXPECT_EQ(large <=> small, std::strong_ordering::greater);
}

TEST(dynar, equal_size_comparison_forwards_to_underlying_vector)
{
    rpnx::dynar< int > low{1, 5};
    rpnx::dynar< int > high{2, 3};
    rpnx::dynar< int > same{1, 5};

    EXPECT_LT(low, high);
    EXPECT_GT(high, low);
    EXPECT_EQ(low, same);
    EXPECT_EQ(low <=> high, std::strong_ordering::less);
    EXPECT_EQ(low <=> same, std::strong_ordering::equal);
}

TEST(dynar, spaceship_size_short_circuit_does_not_compare_elements)
{
    rpnx::dynar< counted_dynar_value > small{{100}};
    rpnx::dynar< counted_dynar_value > large{{1}, {2}};

    counted_dynar_value::three_way_count = 0;

    EXPECT_EQ(small <=> large, std::strong_ordering::less);
    EXPECT_EQ(counted_dynar_value::three_way_count, 0);
}
