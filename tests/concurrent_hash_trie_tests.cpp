// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0
#include <rpnx/concurrent_hash_trie.hpp>

#include <algorithm>
#include <atomic>
#include <barrier>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <gtest/gtest.h>
#include <latch>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace concurrent_trie_tests
{
    /** @brief Preserves every bit of the input key in its routing hash. */
    struct identity_hash
    {
        /** @brief Returns the input as an architecture-independent 64-bit hash. */
        std::uint64_t operator()(std::uint64_t key) const noexcept
        { return key; }
    };

    /** @brief Exercises zero-extension of a narrower hash result. */
    struct narrow_hash
    {
        /** @brief Returns the low 32 bits of the input. */
        std::uint32_t operator()(std::uint64_t key) const noexcept
        { return static_cast< std::uint32_t >(key); }
    };

    /** @brief Distinguishes const and mutable calls to the same hash policy. */
    struct observed_hash
    {
        std::shared_ptr< std::atomic< std::size_t > > const_calls;
        std::shared_ptr< std::atomic< std::size_t > > mutable_calls;

        /** @brief Hashes through the const policy interface used by concurrent operations. */
        std::uint64_t operator()(std::uint64_t key) const noexcept
        {
            const_calls->fetch_add(1, std::memory_order_relaxed);
            return key;
        }

        /** @brief Records a mutable invocation of the same hash function. */
        std::uint64_t operator()(std::uint64_t key) noexcept
        {
            mutable_calls->fetch_add(1, std::memory_order_relaxed);
            return key;
        }
    };

    /** @brief Places every key in one complete-hash collision group. */
    struct constant_hash
    {
        /** @brief Returns one shared routing hash. */
        std::uint64_t operator()(std::uint64_t) const noexcept
        { return 0; }
    };

    /** @brief Counts ordering evaluations while retaining ordinary integer equivalence. */
    struct counted_compare
    {
        std::shared_ptr< std::atomic< std::size_t > > calls;

        /** @brief Records one comparison and returns integer ordering. */
        std::strong_ordering operator()(std::uint64_t left, std::uint64_t right) const noexcept
        {
            calls->fetch_add(1, std::memory_order_relaxed);
            return left <=> right;
        }
    };

    /** @brief Tracks routing and entry allocations across every allocator rebind. */
    struct allocation_state
    {
        std::mutex mutex;
        std::unordered_map< void*, std::size_t > blocks;
        std::size_t bytes = 0;
        bool mismatched_deallocation = false;
        std::optional< std::size_t > allocation_budget;
        std::optional< std::size_t > construction_budget;
    };

    /** @brief Stateful allocator supporting synchronized ownership auditing and failure injection. */
    template < typename T >
    struct audit_allocator
    {
        using value_type = T;
        std::shared_ptr< allocation_state > state;

        /** @brief Creates private audit state. */
        audit_allocator() : state(std::make_shared< allocation_state >())
        {
        }

        /** @brief Uses supplied shared audit state. */
        explicit audit_allocator(std::shared_ptr< allocation_state > input) noexcept : state(std::move(input))
        {
        }

        /** @brief Preserves audit ownership when rebinding. */
        template < typename U >
        audit_allocator(audit_allocator< U > const& other) noexcept : state(other.state)
        {
        }

        /** @brief Allocates storage and records its size after applying the shared budget. */
        T* allocate(std::size_t count)
        {
            std::lock_guard< std::mutex > lock(state->mutex);
            if (state->allocation_budget)
            {
                if (*state->allocation_budget == 0)
                {
                    throw std::bad_alloc();
                }
                --*state->allocation_budget;
            }
            T* result = std::allocator< T >().allocate(count);
            try
            {
                state->blocks.emplace(result, count * sizeof(T));
            }
            catch (...)
            {
                std::allocator< T >().deallocate(result, count);
                throw;
            }
            state->bytes += count * sizeof(T);
            return result;
        }

        /** @brief Verifies allocation ownership and releases its storage. */
        void deallocate(T* address, std::size_t count) noexcept
        {
            std::lock_guard< std::mutex > lock(state->mutex);
            auto found = state->blocks.find(address);
            if (found == state->blocks.end() || found->second != count * sizeof(T))
            {
                state->mismatched_deallocation = true;
            }
            else
            {
                state->bytes -= found->second;
                state->blocks.erase(found);
            }
            std::allocator< T >().deallocate(address, count);
        }

        /** @brief Constructs an object after applying the shared construction budget. */
        template < typename U, typename... Args >
        void construct(U* address, Args&&... args)
        {
            {
                std::lock_guard< std::mutex > lock(state->mutex);
                if (state->construction_budget)
                {
                    if (*state->construction_budget == 0)
                    {
                        throw std::runtime_error("injected construction failure");
                    }
                    --*state->construction_budget;
                }
            }
            std::construct_at(address, std::forward< Args >(args)...);
        }

        /** @brief Destroys a previously constructed object. */
        template < typename U >
        void destroy(U* address) noexcept
        { std::destroy_at(address); }

        /** @brief Compares shared allocation ownership. */
        template < typename U >
        bool operator==(audit_allocator< U > const& other) const noexcept
        { return state == other.state; }
    };

    /** @brief Detects relocation of values stored at stable addresses. */
    struct immovable
    {
        std::uint64_t value;

        /** @brief Constructs the stored value in place. */
        explicit immovable(std::uint64_t input) : value(input)
        {
        }

        /** @brief Prevents copying the stored value. */
        immovable(immovable const&) = delete;
        /** @brief Prevents relocating the stored value. */
        immovable(immovable&&) = delete;
        /** @brief Prevents replacing the stored value through copying. */
        immovable& operator=(immovable const&) = delete;
        /** @brief Prevents replacing the stored value through moving. */
        immovable& operator=(immovable&&) = delete;
    };

    /** @brief Counts actual mapped construction when insertions race. */
    struct counted_value
    {
        std::uint64_t value;

        /** @brief Records one in-place construction. */
        counted_value(std::atomic< std::size_t >* constructed, std::uint64_t input) : value(input)
        { constructed->fetch_add(1, std::memory_order_relaxed); }

        /** @brief Prevents additional construction through copying. */
        counted_value(counted_value const&) = delete;
        /** @brief Prevents additional construction through moving. */
        counted_value(counted_value&&) = delete;
    };

    /** @brief Hashes the logical value of a move-only key. */
    struct indirect_hash
    {
        /** @brief Returns the pointed-to integer as its routing hash. */
        std::uint64_t operator()(std::unique_ptr< std::uint64_t > const& key) const noexcept
        { return *key; }
    };

    /** @brief Orders move-only keys by their pointed-to integer. */
    struct indirect_compare
    {
        /** @brief Compares logical key values independently of pointer identity. */
        std::strong_ordering operator()(std::unique_ptr< std::uint64_t > const& left, std::unique_ptr< std::uint64_t > const& right) const noexcept
        { return *left <=> *right; }
    };

    /** @brief Separates logical key equivalence from its stored representation. */
    struct represented_key
    {
        std::uint64_t identity;
        std::uint64_t representation;
    };

    /** @brief Hashes equivalent represented keys identically. */
    struct represented_hash
    {
        /** @brief Returns an identity-derived hash with deliberate collisions. */
        std::uint64_t operator()(represented_key const& key) const noexcept
        { return key.identity % 4; }
    };

    /** @brief Defines equivalence independently of stored representation. */
    struct represented_compare
    {
        /** @brief Orders represented keys by logical identity. */
        std::strong_ordering operator()(represented_key const& left, represented_key const& right) const noexcept
        { return left.identity <=> right.identity; }
    };

    /** @brief Controls throwing hash and ordering policies during quiescent tests. */
    struct policy_state
    {
        bool reject_hash = false;
        bool reject_compare = false;
        std::uint64_t allowed_key = 0;
    };

    /** @brief Rejects hashing retained keys when testing structural operations. */
    struct controlled_hash
    {
        std::shared_ptr< policy_state > state;

        /** @brief Hashes only permitted keys while rejection is enabled. */
        std::uint64_t operator()(std::uint64_t key) const
        {
            if (state->reject_hash && key != state->allowed_key)
            {
                throw std::runtime_error("rejected key hash");
            }
            return key;
        }
    };

    /** @brief Supports failure injection inside ordered collision operations. */
    struct controlled_compare
    {
        std::shared_ptr< policy_state > state;

        /** @brief Compares keys or throws when comparison rejection is enabled. */
        std::strong_ordering operator()(std::uint64_t left, std::uint64_t right) const
        {
            if (state->reject_compare)
            {
                throw std::runtime_error("rejected key comparison");
            }
            return left <=> right;
        }
    };
} // namespace concurrent_trie_tests

