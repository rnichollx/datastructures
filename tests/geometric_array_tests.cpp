// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0
#include "failing_allocator.hpp"
#include "tracking_allocator.hpp"
#include <cstdint>
#include <gtest/gtest.h>
#include <numeric>
#include <random>
#include <rpnx/geometric_array.hpp>
#include <sstream>
#include <string>
#include <unordered_map>

static_assert(std::random_access_iterator< rpnx::geometric_array< std::int32_t >::iterator >);
static_assert(std::random_access_iterator< rpnx::geometric_array< std::int32_t >::const_iterator >);
static_assert(!std::contiguous_iterator< rpnx::geometric_array< std::int32_t >::iterator >);
static_assert(std::is_same_v< rpnx::geometric_array< bool >::reference, bool& >);

/** @brief Detects whether a container exposes explicit reservation publicly. */
template < typename Container >
concept publicly_reservable = requires(Container& values) { values.reserve(1); };
/** @brief Detects whether a container exposes its allocated slot count publicly. */
template < typename Container >
concept publicly_reports_capacity = requires(Container const& values) { values.capacity(); };
static_assert(!publicly_reservable< rpnx::geometric_array< std::int32_t > >);
static_assert(!publicly_reports_capacity< rpnx::geometric_array< std::int32_t > >);

namespace geometric_array_tests
{
    /** @brief Tracks object lifetime and injects construction/assignment failures. */
    class throwing_value
    {
      public:
        inline static std::int32_t live = 0;
        inline static std::int32_t budget = -1;
        std::int32_t value = 0;
        /** @brief Throws at the configured operation count. */
        static void consume()
        {
            if (budget == 0)
            {
                throw std::runtime_error("element operation failed");
            }
            if (budget > 0)
            {
                --budget;
            }
        }
        /** @brief Constructs a tracked value. */
        explicit throwing_value(std::int32_t input = 0) : value(input)
        {
            consume();
            ++live;
        }
        /** @brief Copies a tracked value. */
        throwing_value(throwing_value const& other) : throwing_value(other.value)
        {
        }
        /** @brief Moves a tracked value with optional failure. */
        throwing_value(throwing_value&& other) : throwing_value(other.value)
        {
            other.value = -1;
        }
        /** @brief Assigns a tracked value with optional failure. */
        throwing_value& operator=(throwing_value const& other)
        {
            consume();
            value = other.value;
            return *this;
        }
        /** @brief Moves into an existing value with optional failure. */
        throwing_value& operator=(throwing_value&& other)
        {
            consume();
            value = other.value;
            other.value = -1;
            return *this;
        }
        /** @brief Records object destruction. */
        ~throwing_value()
        {
            --live;
        }
    };

    /** @brief Nonmovable element used to verify append never relocates elements. */
    struct immovable_value
    {
        std::int32_t value;
        /** @brief Constructs the stored integer. */
        explicit immovable_value(std::int32_t input) : value(input)
        {
        }
        immovable_value(immovable_value const&) = delete;
        immovable_value(immovable_value&&) = delete;
    };

    /** @brief Copy-only element for append and copy construction. */
    struct copy_only_value
    {
        std::int32_t value = 0;
        copy_only_value() = default;
        copy_only_value(copy_only_value const&) = default;
        copy_only_value(copy_only_value&&) = delete;
    };

    /** @brief Records allocation sizes and owning allocator identities. */
    struct allocation_records
    {
        std::unordered_map< void*, std::pair< std::size_t, std::int32_t > > live;
        std::vector< std::size_t > sizes;
        /** @brief Returns the total element slots in live test allocations. */
        std::size_t allocated_elements() const
        {
            std::size_t total = 0;
            for (std::pair< void* const, std::pair< std::size_t, std::int32_t > > const& allocation : live)
            {
                total += allocation.second.first;
            }
            return total;
        }
    };

