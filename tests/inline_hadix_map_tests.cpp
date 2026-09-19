// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0
#include <algorithm>
#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <memory_resource>
#include <random>
#include <rpnx/inline_hadix_map.hpp>
#include <string>
#include <vector>

static_assert(std::forward_iterator< rpnx::inline_hadix_map< std::size_t, std::size_t >::iterator >);
static_assert(std::forward_iterator< rpnx::inline_hadix_map< std::size_t, std::size_t >::const_iterator >);
static_assert(std::forward_iterator< rpnx::inline_hadix_map< std::size_t, std::unique_ptr< std::size_t > >::iterator >);
static_assert(!std::is_copy_constructible_v< rpnx::inline_hadix_map< std::size_t, std::unique_ptr< std::size_t > > >);

namespace inline_hadix_tests
{
    /** @brief Concentrates keys into exact hash collision groups. */
    struct grouped_hash
    {
        std::size_t groups = 1;
        /** @brief Returns a bounded hash, including total collisions when groups is one. */
        std::size_t operator()(std::size_t key) const noexcept
        {
            return key % groups;
        }
    };
    /** @brief Generates reversible distributed hashes. */
    struct distributed_hash
    {
        /** @brief Mixes integer keys across indexing and fingerprint bits. */
        std::size_t operator()(std::size_t key) const noexcept
        {
            key ^= key >> 30;
            key *= UINT64_C(0xbf58476d1ce4e5b9);
            key ^= key >> 27;
            key *= UINT64_C(0x94d049bb133111eb);
            return key ^ (key >> 31);
        }
    };
    /** @brief Tracks live objects, construction failures and value relocation. */
    struct tracked_value
    {
        static inline std::size_t live = 0;
        static inline std::size_t moves = 0;
        std::size_t value;
        /** @brief Constructs a tracked value, optionally failing before it becomes live. */
        explicit tracked_value(std::size_t input, bool fail = false) : value(input)
        {
            if (fail)
            {
                throw std::runtime_error("construction failure");
            }
            ++live;
        }
        /** @brief Transfers a tracked value without assignment or allocation. */
        tracked_value(tracked_value&& other) noexcept : value(other.value)
        {
            ++live;
            ++moves;
            other.value = 0;
        }
        /** @brief Decrements the live-object count. */
        ~tracked_value()
        {
            --live;
        }
    };
    /** @brief Fails selected allocations and counts live allocation blocks. */
    class failing_resource : public std::pmr::memory_resource
    {
      public:
        std::size_t remaining = std::numeric_limits< std::size_t >::max();
        std::size_t blocks = 0;

      private:
        /** @brief Allocates until the configured failure boundary. */
        void* do_allocate(std::size_t bytes, std::size_t alignment) override
        {
            if (remaining == 0)
            {
                throw std::bad_alloc();
            }
            --remaining;
            void* result = std::pmr::new_delete_resource()->allocate(bytes, alignment);
            ++blocks;
            return result;
        }
        /** @brief Releases an allocation with its original size and alignment. */
        void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override
        {
            --blocks;
            std::pmr::new_delete_resource()->deallocate(pointer, bytes, alignment);
        }
        /** @brief Identifies resources by address. */
        bool do_is_equal(std::pmr::memory_resource const& other) const noexcept override
        {
            return this == &other;
        }
    };
    /** @brief Compares randomized growth, churn and drain against an ordered reference map. */
    template < typename Hash >
    void exercise_mutations(Hash hash)
    {
        rpnx::inline_hadix_map< std::size_t, std::size_t, Hash > values(hash);
        std::map< std::size_t, std::size_t > expected;
        std::mt19937_64 random(93751);
        for (std::size_t step = 0; step < 16000; ++step)
        {
            std::size_t key = random() % 2000;
            if (random() % 3 == 0)
            {
                EXPECT_EQ(values.erase(key), expected.erase(key));
            }
            else
            {
                EXPECT_EQ(values.insert_or_assign(key, step).second, expected.insert_or_assign(key, step).second);
            }
            ASSERT_EQ(values.size(), expected.size());
            if (step % 251 == 0)
            {
                std::map< std::size_t, std::size_t > visited;
                for (auto entry : values)
                {
                    EXPECT_TRUE(visited.emplace(entry.first, entry.second).second);
                }
                EXPECT_EQ(visited, expected);
                for (auto entry : expected)
                {
                    ASSERT_EQ(values.at(entry.first), entry.second);
                }
                EXPECT_FALSE(values.contains(3000));
            }
        }
        while (!values.empty())
        {
            auto first = values.begin();
            expected.erase(first->first);
            values.erase(first);
            ASSERT_EQ(values.size(), expected.size());
        }
        EXPECT_EQ(values.begin(), values.end());
    }
} // namespace inline_hadix_tests