static_assert(std::forward_iterator< rpnx::concurrent_hash_trie< std::uint64_t, std::uint64_t >::iterator >);
static_assert(std::forward_iterator< rpnx::concurrent_hash_trie< std::uint64_t, std::uint64_t >::const_iterator >);
static_assert(!std::is_copy_constructible_v< rpnx::concurrent_hash_trie< std::uint64_t, std::uint64_t > >);
static_assert(!std::is_move_constructible_v< rpnx::concurrent_hash_trie< std::uint64_t, std::uint64_t > >);
static_assert(std::is_same_v< decltype(std::declval< rpnx::concurrent_hash_trie< std::uint64_t, std::uint64_t > const& >().find(0)), std::pair< std::uint64_t const, std::uint64_t > const* >);

TEST(concurrent_hash_trie, common_api_and_const_access)
{
    rpnx::concurrent_hash_trie< std::string, std::uint64_t > values;
    EXPECT_EQ(values.find("absent"), nullptr);
    EXPECT_FALSE(values.contains("absent"));
    EXPECT_EQ(values.estimate_size(), 0U);
    EXPECT_EQ(values.size_exclusive(), 0U);
    EXPECT_THROW(values.at("absent"), std::out_of_range);
    EXPECT_TRUE(values.try_emplace("one", 1).second);
    EXPECT_FALSE(values.try_emplace("one", 99).second);
    EXPECT_FALSE(values.insert_or_assign("one", 10).second);
    EXPECT_TRUE(values.insert_or_assign("two", 20).second);
    values["three"] = 30;
    std::string key = "four";
    values[key] = 40;
    EXPECT_EQ(values.at("one"), 10U);
    EXPECT_EQ(values.find("two")->second, 20U);
    rpnx::concurrent_hash_trie< std::string, std::uint64_t > const& immutable = values;
    EXPECT_EQ(immutable.at("three"), 30U);
    EXPECT_EQ(immutable.find("four")->second, 40U);
    EXPECT_TRUE(immutable.contains("one"));
    EXPECT_THROW(immutable.at("absent"), std::out_of_range);
    EXPECT_EQ(values.erase("one"), 1U);
    EXPECT_EQ(values.erase("one"), 0U);
    EXPECT_EQ(values.estimate_size(), 3U);
    EXPECT_EQ(values.size_exclusive(), 3U);
    values.clear();
    EXPECT_EQ(values.size_exclusive(), 0U);
    EXPECT_EQ(values.range_exclusive().begin(), values.range_exclusive().end());
    EXPECT_TRUE(values.try_emplace("again", 42).second);
}