    /** @brief Stateful allocator that verifies matching deallocation ownership and size. */
    template < typename T, bool Propagate >
    struct ownership_allocator
    {
        using value_type = T;
        using propagate_on_container_copy_assignment = std::bool_constant< Propagate >;
        using propagate_on_container_move_assignment = std::bool_constant< Propagate >;
        using propagate_on_container_swap = std::bool_constant< Propagate >;
        std::shared_ptr< allocation_records > records;
        std::int32_t id;
        /** @brief Creates an allocator sharing allocation records. */
        ownership_allocator(std::shared_ptr< allocation_records > input, std::int32_t identity) : records(std::move(input)), id(identity)
        {
        }
        /** @brief Allocates and records a segment. */
        T* allocate(std::size_t count)
        {
            T* result = std::allocator< T >().allocate(count);
            records->live.emplace(result, std::pair(count, id));
            records->sizes.push_back(count);
            return result;
        }
        /** @brief Verifies and releases a segment. */
        void deallocate(T* address, std::size_t count)
        {
            EXPECT_EQ(records->live.at(address), std::make_pair(count, id));
            records->live.erase(address);
            std::allocator< T >().deallocate(address, count);
        }
        /** @brief Compares allocator identities and record ownership. */
        bool operator==(ownership_allocator const&) const = default;
    };
} // namespace geometric_array_tests

TEST(geometric_array, exact_segments_and_stable_addresses)
{
    auto records = std::make_shared< geometric_array_tests::allocation_records >();
    using allocator = geometric_array_tests::ownership_allocator< std::int32_t, false >;
    {
        rpnx::geometric_array< std::int32_t, allocator > values(allocator(records, 7));
        EXPECT_EQ(records->allocated_elements(), 0U);
        std::vector< std::int32_t* > addresses;
        for (std::int32_t i = 0; i < 1024; ++i)
        {
            values.push_back(i);
            addresses.push_back(&values.back());
            EXPECT_EQ(records->allocated_elements(), (std::size_t{1} << std::bit_width(values.size())) - 1);
        }
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            EXPECT_EQ(&values[i], addresses[i]);
            EXPECT_EQ(values[i], i);
        }
        for (std::size_t i = 0; i < records->sizes.size(); ++i)
        {
            EXPECT_EQ(records->sizes[i], std::size_t{1} << i);
        }
        values.resize(10000);
        EXPECT_EQ(&values[1023], addresses[1023]);
        values.resize(1023);
        EXPECT_EQ(records->allocated_elements(), 2047U);
        EXPECT_EQ(&values[1022], addresses[1022]);
        values.clear();
        EXPECT_EQ(records->allocated_elements(), 1U);
    }
    EXPECT_TRUE(records->live.empty());
}

TEST(geometric_array, constructors_access_assignment_and_comparison)
{
    rpnx::geometric_array< std::int32_t > zeroes(12);
    EXPECT_EQ(zeroes, (rpnx::geometric_array< std::int32_t >(12, 0)));
    rpnx::geometric_array< std::int32_t > values{4, 5, 6};
    auto copied = values;
    EXPECT_EQ(copied, values);
    auto moved = std::move(copied);
    EXPECT_TRUE(copied.empty());
    copied.push_back(9);
    EXPECT_EQ(moved, values);
    moved = moved;
    EXPECT_EQ(moved, values);
    values = {8, 9};
    EXPECT_EQ(values.front(), 8);
    EXPECT_EQ(std::as_const(values).back(), 9);
    EXPECT_EQ(std::as_const(values).at(0), 8);
    EXPECT_THROW(values.at(2), std::out_of_range);
    EXPECT_THROW(std::as_const(values).at(2), std::out_of_range);
    values.assign(10, values.front());
    EXPECT_EQ(values, (rpnx::geometric_array< std::int32_t >(10, 8)));
    values.assign({1, 2});
    EXPECT_LT(values, (rpnx::geometric_array< std::int32_t >{2}));
    std::istringstream input("11 12 13 14");
    rpnx::geometric_array from_input(std::istream_iterator< std::int32_t >(input), std::istream_iterator< std::int32_t >{});
    EXPECT_EQ(from_input, (rpnx::geometric_array< std::int32_t >{11, 12, 13, 14}));
    EXPECT_THROW(values.resize(values.max_size() + 1), std::length_error);
    EXPECT_THROW(values.insert(values.begin(), values.max_size(), 1), std::length_error);
}