TEST(inline_hadix_map, distributed_growth_churn_and_drain)
{
    inline_hadix_tests::exercise_mutations(inline_hadix_tests::distributed_hash{});
}
TEST(inline_hadix_map, total_collisions_growth_churn_and_drain)
{
    inline_hadix_tests::exercise_mutations(inline_hadix_tests::grouped_hash{1});
}
TEST(inline_hadix_map, grouped_collisions_growth_churn_and_drain)
{
    inline_hadix_tests::exercise_mutations(inline_hadix_tests::grouped_hash{64});
}
TEST(inline_hadix_map, public_api_and_const_keys)
{
    using map_type = rpnx::inline_hadix_map< std::string, std::int32_t >;
    static_assert(std::is_const_v< std::remove_reference_t< decltype(std::declval< map_type::reference >().first) > >);
    static_assert(std::is_const_v< std::remove_reference_t< decltype(std::declval< map_type::const_reference >().second) > >);
    static_assert(std::is_void_v< decltype(std::declval< map_type& >().erase(std::declval< map_type::iterator >())) >);
    map_type values{{"one", 1}, {"two", 2}};
    values["three"] = 3;
    EXPECT_EQ(values.at("two"), 2);
    EXPECT_FALSE(values.try_emplace("two", 9).second);
    EXPECT_FALSE(values.insert_or_assign("two", 20).second);
    EXPECT_EQ(values.count("two"), 1U);
    EXPECT_EQ(values.erase("one"), 1U);
    EXPECT_THROW(values.at("missing"), std::out_of_range);
    map_type copied(values);
    values.at("two") = 8;
    EXPECT_EQ(copied.at("two"), 20);
    copied = values;
    EXPECT_EQ(copied.at("two"), 8);
    map_type moved(std::move(copied));
    EXPECT_TRUE(copied.empty());
    EXPECT_EQ(moved.size(), 2U);
    copied = std::move(moved);
    EXPECT_TRUE(moved.empty());
    EXPECT_EQ(copied.at("three"), 3);
    for (auto&& [key, value] : copied)
    {
        value += 1;
    }
    EXPECT_EQ(copied.at("three"), 4);
}
TEST(inline_hadix_map, move_only_values_and_failed_construction)
{
    using inline_hadix_tests::tracked_value;
    ASSERT_EQ(tracked_value::live, 0U);
    {
        rpnx::inline_hadix_map< std::size_t, tracked_value, inline_hadix_tests::grouped_hash > values;
        for (std::size_t i = 0; i < 1000; ++i)
        {
            values.try_emplace(i, i);
        }
        EXPECT_EQ(tracked_value::live, values.size());
        EXPECT_FALSE(values.try_emplace(5U, 0, true).second);
        EXPECT_THROW(values.try_emplace(1001U, 0, true), std::runtime_error);
        EXPECT_EQ(values.size(), 1000U);
        for (std::size_t i = 0; i < 1000; i += 2)
        {
            values.erase(i);
        }
        for (std::size_t i = 1; i < 1000; i += 2)
        {
            EXPECT_EQ(values.at(i).value, i);
        }
        EXPECT_EQ(tracked_value::live, values.size());
        EXPECT_GT(tracked_value::moves, 1000U);
    }
    EXPECT_EQ(tracked_value::live, 0U);
}
TEST(inline_hadix_map, aliasing_arguments_survive_split)
{
    rpnx::inline_hadix_map< std::size_t, std::string > values;
    for (std::size_t i = 0; i < 512; ++i)
    {
        if (i == 0)
        {
            values.try_emplace(i, "payload");
        }
        else
        {
            values.try_emplace(i, values.at(i - 1));
        }
    }
    for (std::size_t i = 0; i < 512; ++i)
    {
        EXPECT_EQ(values.at(i), "payload");
    }
}
TEST(inline_hadix_map, allocation_failures_leave_valid_storage)
{
    using allocator = std::pmr::polymorphic_allocator< std::pair< std::size_t const, std::size_t > >;
    using map_type = rpnx::inline_hadix_map< std::size_t, std::size_t, inline_hadix_tests::grouped_hash, std::compare_three_way, allocator >;
    inline_hadix_tests::failing_resource resource;
    for (std::size_t allowance = 0; allowance < 60; ++allowance)
    {
        {
            map_type values(inline_hadix_tests::grouped_hash{64}, {}, allocator(&resource));
            resource.remaining = allowance;
            std::size_t inserted = 0;
            try
            {
                for (; inserted < 200; ++inserted)
                {
                    values.try_emplace(inserted, inserted);
                }
            }
            catch (std::bad_alloc const&)
            {
            }
            EXPECT_EQ(values.size(), inserted);
            for (std::size_t i = 0; i < inserted; ++i)
            {
                EXPECT_EQ(values.at(i), i);
            }
            resource.remaining = std::numeric_limits< std::size_t >::max();
        }
        EXPECT_EQ(resource.blocks, 0U);
    }
}
TEST(inline_hadix_map, allocator_ownership_copy_and_unequal_move)
{
    using allocator = std::pmr::polymorphic_allocator< std::pair< std::size_t const, std::string > >;
    using map_type = rpnx::inline_hadix_map< std::size_t, std::string, std::hash< std::size_t >, std::compare_three_way, allocator >;
    inline_hadix_tests::failing_resource first;
    inline_hadix_tests::failing_resource second;
    {
        map_type source{allocator(&first)};
        map_type destination{allocator(&second)};
        for (std::size_t i = 0; i < 500; ++i)
        {
            source.try_emplace(i, std::to_string(i));
        }
        destination = source;
        EXPECT_EQ(destination.get_allocator().resource(), &second);
        destination = std::move(source);
        EXPECT_TRUE(source.empty());
        for (std::size_t i = 0; i < 500; ++i)
        {
            EXPECT_EQ(destination.at(i), std::to_string(i));
        }
    }
    EXPECT_EQ(first.blocks, 0U);
    EXPECT_EQ(second.blocks, 0U);
}