TEST(concurrent_hash_trie, callbacks_and_exception_release)
{
    rpnx::concurrent_hash_trie< std::uint64_t, std::uint64_t > values;
    values.try_emplace(7, 42);
    bool called = false;
    EXPECT_FALSE(values.with_read(8,
                                  [&](decltype(values)::const_reference)
                                  {
                                      called = true;
                                  }));
    EXPECT_FALSE(values.with_write(8,
                                   [&](auto&)
                                   {
                                       called = true;
                                   }));
    EXPECT_FALSE(called);
    EXPECT_TRUE(values.with_read(7,
                                 [&](decltype(values)::const_reference entry)
                                 {
                                     EXPECT_EQ(entry.second, 42U);
                                 }));
    EXPECT_TRUE(values.with_write(7,
                                  [](auto& entry)
                                  {
                                      ++entry.second;
                                  }));
    EXPECT_THROW(values.with_write(7,
                                   [](auto& entry)
                                   {
                                       ++entry.second;
                                       throw std::runtime_error("callback failure");
                                   }),
                 std::runtime_error);
    EXPECT_THROW(values.with_read(7,
                                  [](decltype(values)::const_reference)
                                  {
                                      throw std::runtime_error("callback failure");
                                  }),
                 std::runtime_error);
    EXPECT_EQ(values.at(7), 44U);
    EXPECT_EQ(values.erase(7), 1U);
}

TEST(concurrent_hash_trie, narrow_hashes_and_upper_hash_bits)
{
    rpnx::concurrent_hash_trie< std::uint64_t, std::uint64_t, concurrent_trie_tests::narrow_hash > narrow;
    rpnx::concurrent_hash_trie< std::uint64_t, std::uint64_t, concurrent_trie_tests::identity_hash > full;
    for (std::uint64_t index = 0; index < 128; ++index)
    {
        std::uint64_t key = (index << 40) | 7;
        EXPECT_TRUE(narrow.try_emplace(key, index).second);
        EXPECT_TRUE(full.try_emplace(key, index).second);
    }
    for (std::uint64_t index = 0; index < 128; ++index)
    {
        std::uint64_t key = (index << 40) | 7;
        EXPECT_EQ(narrow.at(key), index);
        EXPECT_EQ(full.at(key), index);
    }
    EXPECT_EQ(narrow.size_exclusive(), 128U);
    EXPECT_EQ(full.size_exclusive(), 128U);
}

TEST(concurrent_hash_trie, hash_policy_uses_const_invocation)
{
    auto const_calls = std::make_shared< std::atomic< std::size_t > >(0);
    auto mutable_calls = std::make_shared< std::atomic< std::size_t > >(0);
    rpnx::concurrent_hash_trie< std::uint64_t, std::uint64_t, concurrent_trie_tests::observed_hash > values({const_calls, mutable_calls});
    values.try_emplace(7, 42);
    EXPECT_TRUE(values.contains(7));
    EXPECT_NE(values.find(7), nullptr);
    EXPECT_EQ(values.erase(7), 1U);
    EXPECT_GT(const_calls->load(), 0U);
    EXPECT_EQ(mutable_calls->load(), 0U);
}