TEST(geometric_array, iterator_arithmetic_algorithms_and_swap)
{
    rpnx::geometric_array< std::int32_t > values(100);
    std::iota(values.begin(), values.end(), 0);
    EXPECT_EQ(values.end() - values.cbegin(), 100);
    EXPECT_EQ(values.cbegin() - values.end(), -100);
    EXPECT_EQ((50 + values.begin())[-3], 47);
    EXPECT_EQ(*(values.end() - 50), 50);
    EXPECT_EQ(values.begin() + 5, values.cbegin() + 5);
    EXPECT_LT(values.begin(), values.cend());
    EXPECT_EQ(*values.crbegin(), 99);
    EXPECT_EQ(values.crend() - values.rbegin(), 100);
    std::reverse(values.begin(), values.end());
    std::sort(values.begin(), values.end());
    EXPECT_TRUE(std::is_sorted(values.begin(), values.end()));
    auto iter = values.begin() + 17;
    rpnx::geometric_array< std::int32_t > other{9};
    swap(values, other);
    EXPECT_EQ(iter, other.begin() + 17);
    EXPECT_EQ(*iter, 17);
    auto transferred = std::move(other);
    EXPECT_EQ(iter, transferred.begin() + 17);
    EXPECT_EQ(*iter, 17);
}

TEST(geometric_array, insertion_erasure_and_aliasing)
{
    rpnx::geometric_array< std::string > values{"a", "b", "c"};
    values.push_back(values[0]);
    EXPECT_EQ(values.back(), "a");
    EXPECT_EQ(*values.insert(values.begin() + 1, values.back()), "a");
    EXPECT_EQ(values, (rpnx::geometric_array< std::string >{"a", "a", "b", "c", "a"}));
    values.insert(values.begin() + 2, 4, values.front());
    EXPECT_EQ(values.size(), 9U);
    EXPECT_EQ(values[6], "b");
    values.resize(20, values[6]);
    EXPECT_EQ(values.back(), "b");
    values.erase(values.begin() + 1, values.end() - 1);
    EXPECT_EQ(values, (rpnx::geometric_array< std::string >{"a", "b"}));
    auto unchanged = values.erase(values.begin(), values.begin());
    EXPECT_EQ(unchanged, values.begin());
    EXPECT_EQ(values.front(), "a");
    values.insert(values.end(), {"c", "d"});
    std::istringstream input("e f g");
    values.insert(values.begin() + 2, std::istream_iterator< std::string >(input), std::istream_iterator< std::string >());
    EXPECT_EQ(values, (rpnx::geometric_array< std::string >{"a", "b", "e", "f", "g", "c", "d"}));
    EXPECT_EQ(rpnx::erase(values, std::string("f")), 1U);
    EXPECT_EQ(rpnx::erase_if(values,
                             [](std::string const& value)
                             {
                                 return value > "c";
                             }),
              3U);
    EXPECT_EQ(values, (rpnx::geometric_array< std::string >{"a", "b", "c"}));
}

TEST(geometric_array, differential_random_operations)
{
    std::mt19937 random(7291);
    rpnx::geometric_array< std::int32_t > actual;
    std::vector< std::int32_t > expected;
    for (std::size_t step = 0; step < 4000; ++step)
    {
        std::size_t position = random() % (expected.size() + 1);
        std::size_t count = random() % 20;
        std::int32_t value = static_cast< std::int32_t >(random() % 100);
        switch (random() % 7)
        {
        case 0:
            actual.push_back(value);
            expected.push_back(value);
            break;
        case 1:
            actual.insert(actual.begin() + position, count, value);
            expected.insert(expected.begin() + position, count, value);
            break;
        case 2:
            if (position < expected.size())
            {
                actual.erase(actual.begin() + position);
                expected.erase(expected.begin() + position);
            }
            break;
        case 3:
            actual.resize(count, value);
            expected.resize(count, value);
            break;
        case 4:
            actual.resize(count * 10);
            expected.resize(count * 10);
            break;
        case 5:
            if (!expected.empty())
            {
                actual.pop_back();
                expected.pop_back();
            }
            break;
        case 6:
            actual.emplace(actual.begin() + position, value);
            expected.emplace(expected.begin() + position, value);
            break;
        }
        ASSERT_EQ(actual.size(), expected.size());
        ASSERT_TRUE(std::equal(actual.begin(), actual.end(), expected.begin())) << "step " << step;
    }
}

