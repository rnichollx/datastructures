// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0
#include <rpnx/insert_only_concurrent_hash_trie.hpp>
#include <gtest/gtest.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace insert_only_trie_tests
{
    /** @brief Force every key into the collision index. */
    struct constant_hash
    {
        /** @brief Return a constant full hash. */
        std::uint64_t operator()(std::uint64_t) const { return 0; }
    };

    /** @brief Check concurrent factories and reads over overlapping key ranges. */
    template < typename Hash >
    void exercise_concurrent_insertion()
    {
        rpnx::insert_only_concurrent_hash_trie< std::uint64_t, std::uint64_t, Hash > map;
        std::atomic< std::size_t > constructions{0};
        std::atomic< bool > valid{true};
        std::vector< std::thread > threads;
        for (std::uint64_t thread = 0; thread < 8; ++thread)
        {
            threads.emplace_back([&, thread]
            {
                for (std::uint64_t i = 0; i < 4096; ++i)
                {
                    std::uint64_t key = (i * 37 + thread * 71) % 2048;
                    std::uint64_t value = map.get_or_create(key, [&]
                    {
                        constructions.fetch_add(1, std::memory_order_relaxed);
                        return key * 3;
                    });
                    if (value != key * 3 || map.at(key) != key * 3) { valid.store(false); }
                }
            });
        }
        for (std::thread& thread : threads) { thread.join(); }
        EXPECT_TRUE(valid.load());
        EXPECT_EQ(map.size_exclusive(), 2048);
        EXPECT_EQ(constructions.load(), 2048);
    }
}

TEST(insert_only_concurrent_hash_trie, concurrent_distinct_hashes)
{
    insert_only_trie_tests::exercise_concurrent_insertion< std::hash< std::uint64_t > >();
}

TEST(insert_only_concurrent_hash_trie, concurrent_full_hash_collisions)
{
    insert_only_trie_tests::exercise_concurrent_insertion< insert_only_trie_tests::constant_hash >();
}

TEST(insert_only_concurrent_hash_trie, stable_entries_and_duplicate_arguments)
{
    rpnx::insert_only_concurrent_hash_trie< std::uint64_t, std::unique_ptr< std::uint64_t > > map;
    auto original = map.try_emplace(7, std::make_unique< std::uint64_t >(42));
    auto unused = std::make_unique< std::uint64_t >(99);
    EXPECT_FALSE(map.try_emplace(7, std::move(unused)).second);
    EXPECT_NE(unused, nullptr);
    for (std::uint64_t key = 8; key < 8192; ++key) { map.try_emplace(key, nullptr); }
    EXPECT_EQ(map.find(7), original.first);
    EXPECT_EQ(*map.at(7), 42);
    EXPECT_FALSE(map.contains(1));
    EXPECT_THROW(map.at(1), std::out_of_range);
}

TEST(insert_only_concurrent_hash_trie, throwing_factory_leaves_key_absent)
{
    rpnx::insert_only_concurrent_hash_trie< std::uint64_t, std::uint64_t > map;
    EXPECT_THROW(map.get_or_create(3, []() -> std::uint64_t { throw std::runtime_error("failure"); }), std::runtime_error);
    EXPECT_FALSE(map.contains(3));
    EXPECT_EQ(map.size_exclusive(), 0);
    EXPECT_EQ(map.get_or_create(3, [] { return 9; }), 9);
}

TEST(insert_only_concurrent_hash_trie, frozen_compressed_node_survives_prefix_split)
{
    rpnx::insert_only_concurrent_hash_trie< std::uint64_t, std::uint64_t > map;
    for (std::uint64_t key = 0; key < 256; ++key) { map.try_emplace(key << 16, key); }
    std::atomic< bool > valid{true};
    std::vector< std::thread > readers;
    for (std::size_t index = 0; index < 4; ++index)
    {
        readers.emplace_back([&]
        {
            for (std::uint64_t iteration = 0; iteration < 16384; ++iteration)
            {
                std::uint64_t key = iteration % 256;
                if (map.at(key << 16) != key) { valid.store(false); }
            }
        });
    }
    for (std::uint64_t key = 1; key < 4096; ++key) { map.try_emplace(key, key); }
    for (std::thread& reader : readers) { reader.join(); }
    EXPECT_TRUE(valid.load());
    EXPECT_EQ(map.size_exclusive(), 4351);
}