TEST(concurrent_hash_trie, all_routing_bytes_grow_and_shrink_with_stable_values)
{
    for (std::uint8_t depth = 0; depth < 8; ++depth)
    {
        rpnx::concurrent_hash_trie< std::uint64_t, concurrent_trie_tests::immovable, concurrent_trie_tests::identity_hash > values;
        std::uint64_t retained_key = std::uint64_t{255} << (depth * 8);
        auto* retained = values.try_emplace(retained_key, 255).first;
        for (std::uint64_t edge = 0; edge < 255; ++edge)
        {
            values.try_emplace(edge << (depth * 8), edge);
        }
        EXPECT_EQ(values.size_exclusive(), 256U);
        for (std::uint64_t edge = 0; edge < 256; ++edge)
        {
            EXPECT_EQ(values.at(edge << (depth * 8)).value, edge);
        }
        for (std::uint64_t edge = 0; edge < 255; ++edge)
        {
            EXPECT_EQ(values.erase(edge << (depth * 8)), 1U);
            EXPECT_EQ(values.find(retained_key), retained);
        }
        EXPECT_EQ(values.at(retained_key).value, 255U);
        EXPECT_EQ(values.size_exclusive(), 1U);
        EXPECT_EQ(values.erase(retained_key), 1U);
        EXPECT_EQ(values.estimate_size(), 0U);
    }
}

TEST(concurrent_hash_trie, compressed_prefix_splits_and_deep_iteration)
{
    rpnx::concurrent_hash_trie< std::uint64_t, std::uint64_t, concurrent_trie_tests::identity_hash > values;
    auto* retained = values.try_emplace(0, 0).first;
    for (std::size_t remaining = 8; remaining > 0; --remaining)
    {
        std::uint64_t key = std::uint64_t{1} << ((remaining - 1) * 8);
        values.try_emplace(key, key);
    }
    std::set< std::uint64_t > visited;
    auto range = values.range_exclusive();
    auto first = range.begin();
    auto copy = first;
    EXPECT_EQ(first, copy);
    auto previous = copy++;
    EXPECT_EQ(first, previous);
    for (decltype(values)::reference entry : range)
    {
        EXPECT_TRUE(visited.insert(entry.first).second);
        ++entry.second;
    }
    EXPECT_EQ(visited.size(), 9U);
    typename decltype(values)::const_iterator converted = first;
    EXPECT_EQ(converted, first);
    rpnx::concurrent_hash_trie< std::uint64_t, std::uint64_t, concurrent_trie_tests::identity_hash > const& immutable = values;
    for (decltype(values)::const_reference entry : immutable.range_exclusive())
    {
        EXPECT_EQ(entry.second, entry.first + 1);
    }
    for (std::uint8_t depth = 0; depth < 8; ++depth)
    {
        EXPECT_EQ(values.erase(std::uint64_t{1} << (depth * 8)), 1U);
        EXPECT_EQ(values.find(0), retained);
    }
    EXPECT_EQ(values.at(0), 1U);
}

TEST(concurrent_hash_trie, collision_comparisons_have_logarithmic_growth)
{
    auto calls = std::make_shared< std::atomic< std::size_t > >(0);
    rpnx::concurrent_hash_trie< std::uint64_t, std::uint64_t, concurrent_trie_tests::constant_hash, concurrent_trie_tests::counted_compare > values({}, {calls});
    for (std::uint64_t key = 0; key < 2048; ++key)
    {
        values.try_emplace(key, key * 3);
    }
    for (std::uint64_t key = 0; key <= 2048; key += 7)
    {
        calls->store(0);
        bool present = values.contains(key);
        EXPECT_EQ(present, key < 2048);
        EXPECT_LE(calls->load(), 6 * std::bit_width(std::uint64_t{2049}) + 8);
    }
    for (std::uint64_t key = 0; key < 2048; key += 2)
    {
        EXPECT_EQ(values.erase(key), 1U);
    }
    EXPECT_EQ(values.estimate_size(), 1024U);
}

TEST(concurrent_hash_trie, comparator_defines_equivalence)
{
    rpnx::concurrent_hash_trie< concurrent_trie_tests::represented_key, std::uint64_t, concurrent_trie_tests::represented_hash, concurrent_trie_tests::represented_compare > values;
    for (std::uint64_t key = 0; key < 64; ++key)
    {
        EXPECT_TRUE(values.try_emplace({key, 10}, key).second);
        EXPECT_FALSE(values.try_emplace({key, 20}, 999).second);
        EXPECT_EQ(values.find({key, 30})->first.representation, 10U);
    }
    EXPECT_EQ(values.size_exclusive(), 64U);
    for (std::uint64_t key = 0; key < 64; ++key)
    {
        EXPECT_EQ(values.erase({key, 40}), 1U);
    }
}