TEST(geometric_array, nonmovable_copy_only_move_only_and_alignment)
{
    rpnx::geometric_array< geometric_array_tests::immovable_value > fixed;
    for (std::int32_t i = 0; i < 100; ++i)
    {
        fixed.emplace_back(i);
    }
    EXPECT_EQ(fixed.back().value, 99);
    rpnx::geometric_array< geometric_array_tests::copy_only_value > copies;
    geometric_array_tests::copy_only_value value;
    copies.push_back(value);
    auto copied = copies;
    EXPECT_EQ(copied.size(), 1U);
    rpnx::geometric_array< std::unique_ptr< std::int32_t > > pointers;
    pointers.emplace_back(std::make_unique< std::int32_t >(7));
    pointers.emplace(pointers.begin(), std::make_unique< std::int32_t >(8));
    pointers.erase(pointers.begin());
    EXPECT_EQ(*pointers.front(), 7);
    /** @brief Over-aligned payload for allocation alignment checks. */
    struct alignas(128) aligned_value
    {
        std::int32_t value = 0;
    };
    rpnx::geometric_array< aligned_value > aligned(100);
    for (aligned_value& element : aligned)
    {
        EXPECT_EQ(reinterpret_cast< std::uintptr_t >(&element) % alignof(aligned_value), 0U);
    }
    rpnx::geometric_array< bool > booleans{true, false};
    bool* address = &booleans[0];
    EXPECT_TRUE(*address);
}

TEST(geometric_array, allocator_selection_and_ownership)
{
    using tracked = testutils::tracking_allocator< std::int32_t >;
    rpnx::geometric_array< std::int32_t, tracked > source({1, 2}, tracked(9));
    auto copied = source;
    EXPECT_EQ(copied.get_allocator().id, 109);
    auto records = std::make_shared< geometric_array_tests::allocation_records >();
    {
        using allocator = geometric_array_tests::ownership_allocator< std::int32_t, true >;
        rpnx::geometric_array< std::int32_t, allocator > a({1, 2, 3}, allocator(records, 1));
        rpnx::geometric_array< std::int32_t, allocator > b({4}, allocator(records, 2));
        b = a;
        EXPECT_EQ(b.get_allocator().id, 1);
        rpnx::geometric_array< std::int32_t, allocator > c({9}, allocator(records, 3));
        c = std::move(b);
        EXPECT_EQ(c.get_allocator().id, 1);
        rpnx::geometric_array< std::int32_t, allocator > d({8}, allocator(records, 4));
        swap(c, d);
        EXPECT_EQ(c.get_allocator().id, 4);
        EXPECT_EQ(d.front(), 1);
    }
    EXPECT_TRUE(records->live.empty());
    {
        using allocator = geometric_array_tests::ownership_allocator< std::int32_t, false >;
        rpnx::geometric_array< std::int32_t, allocator > a({1, 2, 3}, allocator(records, 1));
        rpnx::geometric_array< std::int32_t, allocator > b({4}, allocator(records, 2));
        b = a;
        EXPECT_EQ(b.get_allocator().id, 2);
        b = std::move(a);
        EXPECT_EQ(b.get_allocator().id, 2);
        EXPECT_EQ(b.size(), 3U);
        rpnx::geometric_array< std::int32_t, allocator > c(std::move(b), allocator(records, 3));
        EXPECT_EQ(c.size(), 3U);
        rpnx::geometric_array< std::int32_t, allocator > d({8}, allocator(records, 3));
        d = std::move(c);
        EXPECT_EQ(d.front(), 1);
    }
    EXPECT_TRUE(records->live.empty());
}