namespace inline_hadix_tests
{
    /** @brief Places every key in the bucket pair joined when 64 buckets shrink to 63. */
    struct join_hash
    {
        /** @brief Alternates between the lower and upper split ranges. */
        std::size_t operator()(std::size_t key) const noexcept
        {
            return 31 + (key % 2) * 32;
        }
    };
    /** @brief Move-only key used to verify that public key immutability does not require copying. */
    struct movable_key
    {
        std::size_t value;
        /** @brief Initializes a key with its comparison identity. */
        explicit movable_key(std::size_t input) noexcept : value(input)
        {
        }
        /** @brief Transfers identity during relocation. */
        movable_key(movable_key&& other) noexcept : value(std::exchange(other.value, 0))
        {
        }
        /** @brief Orders keys by identity. */
        auto operator<=>(movable_key const& other) const noexcept = default;
    };
    /** @brief Hashes move-only key identities into collision groups. */
    struct movable_hash
    {
        /** @brief Uses a bounded hash to exercise overflow ownership. */
        std::size_t operator()(movable_key const& key) const noexcept
        {
            return key.value % 8;
        }
    };
    /** @brief Counts comparisons and can fail a specific comparison. */
    struct counted_compare
    {
        std::size_t* calls;
        std::size_t* limit;
        /** @brief Compares integer keys after enforcing the injected failure boundary. */
        std::strong_ordering operator()(std::size_t left, std::size_t right) const
        {
            if (++*calls > *limit)
            {
                throw std::runtime_error("comparison failure");
            }
            return left <=> right;
        }
    };
} // namespace inline_hadix_tests
TEST(inline_hadix_map, every_staged_join_allocation_can_fail_safely)
{
    using allocator = std::pmr::polymorphic_allocator< std::pair< std::size_t const, std::size_t > >;
    using map_type = rpnx::inline_hadix_map< std::size_t, std::size_t, inline_hadix_tests::join_hash, std::compare_three_way, allocator >;
    inline_hadix_tests::failing_resource resource;
    for (std::size_t allowance = 0; allowance < 8; ++allowance)
    {
        {
            map_type values({}, {}, allocator(&resource));
            resource.remaining = std::numeric_limits< std::size_t >::max();
            for (std::size_t i = 0; i < 256; ++i)
            {
                values.try_emplace(i, i);
            }
            for (std::size_t i = 0; i < 11; ++i)
            {
                values.erase(i);
            }
            ASSERT_EQ(values.bucket_count(), 64U);
            resource.remaining = allowance;
            EXPECT_THROW(values.erase(11), std::bad_alloc);
            EXPECT_EQ(values.size(), 244U);
            EXPECT_EQ(values.bucket_count(), 64U);
            for (std::size_t i = 12; i < 256; ++i)
            {
                ASSERT_EQ(values.at(i), i);
            }
            EXPECT_FALSE(values.contains(11));
            resource.remaining = std::numeric_limits< std::size_t >::max();
            EXPECT_EQ(values.erase(12), 1U);
            EXPECT_EQ(values.bucket_count(), 63U);
        }
        EXPECT_EQ(resource.blocks, 0U);
    }
}
TEST(inline_hadix_map, move_only_keys_and_mapped_values)
{
    using inline_hadix_tests::movable_key;
    rpnx::inline_hadix_map< movable_key, std::unique_ptr< std::size_t >, inline_hadix_tests::movable_hash > values;
    for (std::size_t i = 0; i < 1000; ++i)
    {
        values.try_emplace(movable_key(i), std::make_unique< std::size_t >(i));
    }
    for (std::size_t i = 0; i < 1000; ++i)
    {
        ASSERT_EQ(*values.at(movable_key(i)), i);
    }
    for (std::size_t i = 0; i < 1000; ++i)
    {
        EXPECT_EQ(values.erase(movable_key(i)), 1U);
    }
    EXPECT_TRUE(values.empty());
}
TEST(inline_hadix_map, collision_comparison_bound_and_throwing_comparison)
{
    std::size_t calls = 0;
    std::size_t limit = std::numeric_limits< std::size_t >::max();
    using map_type = rpnx::inline_hadix_map< std::size_t, std::size_t, inline_hadix_tests::grouped_hash, inline_hadix_tests::counted_compare >;
    map_type values({}, {&calls, &limit});
    for (std::size_t i = 0; i < 8192; ++i)
    {
        values.try_emplace(i, i);
    }
    for (std::size_t key : {0U, 14U, 15U, 4096U, 8191U, 9000U})
    {
        calls = 0;
        EXPECT_EQ(values.contains(key), key < 8192);
        EXPECT_LE(calls, 16U + 2U * std::bit_width(values.size()));
    }
    for (std::size_t allowance = 0; allowance < 50; ++allowance)
    {
        calls = 0;
        limit = allowance;
        try
        {
            values.try_emplace(9000U, 9000U);
        }
        catch (std::runtime_error const&)
        {
        }
        limit = std::numeric_limits< std::size_t >::max();
        for (std::size_t i = 0; i < 8192; ++i)
        {
            ASSERT_EQ(values.at(i), i);
        }
        values.erase(9000U);
        EXPECT_EQ(values.size(), 8192U);
    }
}
TEST(inline_hadix_map, inline_entries_do_not_allocate_individually)
{
    using allocator = std::pmr::polymorphic_allocator< std::pair< std::size_t const, std::size_t > >;
    using map_type = rpnx::inline_hadix_map< std::size_t, std::size_t, inline_hadix_tests::grouped_hash, std::compare_three_way, allocator >;
    inline_hadix_tests::failing_resource resource;
    {
        map_type values({}, {}, allocator(&resource));
        for (std::size_t i = 0; i < 8; ++i)
        {
            values.try_emplace(i, i);
        }
        for (std::size_t i = 1; i < 8; ++i)
        {
            values.erase(i);
        }
        std::size_t blocks = resource.blocks;
        resource.remaining = 0;
        for (std::size_t i = 1; i < 8; ++i)
        {
            EXPECT_NO_THROW(values.try_emplace(i, i));
        }
        EXPECT_EQ(resource.blocks, blocks);
    }
    EXPECT_EQ(resource.blocks, 0U);
}