TEST(concurrent_hash_trie, equivalent_zero_values_use_one_entry)
{
    rpnx::concurrent_hash_trie< double, std::uint64_t > values;
    EXPECT_TRUE(values.try_emplace(-0.0, 1).second);
    EXPECT_FALSE(values.try_emplace(0.0, 2).second);
    EXPECT_EQ(values.at(0.0), 1U);
}

TEST(concurrent_hash_trie, duplicate_insertion_preserves_move_only_arguments)
{
    rpnx::concurrent_hash_trie< std::string, std::unique_ptr< std::uint64_t > > values;
    values.try_emplace("key", std::make_unique< std::uint64_t >(42));
    std::string key = "key";
    auto candidate = std::make_unique< std::uint64_t >(99);
    EXPECT_FALSE(values.try_emplace(std::move(key), std::move(candidate)).second);
    EXPECT_EQ(key, "key");
    ASSERT_TRUE(candidate);
    EXPECT_EQ(*candidate, 99U);
    EXPECT_EQ(*values.at("key"), 42U);
}

TEST(concurrent_hash_trie, move_only_keys_and_immovable_values)
{
    rpnx::concurrent_hash_trie< std::unique_ptr< std::uint64_t >, concurrent_trie_tests::immovable, concurrent_trie_tests::indirect_hash, concurrent_trie_tests::indirect_compare > values;
    for (std::uint64_t key = 0; key < 256; ++key)
    {
        EXPECT_TRUE(values.try_emplace(std::make_unique< std::uint64_t >(key), key * 3).second);
    }
    auto duplicate = std::make_unique< std::uint64_t >(42);
    EXPECT_FALSE(values.try_emplace(std::move(duplicate), 999).second);
    ASSERT_TRUE(duplicate);
    EXPECT_EQ(values.at(duplicate).value, 126U);
    EXPECT_EQ(values.erase(duplicate), 1U);
    EXPECT_EQ(values.size_exclusive(), 255U);
}

TEST(concurrent_hash_trie, randomized_mutations_match_ordered_oracle)
{
    rpnx::concurrent_hash_trie< std::uint64_t, std::uint64_t, concurrent_trie_tests::identity_hash > values;
    std::map< std::uint64_t, std::uint64_t > expected;
    std::mt19937_64 random(12345);
    for (std::size_t iteration = 0; iteration < 10000; ++iteration)
    {
        std::uint64_t key = random() % 1024;
        key = (key << 40) | (key % 16);
        switch (random() % 3)
        {
        case 0:
            EXPECT_EQ(values.try_emplace(key, iteration).second, expected.try_emplace(key, iteration).second);
            break;
        case 1:
            EXPECT_EQ(values.erase(key), expected.erase(key));
            break;
        default:
            EXPECT_EQ(values.insert_or_assign(key, iteration).second, expected.insert_or_assign(key, iteration).second);
            break;
        }
    }
    std::map< std::uint64_t, std::uint64_t > actual;
    for (decltype(values)::const_reference entry : values.range_exclusive())
    {
        EXPECT_TRUE(actual.insert(entry).second);
    }
    EXPECT_EQ(actual, expected);
    EXPECT_EQ(values.size_exclusive(), expected.size());
    EXPECT_EQ(values.estimate_size(), expected.size());
}

TEST(concurrent_hash_trie, erasure_reclaims_all_allocator_owned_storage)
{
    auto state = std::make_shared< concurrent_trie_tests::allocation_state >();
    using allocator = concurrent_trie_tests::audit_allocator< std::pair< std::uint64_t const, concurrent_trie_tests::immovable > >;
    {
        rpnx::concurrent_hash_trie< std::uint64_t, concurrent_trie_tests::immovable, concurrent_trie_tests::identity_hash, std::compare_three_way, allocator > values({}, {}, allocator(state));
        EXPECT_EQ(values.get_allocator().state, state);
        for (std::uint64_t key = 0; key < 512; ++key)
        {
            values.try_emplace(key, key);
        }
        std::size_t full_bytes = state->bytes;
        auto* retained = values.find(511);
        for (std::uint64_t key = 0; key < 511; ++key)
        {
            values.erase(key);
        }
        EXPECT_LT(state->bytes, full_bytes / 2);
        EXPECT_EQ(values.find(511), retained);
        EXPECT_EQ(values.erase(511), 1U);
        EXPECT_EQ(state->bytes, 0U);
        EXPECT_TRUE(state->blocks.empty());
        values.try_emplace(42, 42);
    }
    EXPECT_EQ(state->bytes, 0U);
    EXPECT_TRUE(state->blocks.empty());
    EXPECT_FALSE(state->mismatched_deallocation);
}