TEST(geometric_array, allocation_failure_rolls_back_growth)
{
    using allocator = testutils::failing_allocator< std::int32_t >;
    allocator::allocation_count = 0;
    allocator::fail_at = 999;
    {
        rpnx::geometric_array< std::int32_t, allocator > values{1, 2, 3};
        auto original = values.begin();
        std::int32_t live = allocator::total_allocations;
        allocator::fail_at = allocator::allocation_count + 2;
        EXPECT_THROW(values.resize(100), std::bad_alloc);
        EXPECT_EQ(values.size(), 3U);
        EXPECT_EQ(values.begin(), original);
        EXPECT_EQ(*original, 1);
        EXPECT_EQ(allocator::total_allocations, live);
    }
    EXPECT_EQ(allocator::total_allocations, 0);
    allocator::fail_at = 999;
}

TEST(geometric_array, construction_and_assignment_exceptions_preserve_lifetimes)
{
    using value = geometric_array_tests::throwing_value;
    value::budget = -1;
    {
        rpnx::geometric_array< value > values(5);
        auto original = values.begin();
        value::budget = 2;
        EXPECT_THROW(values.resize(20), std::runtime_error);
        EXPECT_EQ(values.begin(), original);
        EXPECT_EQ(values.size(), 5U);
        EXPECT_EQ(value::live, 5);
        value::budget = 2;
        EXPECT_THROW((rpnx::geometric_array< value >(values)), std::runtime_error);
        EXPECT_EQ(value::live, 5);
        value::budget = 2;
        EXPECT_THROW((rpnx::geometric_array< value >(20)), std::runtime_error);
        EXPECT_EQ(value::live, 5);
        value::budget = -1;
        rpnx::geometric_array< value > target(2);
        value::budget = 2;
        EXPECT_THROW(target = values, std::runtime_error);
        EXPECT_EQ(target.size(), 2U);
        EXPECT_EQ(value::live, 7);
        value::budget = 2;
        EXPECT_THROW(values.emplace(values.begin() + 1, 9), std::runtime_error);
        EXPECT_EQ(value::live, values.size() + target.size());
        value::budget = 0;
        EXPECT_THROW(values.erase(values.begin()), std::runtime_error);
        EXPECT_EQ(value::live, values.size() + target.size());
        value::budget = -1;
    }
    EXPECT_EQ(value::live, 0);
}

TEST(geometric_array, failed_append_preserves_storage_and_iterators)
{
    using value = geometric_array_tests::throwing_value;
    value::budget = -1;
    {
        rpnx::geometric_array< value > values(7);
        auto original = values.begin();
        value::budget = 0;
        EXPECT_THROW(values.emplace_back(9), std::runtime_error);
        EXPECT_EQ(values.size(), 7U);
        EXPECT_EQ(values.begin(), original);
        EXPECT_EQ(value::live, 7);
        value::budget = -1;
    }
    EXPECT_EQ(value::live, 0);
}

TEST(geometric_array, failed_range_insertion_preserves_storage)
{
    using value = geometric_array_tests::throwing_value;
    value::budget = -1;
    {
        rpnx::geometric_array< value > values(7);
        rpnx::geometric_array< value > input(4);
        auto original = values.begin();
        // Four successful copies stage the input, then two append successfully.
        value::budget = 6;
        EXPECT_THROW(values.insert(values.begin() + 1, input.begin(), input.end()), std::runtime_error);
        EXPECT_EQ(values.size(), 7U);
        EXPECT_EQ(values.begin(), original);
        EXPECT_EQ(value::live, 11);
        value::budget = -1;
    }
    EXPECT_EQ(value::live, 0);
}