namespace inline_hadix_tests
{
    /** @brief Allocator propagating on copy assignment but retaining ownership on move assignment. */
    template < typename T >
    struct copy_propagating_allocator
    {
        using value_type = T;
        using propagate_on_container_copy_assignment = std::true_type;
        failing_resource* resource;
        /** @brief Selects the allocation resource. */
        explicit copy_propagating_allocator(failing_resource* input) noexcept : resource(input)
        {
        }
        /** @brief Rebinds while preserving resource identity. */
        template < typename U >
        copy_propagating_allocator(copy_propagating_allocator< U > const& other) noexcept : resource(other.resource)
        {
        }
        /** @brief Allocates aligned storage for the requested object count. */
        T* allocate(std::size_t count)
        {
            return static_cast< T* >(resource->allocate(count * sizeof(T), alignof(T)));
        }
        /** @brief Releases storage through its owning resource. */
        void deallocate(T* pointer, std::size_t count) noexcept
        {
            resource->deallocate(pointer, count * sizeof(T), alignof(T));
        }
        /** @brief Compares allocation ownership. */
        template < typename U >
        bool operator==(copy_propagating_allocator< U > const& other) const noexcept
        {
            return resource == other.resource;
        }
    };
} // namespace inline_hadix_tests
TEST(inline_hadix_map, copy_propagation_replaces_bucket_allocator)
{
    using allocator = inline_hadix_tests::copy_propagating_allocator< std::pair< std::size_t const, std::size_t > >;
    using map_type = rpnx::inline_hadix_map< std::size_t, std::size_t, inline_hadix_tests::grouped_hash, std::compare_three_way, allocator >;
    inline_hadix_tests::failing_resource first;
    inline_hadix_tests::failing_resource second;
    {
        map_type source{allocator(&first)};
        map_type destination{allocator(&second)};
        for (std::size_t i = 0; i < 200; ++i)
        {
            source.try_emplace(i, i);
            destination.try_emplace(i + 200, i);
        }
        destination = source;
        EXPECT_EQ(destination.get_allocator().resource, &first);
        EXPECT_EQ(second.blocks, 0U);
        for (std::size_t i = 0; i < 200; ++i)
        {
            EXPECT_EQ(destination.at(i), i);
        }
        destination = destination;
        EXPECT_EQ(destination.size(), 200U);
    }
    EXPECT_EQ(first.blocks, 0U);
    EXPECT_EQ(second.blocks, 0U);
}