TEST(concurrent_hash_trie, insertion_allocation_failures_preserve_entries)
{
    using allocator = concurrent_trie_tests::audit_allocator< std::pair< std::uint64_t const, std::uint64_t > >;
    for (std::size_t initial : {std::size_t{1}, std::size_t{4}, std::size_t{16}, std::size_t{48}})
    {
        for (std::size_t budget = 0; budget < 7; ++budget)
        {
            auto state = std::make_shared< concurrent_trie_tests::allocation_state >();
            {
                rpnx::concurrent_hash_trie< std::uint64_t, std::uint64_t, concurrent_trie_tests::identity_hash, std::compare_three_way, allocator > values({}, {}, allocator(state));
                for (std::uint64_t key = 0; key < initial; ++key)
                {
                    values.try_emplace(key, key * 3);
                }
                auto* retained = values.find(0);
                state->allocation_budget = budget;
                bool inserted = false;
                try
                {
                    inserted = values.try_emplace(initial, 999).second;
                }
                catch (std::bad_alloc const&)
                {
                }
                state->allocation_budget.reset();
                EXPECT_EQ(values.find(0), retained);
                EXPECT_EQ(values.at(0), 0U);
                EXPECT_EQ(values.size_exclusive(), initial + (inserted ? 1 : 0));
                EXPECT_EQ(values.contains(initial), inserted);
                EXPECT_TRUE(values.try_emplace(1000, 1000).second);
                values.clear();
                EXPECT_TRUE(state->blocks.empty());
            }
            EXPECT_TRUE(state->blocks.empty());
            EXPECT_FALSE(state->mismatched_deallocation);
        }
    }
}

TEST(concurrent_hash_trie, construction_failures_leave_reusable_routing)
{
    using allocator = concurrent_trie_tests::audit_allocator< std::pair< std::uint64_t const, std::uint64_t > >;
    for (std::size_t budget = 0; budget < 7; ++budget)
    {
        auto state = std::make_shared< concurrent_trie_tests::allocation_state >();
        {
            rpnx::concurrent_hash_trie< std::uint64_t, std::uint64_t, concurrent_trie_tests::identity_hash, std::compare_three_way, allocator > values({}, {}, allocator(state));
            values.try_emplace(0, 42);
            state->construction_budget = budget;
            bool inserted = false;
            try
            {
                inserted = values.try_emplace(256, 256).second;
            }
            catch (std::runtime_error const&)
            {
            }
            state->construction_budget.reset();
            EXPECT_EQ(values.at(0), 42U);
            EXPECT_EQ(values.size_exclusive(), 1U + (inserted ? 1U : 0U));
            EXPECT_TRUE(values.try_emplace(512, 512).second);
        }
        EXPECT_TRUE(state->blocks.empty());
        EXPECT_FALSE(state->mismatched_deallocation);
    }
}

TEST(concurrent_hash_trie, shrinking_failure_preserves_committed_erasure)
{
    auto state = std::make_shared< concurrent_trie_tests::allocation_state >();
    using allocator = concurrent_trie_tests::audit_allocator< std::pair< std::uint64_t const, std::uint64_t > >;
    {
        rpnx::concurrent_hash_trie< std::uint64_t, std::uint64_t, concurrent_trie_tests::identity_hash, std::compare_three_way, allocator > values({}, {}, allocator(state));
        for (std::uint64_t key = 0; key < 49; ++key)
        {
            values.try_emplace(key, key);
        }
        auto* retained = values.find(48);
        for (std::uint64_t key = 0; key < 11; ++key)
        {
            values.erase(key);
        }
        state->allocation_budget = 0;
        EXPECT_THROW(values.erase(11), std::bad_alloc);
        state->allocation_budget.reset();
        EXPECT_FALSE(values.contains(11));
        EXPECT_EQ(values.size_exclusive(), 37U);
        EXPECT_EQ(values.find(48), retained);
        for (std::uint64_t key = 12; key < 49; ++key)
        {
            EXPECT_EQ(values.erase(key), 1U);
        }
        EXPECT_TRUE(state->blocks.empty());
    }
    EXPECT_FALSE(state->mismatched_deallocation);
}

TEST(concurrent_hash_trie, structural_changes_do_not_rehash_retained_keys)
{
    auto state = std::make_shared< concurrent_trie_tests::policy_state >();
    rpnx::concurrent_hash_trie< std::uint64_t, std::uint64_t, concurrent_trie_tests::controlled_hash > values({state});
    for (std::uint64_t key = 0; key < 256; ++key)
    {
        state->allowed_key = key;
        state->reject_hash = true;
        EXPECT_TRUE(values.try_emplace(key, key).second);
    }
    state->allowed_key = 999;
    EXPECT_THROW(values.contains(0), std::runtime_error);
    for (std::uint64_t key = 0; key < 256; ++key)
    {
        state->allowed_key = key;
        EXPECT_EQ(values.erase(key), 1U);
    }
    EXPECT_EQ(values.size_exclusive(), 0U);
}