TEST(geometric_array, removal_retains_one_empty_segment)
{
    for (std::size_t count = 0; count <= 64; ++count)
    {
        auto records = std::make_shared< geometric_array_tests::allocation_records >();
        using allocator = geometric_array_tests::ownership_allocator< std::int32_t, false >;
        {
            rpnx::geometric_array< std::int32_t, allocator > values(allocator(records, 7));
            values.resize(511, 42);
            std::vector< std::int32_t* > addresses;
            for (std::int32_t& value : values)
            {
                addresses.push_back(&value);
            }
            auto beginning = values.begin();
            std::size_t allocations = records->sizes.size();
            values.resize(count);
            std::size_t retained = std::bit_width(count) + 1;
            EXPECT_EQ(values.size(), count);
            EXPECT_EQ(records->allocated_elements(), (std::size_t{1} << retained) - 1);
            EXPECT_EQ(records->live.size(), retained);
            EXPECT_EQ(records->sizes.size(), allocations);
            EXPECT_EQ(values.begin(), beginning);
            for (std::size_t index = 0; index < count; ++index)
            {
                EXPECT_EQ(&values[index], addresses[index]);
                EXPECT_EQ(values[index], 42);
            }
            values.emplace_back(99);
            EXPECT_EQ(records->sizes.size(), allocations);
        }
        EXPECT_TRUE(records->live.empty());
    }
}

TEST(geometric_array, automatic_shrink_boundaries_and_clear)
{
    auto records = std::make_shared< geometric_array_tests::allocation_records >();
    using allocator = geometric_array_tests::ownership_allocator< std::int32_t, false >;
    rpnx::geometric_array< std::int32_t, allocator > values(allocator(records, 7));
    values.clear();
    EXPECT_EQ(records->allocated_elements(), 0U);
    values.resize(7, 42);
    auto beginning = values.begin();
    values.resize(4, 9);
    EXPECT_EQ(records->allocated_elements(), 7U);
    values.pop_back();
    EXPECT_EQ(records->allocated_elements(), 7U);
    values.pop_back();
    EXPECT_EQ(records->allocated_elements(), 7U);
    // Size 1 leaves two empty segments, so the larger segment is reclaimed.
    values.pop_back();
    EXPECT_EQ(records->allocated_elements(), 3U);
    EXPECT_EQ(values.begin(), beginning);
    values.clear();
    EXPECT_EQ(records->allocated_elements(), 1U);
    values.resize(127);
    values.clear();
    EXPECT_EQ(records->allocated_elements(), 1U);
}

TEST(geometric_array, erase_reclaims_segments_and_preserves_prefix_iterators)
{
    auto records = std::make_shared< geometric_array_tests::allocation_records >();
    using allocator = geometric_array_tests::ownership_allocator< std::int32_t, false >;
    rpnx::geometric_array< std::int32_t, allocator > values(127, allocator(records, 7));
    std::iota(values.begin(), values.end(), 0);
    auto beginning = values.begin();
    auto result = values.erase(values.begin() + 2, values.end() - 1);
    EXPECT_EQ(values.size(), 3U);
    EXPECT_EQ(records->allocated_elements(), 7U);
    EXPECT_EQ(values.begin(), beginning);
    EXPECT_EQ(result, values.begin() + 2);
    EXPECT_EQ(*result, 126);
    values.erase(values.begin(), values.end());
    EXPECT_TRUE(values.empty());
    EXPECT_EQ(records->allocated_elements(), 1U);
}

TEST(geometric_array, explicit_compaction_releases_the_spare_segment)
{
    auto records = std::make_shared< geometric_array_tests::allocation_records >();
    using allocator = geometric_array_tests::ownership_allocator< std::int32_t, false >;
    rpnx::geometric_array< std::int32_t, allocator > values(63, 42, allocator(records, 7));
    values.resize(5);
    EXPECT_EQ(records->allocated_elements(), 15U);
    std::int32_t* first = &values.front();
    values.shrink_to_fit();
    EXPECT_EQ(records->allocated_elements(), 7U);
    EXPECT_EQ(&values.front(), first);
    EXPECT_EQ(values.size(), 5U);
    values.clear();
    EXPECT_EQ(records->allocated_elements(), 1U);
    values.shrink_to_fit();
    EXPECT_EQ(records->allocated_elements(), 0U);
    values.emplace_back(99);
    EXPECT_EQ(values.front(), 99);
}