namespace inline_hadix_tests
{
    /** @brief Exercises extended alignment together with tracked value lifetimes. */
    struct alignas(128) aligned_value : tracked_value
    {
        using tracked_value::tracked_value;
    };
} // namespace inline_hadix_tests
TEST(inline_hadix_map, extended_alignment_and_lifetimes_survive_relocation)
{
    using map_type = rpnx::inline_hadix_map< std::size_t, inline_hadix_tests::aligned_value, inline_hadix_tests::grouped_hash >;
    ASSERT_EQ(inline_hadix_tests::tracked_value::live, 0U);
    {
        map_type values;
        for (std::size_t i = 0; i < 512; ++i)
        {
            values.try_emplace(i, i);
        }
        map_type moved(std::move(values));
        for (std::size_t i = 0; i < 512; i += 2)
        {
            EXPECT_EQ(moved.erase(i), 1U);
        }
        for (std::size_t i = 1; i < 512; i += 2)
        {
            EXPECT_EQ(moved.at(i).value, i);
            EXPECT_EQ(reinterpret_cast< std::uintptr_t >(std::addressof(moved.at(i))) % 128, 0U);
        }
        EXPECT_EQ(inline_hadix_tests::tracked_value::live, 256U);
        moved.clear();
        EXPECT_EQ(inline_hadix_tests::tracked_value::live, 0U);
    }
    EXPECT_EQ(inline_hadix_tests::tracked_value::live, 0U);
}
namespace inline_hadix_tests
{
    /** @brief Varies all fingerprint bits while keeping keys in the same bucket. */
    struct fingerprint_hash
    {
        /** @brief Places the byte-sized key at the opposite end from bucket indexing. */
        std::size_t operator()(std::size_t key) const noexcept
        {
            return key << (std::numeric_limits< std::size_t >::digits - 8);
        }
    };
} // namespace inline_hadix_tests
TEST(inline_hadix_map, every_fingerprint_survives_full_and_partial_groups)
{
    for (std::size_t first = 0; first < 256; ++first)
    {
        rpnx::inline_hadix_map< std::size_t, std::size_t, inline_hadix_tests::fingerprint_hash > values;
        for (std::size_t i = 0; i < 16; ++i)
        {
            std::size_t key = (first + i) % 256;
            values.try_emplace(key, key);
        }
        for (std::size_t key = 0; key < 256; ++key)
        {
            EXPECT_EQ(values.contains(key), (key + 256 - first) % 256 < 16);
        }
        for (std::size_t i = 0; i < 16; ++i)
        {
            std::size_t key = (first + i) % 256;
            EXPECT_EQ(values.at(key), key);
            EXPECT_EQ(values.erase(key), 1U);
            EXPECT_FALSE(values.contains(key));
        }
    }
}