TEST(concurrent_hash_trie, throwing_comparison_preserves_collision_entries)
{
    auto state = std::make_shared< concurrent_trie_tests::policy_state >();
    rpnx::concurrent_hash_trie< std::uint64_t, std::uint64_t, concurrent_trie_tests::constant_hash, concurrent_trie_tests::controlled_compare > values({}, {state});
    values.try_emplace(1, 10);
    values.try_emplace(2, 20);
    state->reject_compare = true;
    EXPECT_THROW(values.try_emplace(3, 30), std::runtime_error);
    EXPECT_THROW(values.erase(1), std::runtime_error);
    EXPECT_THROW(values.contains(2), std::runtime_error);
    state->reject_compare = false;
    EXPECT_EQ(values.at(1), 10U);
    EXPECT_EQ(values.at(2), 20U);
    EXPECT_EQ(values.size_exclusive(), 2U);
}

TEST(concurrent_hash_trie, racing_insertion_constructs_one_value)
{
    rpnx::concurrent_hash_trie< std::uint64_t, concurrent_trie_tests::counted_value > values;
    std::atomic< std::size_t > constructed{0};
    std::atomic< std::size_t > inserted{0};
    std::latch start(16);
    std::vector< std::thread > threads;
    for (std::uint64_t index = 0; index < 16; ++index)
    {
        threads.emplace_back(
            [&, index]
            {
                start.arrive_and_wait();
                if (values.try_emplace(7, &constructed, index).second)
                {
                    inserted.fetch_add(1, std::memory_order_relaxed);
                }
            });
    }
    for (std::thread& thread : threads)
    {
        thread.join();
    }
    EXPECT_EQ(inserted.load(), 1U);
    EXPECT_EQ(constructed.load(), 1U);
    EXPECT_LT(values.at(7).value, 16U);
}

TEST(concurrent_hash_trie, locked_updates_do_not_lose_writes)
{
    rpnx::concurrent_hash_trie< std::uint64_t, std::uint64_t > values;
    values.try_emplace(7, 0);
    std::latch start(8);
    std::vector< std::thread > threads;
    for (std::size_t index = 0; index < 8; ++index)
    {
        threads.emplace_back(
            [&]
            {
                start.arrive_and_wait();
                for (std::size_t iteration = 0; iteration < 1000; ++iteration)
                {
                    values.with_write(7,
                                      [](auto& entry)
                                      {
                                          ++entry.second;
                                      });
                }
            });
    }
    for (std::thread& thread : threads)
    {
        thread.join();
    }
    EXPECT_EQ(values.at(7), 8000U);
}

TEST(concurrent_hash_trie, distinct_branch_updates_and_publication_progress)
{
    rpnx::concurrent_hash_trie< std::uint64_t, std::uint64_t, concurrent_trie_tests::identity_hash > values;
    values.try_emplace(0, 0);
    values.try_emplace(1, 1);
    std::promise< void > entered;
    std::future< void > ready = entered.get_future();
    std::latch release(1);
    std::thread holder(
        [&]
        {
            values.with_write(0,
                              [&](auto&)
                              {
                                  entered.set_value();
                                  release.wait();
                              });
        });
    ready.wait();
    std::future< bool > peer = std::async(std::launch::async,
                                          [&]
                                          {
                                              bool updated = values.with_write(1,
                                                                               [](auto& entry)
                                                                               {
                                                                                   ++entry.second;
                                                                               });
                                              return values.try_emplace(2, 2).second && updated;
                                          });
    std::future_status status = peer.wait_for(std::chrono::seconds(5));
    release.count_down();
    holder.join();
    EXPECT_TRUE(peer.get());
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_EQ(values.at(1), 2U);
}

TEST(concurrent_hash_trie, erasure_waits_for_protected_reader)
{
    rpnx::concurrent_hash_trie< std::uint64_t, std::uint64_t > values;
    values.try_emplace(7, 42);
    std::promise< void > entered;
    std::future< void > ready = entered.get_future();
    std::latch release(1);
    std::thread reader(
        [&]
        {
            values.with_read(7,
                             [&](decltype(values)::const_reference entry)
                             {
                                 entered.set_value();
                                 release.wait();
                                 EXPECT_EQ(entry.second, 42U);
                             });
        });
    ready.wait();
    std::future< std::size_t > erased = std::async(std::launch::async,
                                                   [&]
                                                   {
                                                       return values.erase(7);
                                                   });
    std::future_status status = erased.wait_for(std::chrono::milliseconds(20));
    release.count_down();
    reader.join();
    EXPECT_EQ(erased.get(), 1U);
    EXPECT_EQ(status, std::future_status::timeout);
    EXPECT_FALSE(values.contains(7));
}

