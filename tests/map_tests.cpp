// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>

#include "rpnx/map.hpp"
#include "gtest/gtest.h"

#include <compare>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    struct counted_map_key
    {
        static int three_way_count;

        int value = 0;

        std::strong_ordering operator<=>(counted_map_key const& other) const
        {
            ++three_way_count;
            return value <=> other.value;
        }

        bool operator==(counted_map_key const& other) const
        {
            return value == other.value;
        }
    };

    int counted_map_key::three_way_count = 0;
} // namespace

static_assert(std::is_same_v< decltype(std::declval< rpnx::map< int, int > const& >() <=> std::declval< rpnx::map< int, int > const& >()), std::strong_ordering >);

TEST(map, initializer_list_sorts_by_key)
{
    rpnx::map< int, std::string > values{{3, "three"}, {1, "one"}, {2, "two"}};
    std::vector< int > keys;

    for (std::pair< int const, std::string > const& entry : values)
    {
        keys.push_back(entry.first);
    }

    EXPECT_EQ(values.size(), 3U);
    EXPECT_EQ(keys, (std::vector< int >{1, 2, 3}));
    EXPECT_TRUE(values.contains(2));
    EXPECT_EQ(values.at(3), "three");
}

TEST(map, insert_assign_and_lookup_work)
{
    rpnx::map< std::string, int > values;
    rpnx::map< std::string, int > source{{"delta", 4}, {"epsilon", 5}};

    std::pair< rpnx::map< std::string, int >::iterator, bool > inserted = values.insert({"alpha", 1});
    ASSERT_TRUE(inserted.second);
    EXPECT_EQ(inserted.first->second, 1);

    values["beta"] = 2;
    values.insert_or_assign("alpha", 3);
    values.try_emplace("gamma", 4);
    values.insert(source.begin(), source.end());

    EXPECT_EQ(values.at("alpha"), 3);
    EXPECT_EQ(values.find("beta")->second, 2);
    EXPECT_EQ(values.find("delta")->second, 4);
    EXPECT_EQ(values.erase("gamma"), 1U);
    EXPECT_FALSE(values.contains("gamma"));
}

TEST(map, custom_comparator_controls_iteration_order)
{
    rpnx::map< int, int, std::greater< int > > values{{1, 10}, {3, 30}, {2, 20}};
    std::vector< int > keys;

    for (std::pair< int const, int > const& entry : values)
    {
        keys.push_back(entry.first);
    }

    EXPECT_EQ(keys, (std::vector< int >{3, 2, 1}));
}

TEST(map, smaller_size_compares_less_regardless_of_values)
{
    rpnx::map< int, int > small{{100, 100}};
    rpnx::map< int, int > large{{1, 1}, {2, 2}};

    EXPECT_LT(small, large);
    EXPECT_GT(large, small);
    EXPECT_EQ(small <=> large, std::strong_ordering::less);
    EXPECT_EQ(large <=> small, std::strong_ordering::greater);
}

TEST(map, equal_size_comparison_forwards_to_underlying_map)
{
    rpnx::map< int, int > low{{1, 5}, {2, 0}};
    rpnx::map< int, int > high{{2, 3}, {3, 0}};
    rpnx::map< int, int > same{{1, 5}, {2, 0}};

    EXPECT_LT(low, high);
    EXPECT_GT(high, low);
    EXPECT_EQ(low, same);
    EXPECT_EQ(low <=> high, std::strong_ordering::less);
    EXPECT_EQ(low <=> same, std::strong_ordering::equal);
}

TEST(map, spaceship_size_short_circuit_does_not_compare_elements)
{
    rpnx::map< counted_map_key, int > small{{{100}, 100}};
    rpnx::map< counted_map_key, int > large{{{1}, 1}, {{2}, 2}};

    counted_map_key::three_way_count = 0;

    EXPECT_EQ(small <=> large, std::strong_ordering::less);
    EXPECT_EQ(counted_map_key::three_way_count, 0);
}