TEST(concurrent_hash_trie, concurrent_restructuring_preserves_protected_values)
{
    rpnx::concurrent_hash_trie< std::uint64_t, std::pair< std::uint64_t, std::uint64_t >, concurrent_trie_tests::identity_hash > values;
    std::atomic< std::size_t > writers_remaining{4};
    std::atomic< bool > inconsistent{false};
    std::latch start(7);
    std::vector< std::thread > threads;
    for (std::uint64_t writer = 0; writer < 4; ++writer)
    {
        threads.emplace_back(
            [&, writer]
            {
                start.arrive_and_wait();
                for (std::uint64_t round = 0; round < 4; ++round)
                {
                    for (std::uint64_t index = 0; index < 256; ++index)
                    {
                        std::uint64_t identity = writer * 256 + index;
                        std::uint64_t key = (identity << 32) | (index % 16);
                        values.try_emplace(key, 0, 0);
                        values.with_write(key,
                                          [&](auto& entry)
                                          {
                                              entry.second.first = identity + round;
                                              entry.second.second = entry.second.first * 7;
                                          });
                        if (index % 3 == 0)
                        {
                            values.erase(key);
                        }
                    }
                }
                writers_remaining.fetch_sub(1, std::memory_order_relaxed);
            });
    }
    for (std::size_t reader = 0; reader < 2; ++reader)
    {
        threads.emplace_back(
            [&]
            {
                start.arrive_and_wait();
                do
                {
                    for (std::uint64_t identity = 0; identity < 1024; ++identity)
                    {
                        std::uint64_t key = (identity << 32) | (identity % 256 % 16);
                        values.with_read(key,
                                         [&](decltype(values)::const_reference entry)
                                         {
                                             if (entry.second.second != entry.second.first * 7)
                                             {
                                                 inconsistent.store(true, std::memory_order_relaxed);
                                             }
                                         });
                    }
                } while (writers_remaining.load(std::memory_order_relaxed) != 0);
            });
    }
    threads.emplace_back(
        [&]
        {
            start.arrive_and_wait();
            while (writers_remaining.load(std::memory_order_relaxed) != 0)
            {
                values.estimate_size();
                std::this_thread::yield();
            }
        });
    for (std::thread& thread : threads)
    {
        thread.join();
    }
    EXPECT_FALSE(inconsistent.load());
    EXPECT_EQ(values.size_exclusive(), 4U * (256U - 86U));
    EXPECT_EQ(values.estimate_size(), values.size_exclusive());
    std::size_t visited = 0;
    for (decltype(values)::const_reference entry : values.range_exclusive())
    {
        ++visited;
        EXPECT_EQ(entry.second.second, entry.second.first * 7);
        EXPECT_EQ(entry.second.first, (entry.first >> 32) + 3);
    }
    EXPECT_EQ(visited, values.size_exclusive());
}

TEST(concurrent_hash_trie, concurrent_publication_and_reclamation_across_layouts)
{
    auto state = std::make_shared< concurrent_trie_tests::allocation_state >();
    using allocator = concurrent_trie_tests::audit_allocator< std::pair< std::uint64_t const, std::uint64_t > >;
    rpnx::concurrent_hash_trie< std::uint64_t, std::uint64_t, concurrent_trie_tests::identity_hash, std::compare_three_way, allocator > values({}, {}, allocator(state));
    std::barrier phase(8);
    std::atomic< bool > incorrect{false};
    std::vector< std::thread > threads;
    for (std::uint64_t writer = 0; writer < 8; ++writer)
    {
        threads.emplace_back(
            [&, writer]
            {
                for (std::uint64_t round = 0; round < 6; ++round)
                {
                    phase.arrive_and_wait();
                    for (std::uint64_t step = 0; step < 256; ++step)
                    {
                        std::uint64_t edge = (step * 73 + writer * 19) % 256;
                        std::uint64_t key = (writer << 40) | edge;
                        values.try_emplace(key, round);
                        if (!values.with_read(key,
                                              [&](decltype(values)::const_reference entry)
                                              {
                                                  if (entry.second != round)
                                                  {
                                                      incorrect.store(true, std::memory_order_relaxed);
                                                  }
                                              }))
                        {
                            incorrect.store(true, std::memory_order_relaxed);
                        }
                        if (step % 3 == 0)
                        {
                            values.erase(key);
                        }
                    }
                    phase.arrive_and_wait();
                    for (std::uint64_t step = 0; step < 256; ++step)
                    {
                        std::uint64_t edge = (step * 151 + writer * 23) % 256;
                        values.erase((writer << 40) | edge);
                    }
                    phase.arrive_and_wait();
                }
            });
    }
    for (std::thread& thread : threads)
    {
        thread.join();
    }
    EXPECT_FALSE(incorrect.load());
    EXPECT_EQ(values.estimate_size(), 0U);
    EXPECT_EQ(values.range_exclusive().begin(), values.range_exclusive().end());
    EXPECT_TRUE(state->blocks.empty());
    EXPECT_EQ(state->bytes, 0U);
    EXPECT_FALSE(state->mismatched_deallocation);
    EXPECT_TRUE(values.try_emplace(42, 42).second);
}
