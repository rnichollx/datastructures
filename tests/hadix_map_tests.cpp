// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0
#include <algorithm>
#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <memory_resource>
#include <numeric>
#include <optional>
#include <random>
#include <rpnx/hadix_map.hpp>
#include <string>
#include <unordered_map>
#include <vector>

static_assert(std::forward_iterator< rpnx::hadix_map< std::uint64_t, std::uint64_t >::iterator >);
static_assert(std::forward_iterator< rpnx::hadix_map< std::uint64_t, std::uint64_t >::const_iterator >);
static_assert(std::forward_iterator< rpnx::hadix_map< std::uint64_t, std::uint64_t >::local_iterator >);

TEST(hadix_map, common_api)
{
    rpnx::hadix_map< std::string, std::int32_t > values{{"one", 1}, {"two", 2}};
    values["three"] = 3;
    EXPECT_EQ(values.size(), 3U);
    EXPECT_EQ(values.at("two"), 2);
    EXPECT_FALSE(values.try_emplace("two", 200).second);
    EXPECT_EQ(values.at("two"), 2);
    EXPECT_FALSE(values.insert_or_assign("two", 20).second);
    EXPECT_EQ(values.at("two"), 20);
    EXPECT_EQ(values.erase("one"), 1U);
    EXPECT_EQ(values.erase("one"), 0U);
    EXPECT_EQ(values.size(), 2U);
    EXPECT_THROW(values.at("absent"), std::out_of_range);
}

namespace hadix_tests
{
    /** @brief Detects a public element reservation operation. */
    template < typename Map >
    concept has_reserve = requires(Map& values) { values.reserve(1); };
    /** @brief Detects a public bucket reservation operation. */
    template < typename Map >
    concept has_rehash = requires(Map& values) { values.rehash(1); };
    /** @brief Detects a writable load-factor policy. */
    template < typename Map >
    concept has_load_setter = requires(Map& values) { values.max_load_factor(typename Map::size_type{1}); };
    static_assert(!has_reserve< rpnx::hadix_map< std::size_t, std::size_t > >);
    static_assert(!has_rehash< rpnx::hadix_map< std::size_t, std::size_t > >);
    static_assert(!has_load_setter< rpnx::hadix_map< std::size_t, std::size_t > >);
    static_assert(!std::is_constructible_v< rpnx::hadix_map< std::size_t, std::size_t >, std::size_t >);
    /** @brief Hash policy that places all keys in one bucket. */
    struct constant_hash
    {
        /** @brief Returns the same hash for every key. */
        std::size_t operator()(std::uint64_t) const noexcept
        {
            return 0;
        }
    };
    /** @brief Hash policy exposing a small set of collision groups. */
    struct grouped_hash
    {
        /** @brief Distributes keys among 64 exact hash values. */
        std::size_t operator()(std::uint64_t key) const noexcept
        {
            return key % 64;
        }
    };
    /** @brief Concentrates entries in the pair joined when 64 buckets become 63. */
    struct join_hash
    {
        /** @brief Selects hash 31 or 63 by key parity. */
        std::size_t operator()(std::uint64_t key) const noexcept
        {
            return 31 + (key % 2) * 32;
        }
    };
    /** @brief Hash policy using the key bits directly. */
    struct identity_hash
    {
        /** @brief Preserves all key bits in the indexing hash. */
        std::size_t operator()(std::uint64_t key) const noexcept
        {
            return key;
        }
    };
    /** @brief Tracks allocation ownership, construction and structural invariants. */
    struct allocation_state
    {
        /** @brief Describes a live allocation and the allocator that owns it. */
        struct allocation
        {
            std::size_t count;
            std::size_t bytes;
            std::int32_t owner;
        };
        std::unordered_map< void*, allocation > live;
        std::unordered_map< void*, std::function< void() > > checks;
        std::int32_t allocation_budget = -1;
        std::int32_t construction_budget = -1;
        std::size_t node_allocations = 0;
        std::size_t bucket_allocations = 0;

        /** @brief Verifies every constructed bucket and AVL node. */
        void verify() const
        {
            for (std::pair< void* const, std::function< void() > > const& entry : checks)
            {
                entry.second();
            }
        }
    };
    /** @brief Stateful allocator supporting shared failure injection across rebinds. */
    template < typename T, bool Propagate = false, bool MovePropagate = Propagate >
    struct audit_allocator
    {
        using value_type = T;
        using propagate_on_container_copy_assignment = std::bool_constant< Propagate >;
        using propagate_on_container_move_assignment = std::bool_constant< MovePropagate >;
        using propagate_on_container_swap = std::bool_constant< Propagate >;
        /** @brief Rebinds the allocator while retaining its propagation policy. */
        template < typename U >
        struct rebind
        {
            using other = audit_allocator< U, Propagate, MovePropagate >;
        };
        std::shared_ptr< allocation_state > state;
        std::int32_t owner = 1;

        /** @brief Creates an allocator with private audit state. */
        audit_allocator() : state(std::make_shared< allocation_state >())
        {
        }
        /** @brief Creates an allocator sharing an audit and ownership identity. */
        audit_allocator(std::shared_ptr< allocation_state > input, std::int32_t identity = 1) : state(std::move(input)), owner(identity)
        {
        }
        /** @brief Rebinds to a different allocation type. */
        template < typename U >
        audit_allocator(audit_allocator< U, Propagate, MovePropagate > const& other) noexcept : state(other.state), owner(other.owner)
        {
        }
        /** @brief Allocates aligned storage and records its owner. */
        T* allocate(std::size_t count)
        {
            if (state->allocation_budget == 0)
            {
                throw std::bad_alloc();
            }
            if (state->allocation_budget > 0)
            {
                --state->allocation_budget;
            }
            T* result = std::allocator< T >().allocate(count);
            state->live.emplace(result, allocation_state::allocation{count, count * sizeof(T), owner});
            if constexpr (requires(T item) {
                              item.hashes;
                              item.values;
                              item.root;
                          })
            {
                static_assert(sizeof(T) == 32 * sizeof(std::size_t));
                static_assert(alignof(T) == 8 * sizeof(std::size_t));
                static_assert(offsetof(T, hashes) == sizeof(std::size_t));
                static_assert(offsetof(T, values) == 16 * sizeof(std::size_t));
                static_assert(offsetof(T, root) == 31 * sizeof(std::size_t));
                EXPECT_EQ(reinterpret_cast< std::uintptr_t >(result) % alignof(T), 0U);
                ++state->bucket_allocations;
            }
            if constexpr (requires(T item) {
                              item.left;
                              item.right;
                              item.parent;
                              item.height;
                          })
            {
                static_assert(alignof(T) == 4 * sizeof(void*));
                static_assert(offsetof(T, hash) == 0);
                static_assert(offsetof(T, left) == sizeof(std::size_t));
                static_assert(offsetof(T, right) == sizeof(std::size_t) + sizeof(void*));
                static_assert(offsetof(T, value) == sizeof(std::size_t) + 2 * sizeof(void*));
                EXPECT_EQ(reinterpret_cast< std::uintptr_t >(result) % alignof(T), 0U);
                ++state->node_allocations;
            }
            return result;
        }
        /** @brief Verifies allocation identity and returns storage to its allocator. */
        void deallocate(T* address, std::size_t count) noexcept
        {
            auto found = state->live.find(address);
            EXPECT_NE(found, state->live.end());
            if (found != state->live.end())
            {
                EXPECT_EQ(found->second.count, count);
                EXPECT_EQ(found->second.owner, owner);
                state->live.erase(found);
            }
            std::allocator< T >().deallocate(address, count);
        }
        /** @brief Constructs an object and registers applicable structural checks. */
        template < typename U, typename... Args >
        void construct(U* address, Args&&... args)
        {
            if (state->construction_budget == 0)
            {
                throw std::runtime_error("injected construction failure");
            }
            if (state->construction_budget > 0)
            {
                --state->construction_budget;
            }
            std::construct_at(address, std::forward< Args >(args)...);
            if constexpr (requires(U item) {
                              item.left;
                              item.right;
                              item.parent;
                              item.height;
                          })
            {
                state->checks.emplace(address,
                                      [address]()
                                      {
                                          std::int32_t left_height = address->left ? address->left->height : 0;
                                          std::int32_t right_height = address->right ? address->right->height : 0;
                                          std::size_t left_count = address->left ? address->left->count : 0;
                                          std::size_t right_count = address->right ? address->right->count : 0;
                                          EXPECT_EQ(address->height, std::max(left_height, right_height) + 1);
                                          EXPECT_LE(std::abs(left_height - right_height), 1);
                                          EXPECT_EQ(address->count, left_count + right_count + 1);
                                          if (address->left)
                                          {
                                              EXPECT_EQ(address->left->parent, address);
                                          }
                                          if (address->right)
                                          {
                                              EXPECT_EQ(address->right->parent, address);
                                          }
                                          if (address->parent)
                                          {
                                              EXPECT_TRUE(address->parent->left == address || address->parent->right == address);
                                          }
                                      });
            }
            if constexpr (requires(U item) {
                              item.hashes;
                              item.values;
                              item.root;
                          })
            {
                state->checks.emplace(address,
                                      [address]()
                                      {
                                          std::size_t occupied = std::min(address->count, std::size_t{15});
                                          EXPECT_EQ(address->count, occupied + (address->root ? address->root->count : 0));
                                          if (address->root)
                                          {
                                              EXPECT_EQ(address->root->parent, nullptr);
                                          }
                                          for (std::size_t i = 0; i < occupied; ++i)
                                          {
                                              EXPECT_NE(address->values[i], nullptr);
                                              if (i != 0)
                                              {
                                                  EXPECT_LE(address->hashes[i - 1], address->hashes[i]);
                                              }
                                          }
                                          if (address->root && occupied != 0)
                                          {
                                              auto node = address->root;
                                              while (node->left)
                                              {
                                                  node = node->left;
                                              }
                                              EXPECT_LE(address->hashes[occupied - 1], node->hash);
                                          }
                                      });
            }
        }
        /** @brief Unregisters an object and destroys it. */
        template < typename U >
        void destroy(U* address) noexcept
        {
            state->checks.erase(address);
            std::destroy_at(address);
        }
        /** @brief Compares actual deallocation ownership. */
        template < typename U >
        bool operator==(audit_allocator< U, Propagate, MovePropagate > const& other) const noexcept
        {
            return state == other.state && owner == other.owner;
        }
        /** @brief Supplies a distinguishable copy-construction allocator identity. */
        audit_allocator select_on_container_copy_construction() const
        {
            return audit_allocator(state, owner + 100);
        }
    };
    /** @brief Nonmovable payload proving that structural changes only move pointers. */
    struct immovable
    {
        std::uint64_t value;
        /** @brief Constructs a value directly at its permanent address. */
        explicit immovable(std::uint64_t input) : value(input)
        {
        }
        immovable(immovable const&) = delete;
        immovable(immovable&&) = delete;
    };
    /** @brief Counts and optionally rejects key ordering operations. */
    struct counted_compare_three_way
    {
        std::shared_ptr< std::size_t > calls;
        std::shared_ptr< bool > reject;
        std::shared_ptr< std::optional< std::uint64_t > > allowed_key = nullptr;
        /** @brief Compares keys while recording the comparison. */
        std::strong_ordering operator()(std::uint64_t left, std::uint64_t right) const
        {
            ++*calls;
            if (*reject || (allowed_key && allowed_key->has_value() && left != **allowed_key))
            {
                throw std::runtime_error("unexpected key ordering");
            }
            return left <=> right;
        }
    };
    /** @brief Counts and optionally rejects hash evaluations. */
    struct counted_hash
    {
        std::shared_ptr< std::size_t > calls;
        std::shared_ptr< bool > reject;
        std::shared_ptr< std::optional< std::uint64_t > > allowed_key = nullptr;
        /** @brief Produces eight collision hashes sharing their lowest three bits. */
        std::size_t operator()(std::uint64_t key) const
        {
            ++*calls;
            if (*reject || (allowed_key && allowed_key->has_value() && key != **allowed_key))
            {
                throw std::runtime_error("unexpected hashing");
            }
            return (key % 8) << 3;
        }
    };
    /** @brief Orders keys in descending order with a single three-way result. */
    struct reverse_compare_three_way
    {
        /** @brief Compares keys with reversed ordering. */
        std::strong_ordering operator()(std::uint64_t left, std::uint64_t right) const noexcept
        {
            return right <=> left;
        }
    };
    /** @brief Key with an identity and a representation, without comparison operators. */
    struct represented_key
    {
        std::uint64_t identity;
        std::uint64_t representation;
    };
    /** @brief Groups identities into four hashes independently of their representation. */
    struct represented_hash
    {
        /** @brief Hashes equivalent representations equally while producing collisions. */
        std::size_t operator()(represented_key const& key) const noexcept
        {
            return key.identity % 4;
        }
    };
    /** @brief Defines key equivalence and configurable ordering using identity alone. */
    struct represented_compare_three_way
    {
        bool descending = false;
        /** @brief Compares identities while treating different representations as equivalent. */
        std::weak_ordering operator()(represented_key const& left, represented_key const& right) const noexcept
        {
            return descending ? right.identity <=> left.identity : left.identity <=> right.identity;
        }
    };
    /** @brief Verifies contents, unique traversal, bucket membership and local iteration. */
    template < typename Map >
    void verify_contents(Map const& values, std::map< std::uint64_t, std::uint64_t > const& expected)
    {
        EXPECT_EQ(values.size(), expected.size());
        std::map< std::uint64_t, std::uint64_t > actual;
        for (typename Map::value_type const& entry : values)
        {
            EXPECT_TRUE(actual.emplace(entry.first, entry.second).second);
        }
        EXPECT_EQ(actual, expected);
        std::size_t total = 0;
        for (std::size_t index = 0; index < values.bucket_count(); ++index)
        {
            std::size_t count = 0;
            for (auto it = values.begin(index); it != values.end(index); ++it)
            {
                EXPECT_EQ(values.bucket(it->first), index);
                ++count;
            }
            EXPECT_EQ(count, values.bucket_size(index));
            total += count;
        }
        EXPECT_EQ(total, expected.size());
        for (std::pair< std::uint64_t const, std::uint64_t > const& entry : expected)
        {
            auto found = values.find(entry.first);
            ASSERT_NE(found, values.end());
            EXPECT_EQ(found->second, entry.second);
        }
    }
} // namespace hadix_tests

TEST(hadix_map, collision_trees_and_stable_immovable_values)
{
    auto state = std::make_shared< hadix_tests::allocation_state >();
    using allocator = hadix_tests::audit_allocator< std::pair< std::uint64_t const, hadix_tests::immovable > >;
    using map = rpnx::hadix_map< std::uint64_t, hadix_tests::immovable, hadix_tests::constant_hash, std::compare_three_way, allocator >;
    {
        map values({}, {}, allocator(state));
        std::vector< hadix_tests::immovable* > addresses(1024);
        for (std::uint64_t i = 0; i < 1024; ++i)
        {
            std::uint64_t key = (i * 683) % 1024;
            auto result = values.try_emplace(key, key * 3);
            ASSERT_TRUE(result.second);
            addresses[key] = &result.first->second;
            if (i < 35 || i % 47 == 0)
            {
                state->verify();
            }
        }
        state->verify();
        EXPECT_GT(state->node_allocations, 0U);
        for (std::uint64_t i = 0; i < 1024; ++i)
        {
            EXPECT_EQ(&values.at(i), addresses[i]);
            EXPECT_EQ(values.at(i).value, i * 3);
        }
        for (std::uint64_t i = 0; i < 1024; ++i)
        {
            std::uint64_t key = (i * 683) % 1024;
            EXPECT_EQ(values.erase(key), 1U);
            if (i < 35 || i % 47 == 0)
            {
                state->verify();
            }
        }
        EXPECT_TRUE(values.empty());
        EXPECT_EQ(values.begin(), values.end());
        state->verify();
    }
    EXPECT_TRUE(state->live.empty());
    EXPECT_TRUE(state->checks.empty());
}

TEST(hadix_map, automatic_splits_and_joins_do_not_hash_or_compare_retained_keys)
{
    auto state = std::make_shared< hadix_tests::allocation_state >();
    auto hash_calls = std::make_shared< std::size_t >(0);
    auto compare_calls = std::make_shared< std::size_t >(0);
    auto reject = std::make_shared< bool >(false);
    auto allowed_key = std::make_shared< std::optional< std::uint64_t > >();
    using allocator = hadix_tests::audit_allocator< std::pair< std::uint64_t const, std::uint64_t > >;
    using map = rpnx::hadix_map< std::uint64_t, std::uint64_t, hadix_tests::counted_hash, hadix_tests::counted_compare_three_way, allocator >;
    {
        map values({hash_calls, reject, allowed_key}, {compare_calls, reject, allowed_key}, allocator(state));
        std::map< std::uint64_t, std::uint64_t > expected;
        std::vector< std::uint64_t* > addresses(1024);
        for (std::uint64_t i = 0; i < 1024; ++i)
        {
            std::uint64_t key = (i * 683) % 1024;
            *allowed_key = key;
            *hash_calls = 0;
            std::size_t before = values.bucket_count();
            auto result = values.emplace(key, key * 5);
            ASSERT_TRUE(result.second);
            EXPECT_EQ(*hash_calls, 1U);
            EXPECT_GE(values.bucket_count(), before);
            EXPECT_LE(values.bucket_count() - before, 1U);
            addresses[key] = &result.first->second;
            expected.emplace(key, key * 5);
            allowed_key->reset();
            if (values.bucket_count() != before)
            {
                state->verify();
                hadix_tests::verify_contents(values, expected);
            }
        }
        for (std::uint64_t i = 0; i < 1024; ++i)
        {
            std::uint64_t key = (i * 683) % 1024;
            EXPECT_EQ(&values.at(key), addresses[key]);
            *allowed_key = key;
            *hash_calls = 0;
            std::size_t before = values.bucket_count();
            std::size_t allocated = state->node_allocations;
            EXPECT_EQ(values.erase(key), 1U);
            EXPECT_EQ(*hash_calls, 1U);
            EXPECT_LE(values.bucket_count(), before);
            EXPECT_LE(before - values.bucket_count(), 1U);
            EXPECT_LE(state->node_allocations - allocated, 15U);
            expected.erase(key);
            allowed_key->reset();
            if (values.bucket_count() != before)
            {
                state->verify();
                hadix_tests::verify_contents(values, expected);
            }
        }
        EXPECT_TRUE(values.empty());
        EXPECT_LE(values.bucket_count(), 3U);
    }
    EXPECT_TRUE(state->live.empty());
}

TEST(hadix_map, automatic_bucket_boundaries_and_empty_hash_partitions)
{
    rpnx::hadix_map< std::uint64_t, std::uint64_t, hadix_tests::identity_hash > values;
    std::map< std::uint64_t, std::uint64_t > expected;
    for (std::uint64_t i = 0; i < 2056; ++i)
    {
        std::size_t before = values.bucket_count();
        std::uint64_t key = i << 8;
        values.try_emplace(key, i);
        expected.emplace(key, i);
        EXPECT_EQ(values.bucket_count(), (values.size() + 7) / 8);
        if (values.bucket_count() != before)
        {
            EXPECT_EQ(values.bucket_count(), before + 1);
            hadix_tests::verify_contents(values, expected);
        }
    }
    EXPECT_EQ(values.bucket_count(), 257U);
    for (std::uint64_t i = 2056; i != 0; --i)
    {
        std::size_t before = values.bucket_count();
        std::uint64_t key = (i - 1) << 8;
        values.erase(key);
        expected.erase(key);
        EXPECT_LE(before - values.bucket_count(), 1U);
        if (values.bucket_count() != before)
        {
            hadix_tests::verify_contents(values, expected);
        }
    }
    EXPECT_TRUE(values.empty());
    EXPECT_LE(values.bucket_count(), 3U);
}

TEST(hadix_map, randomized_differential_mutations_and_reference_stability)
{
    auto state = std::make_shared< hadix_tests::allocation_state >();
    using allocator = hadix_tests::audit_allocator< std::pair< std::uint64_t const, std::uint64_t > >;
    using map = rpnx::hadix_map< std::uint64_t, std::uint64_t, hadix_tests::grouped_hash, std::compare_three_way, allocator >;
    {
        map values({}, {}, allocator(state));
        std::map< std::uint64_t, std::uint64_t > expected;
        std::unordered_map< std::uint64_t, std::uint64_t* > addresses;
        std::mt19937_64 random(789123);
        for (std::size_t step = 0; step < 15000; ++step)
        {
            std::uint64_t key = random() % 3000;
            std::uint64_t value = random();
            switch (random() % 5)
            {
            case 0:
            case 1: {
                auto result = values.insert_or_assign(key, value);
                bool inserted = expected.insert_or_assign(key, value).second;
                EXPECT_EQ(result.second, inserted);
                if (inserted)
                {
                    addresses[key] = &result.first->second;
                }
                else
                {
                    EXPECT_EQ(&result.first->second, addresses.at(key));
                }
                break;
            }
            case 2:
                EXPECT_EQ(values.erase(key), expected.erase(key));
                addresses.erase(key);
                break;
            case 3: {
                auto result = values.try_emplace(key, value);
                bool inserted = expected.emplace(key, value).second;
                EXPECT_EQ(result.second, inserted);
                if (inserted)
                {
                    addresses[key] = &result.first->second;
                }
                break;
            }
            default:
                EXPECT_EQ(values.contains(key), expected.contains(key));
                break;
            }
            if (step % 251 == 0)
            {
                state->verify();
                hadix_tests::verify_contents(values, expected);
                for (std::pair< std::uint64_t const, std::uint64_t* > const& entry : addresses)
                {
                    EXPECT_EQ(&values.at(entry.first), entry.second);
                }
            }
        }
        state->verify();
        hadix_tests::verify_contents(values, expected);
    }
    EXPECT_TRUE(state->live.empty());
}

TEST(hadix_map, iterator_erasure_survives_prefix_refill_and_directory_shrinking)
{
    rpnx::hadix_map< std::uint64_t, std::uint64_t, hadix_tests::grouped_hash > values;
    for (std::uint64_t i = 0; i < 2048; ++i)
    {
        values.try_emplace(i, i);
    }
    std::vector< std::uint64_t > traversal;
    for (std::pair< std::uint64_t const, std::uint64_t > const& entry : values)
    {
        traversal.push_back(entry.first);
    }
    std::size_t erased = 0;
    for (auto it = values.begin(); it != values.end();)
    {
        ASSERT_LT(erased, traversal.size());
        EXPECT_EQ(it->first, traversal[erased]);
        it = values.erase(it);
        ++erased;
    }
    EXPECT_EQ(erased, traversal.size());
    EXPECT_TRUE(values.empty());
    for (std::uint64_t i = 0; i < 2048; ++i)
    {
        values.try_emplace(i, i);
    }
    auto first = values.cbegin();
    std::advance(first, 9);
    auto last = first;
    std::advance(last, 1700);
    std::uint64_t retained = last->first;
    auto result = values.erase(first, last);
    ASSERT_NE(result, values.end());
    EXPECT_EQ(result->first, retained);
    EXPECT_EQ(values.size(), 348U);
    EXPECT_EQ(values.erase(values.cbegin(), values.cend()), values.end());
    EXPECT_TRUE(values.empty());
}

TEST(hadix_map, iterator_value_pointers_follow_inline_tree_and_bucket_transitions)
{
    using map = rpnx::hadix_map< std::uint64_t, std::uint64_t, hadix_tests::grouped_hash >;
    map values;
    std::vector< map::value_type* > addresses(2048);
    for (std::uint64_t key = 0; key < addresses.size(); ++key)
    {
        addresses[key] = &*values.try_emplace(key, key * 3).first;
    }
    std::vector< bool > visited(addresses.size());
    for (map::iterator current = values.begin(); current != values.end();)
    {
        map::const_iterator converted = current;
        map::iterator previous = current++;
        std::uint64_t key = previous->first;
        ASSERT_LT(key, addresses.size());
        EXPECT_FALSE(visited[key]);
        visited[key] = true;
        EXPECT_EQ(&*previous, addresses[key]);
        EXPECT_EQ(converted.operator->(), addresses[key]);
        previous->second += 1;
        EXPECT_EQ(converted->second, key * 3 + 1);
        EXPECT_EQ(++converted, current);
        if (current != values.end())
        {
            EXPECT_EQ(converted.operator->(), &*current);
        }
    }
    EXPECT_TRUE(std::ranges::all_of(visited,
                                    [](bool present)
                                    {
                                        return present;
                                    }));
    std::fill(visited.begin(), visited.end(), false);
    for (std::size_t bucket = 0; bucket < values.bucket_count(); ++bucket)
    {
        for (map::local_iterator current = values.begin(bucket); current != values.end(bucket);)
        {
            map::const_local_iterator converted = current;
            map::local_iterator previous = current++;
            std::uint64_t key = previous->first;
            ASSERT_LT(key, addresses.size());
            EXPECT_FALSE(visited[key]);
            visited[key] = true;
            EXPECT_EQ(&*previous, addresses[key]);
            EXPECT_EQ(converted.operator->(), addresses[key]);
            EXPECT_EQ(++converted, current);
            if (current != values.end(bucket))
            {
                EXPECT_EQ(converted.operator->(), &*current);
            }
        }
    }
    EXPECT_TRUE(std::ranges::all_of(visited,
                                    [](bool present)
                                    {
                                        return present;
                                    }));
}

TEST(hadix_map, duplicate_try_emplace_does_not_consume_move_only_arguments)
{
    rpnx::hadix_map< std::string, std::unique_ptr< std::uint64_t > > values;
    values.try_emplace("key", std::make_unique< std::uint64_t >(42));
    auto candidate = std::make_unique< std::uint64_t >(99);
    std::string key = "key";
    EXPECT_FALSE(values.try_emplace(std::move(key), std::move(candidate)).second);
    ASSERT_NE(candidate, nullptr);
    EXPECT_EQ(*candidate, 99U);
    EXPECT_EQ(key, "key");
    auto result = values.emplace_hint(values.cend(), "another", std::make_unique< std::uint64_t >(17));
    EXPECT_EQ(*result->second, 17U);
    values.insert_or_assign(values.cend(), "key", std::move(candidate));
    EXPECT_EQ(*values.at("key"), 99U);
    auto range = values.equal_range("key");
    EXPECT_EQ(std::distance(range.first, range.second), 1);
    auto missing = std::as_const(values).equal_range("absent");
    EXPECT_EQ(missing.first, values.end());
    EXPECT_EQ(missing.second, values.end());
}

TEST(hadix_map, collisions_use_the_supplied_key_order)
{
    using map = rpnx::hadix_map< std::uint64_t, std::uint64_t, hadix_tests::constant_hash, hadix_tests::reverse_compare_three_way >;
    map values;
    for (std::uint64_t i = 0; i < 200; ++i)
    {
        values.try_emplace(i, i);
    }
    std::uint64_t expected = 200;
    for (std::pair< std::uint64_t const, std::uint64_t > const& entry : values)
    {
        EXPECT_EQ(entry.first, --expected);
    }
    EXPECT_EQ(expected, 0U);
    for (std::uint64_t i = 0; i < 200; ++i)
    {
        EXPECT_EQ(values.at(i), i);
    }
}

TEST(hadix_map, three_way_comparator_defines_key_equivalence)
{
    using map = rpnx::hadix_map< hadix_tests::represented_key, std::uint64_t, hadix_tests::represented_hash, hadix_tests::represented_compare_three_way >;
    static_assert(std::same_as< map::key_compare_three_way, hadix_tests::represented_compare_three_way >);
    for (bool descending : {false, true})
    {
        map values({}, {descending});
        std::vector< std::uint64_t* > addresses(128);
        for (std::uint64_t i = 0; i < 128; ++i)
        {
            std::uint64_t identity = (i * 37) % 128;
            auto inserted = values.try_emplace(hadix_tests::represented_key{identity, 10}, identity);
            ASSERT_TRUE(inserted.second);
            addresses[identity] = &inserted.first->second;
        }
        for (std::uint64_t i = 0; i < 128; ++i)
        {
            auto found = values.find({i, 20});
            ASSERT_NE(found, values.end());
            EXPECT_EQ(found->first.representation, 10U);
            EXPECT_EQ(&found->second, addresses[i]);
            EXPECT_FALSE(values.try_emplace(hadix_tests::represented_key{i, 30}, 999).second);
            EXPECT_FALSE(values.insert_or_assign(hadix_tests::represented_key{i, 40}, i).second);
            EXPECT_EQ(values.count({i, 50}), 1U);
            EXPECT_EQ(values.at({i, 60}), i);
            EXPECT_EQ(values.size(), 128U);
        }
        map copied(values);
        map moved(std::move(copied));
        map assigned({}, {!descending});
        assigned = moved;
        map move_assigned({}, {!descending});
        move_assigned = std::move(assigned);
        map swapped({}, {!descending});
        swapped.swap(move_assigned);
        EXPECT_EQ(moved.key_comp_three_way().descending, descending);
        EXPECT_EQ(swapped.key_comp_three_way().descending, descending);
        EXPECT_EQ(move_assigned.key_comp_three_way().descending, !descending);
        EXPECT_EQ(values, swapped);
        EXPECT_EQ(values, moved);

        map alternate({}, {descending});
        for (std::uint64_t i = 0; i < 128; ++i)
        {
            alternate.try_emplace(hadix_tests::represented_key{i, 70}, i);
            EXPECT_EQ(swapped.at({i, 80}), i);
        }
        EXPECT_EQ(values, alternate);
        alternate.at({100, 90}) = 999;
        EXPECT_NE(values, alternate);
        for (std::uint64_t i = 0; i < 128; ++i)
        {
            EXPECT_EQ(values.erase({i, 100}), 1U);
            EXPECT_EQ(values.erase({i, 110}), 0U);
        }
        EXPECT_TRUE(values.empty());
    }
}

TEST(hadix_map, key_comparator_runs_only_when_hashes_match)
{
    auto calls = std::make_shared< std::size_t >(0);
    auto reject = std::make_shared< bool >(true);
    using map = rpnx::hadix_map< std::uint64_t, std::uint64_t, hadix_tests::identity_hash, hadix_tests::counted_compare_three_way >;
    map values({}, {calls, reject});
    for (std::uint64_t i = 0; i < 128; ++i)
    {
        EXPECT_TRUE(values.try_emplace(i, i).second);
    }
    EXPECT_FALSE(values.contains(129));
    EXPECT_EQ(*calls, 0U);
    *reject = false;
    for (std::uint64_t i = 0; i < 128; ++i)
    {
        *calls = 0;
        EXPECT_EQ(values.at(i), i);
        EXPECT_EQ(*calls, 1U);
        *calls = 0;
        EXPECT_FALSE(values.try_emplace(i, 999).second);
        EXPECT_EQ(*calls, 1U);
    }
}

TEST(hadix_map, hash_scan_excludes_inactive_slots_after_prefix_refill_and_erasure)
{
    std::size_t shift = std::numeric_limits< std::size_t >::digits - 6;
    std::size_t suffix_mask = (std::size_t{1} << shift) - 1;
    for (std::size_t suffix : {std::size_t{0}, suffix_mask})
    {
        rpnx::hadix_map< std::uint64_t, std::uint64_t, hadix_tests::identity_hash > values;
        std::map< std::uint64_t, std::uint64_t > expected;
        for (std::size_t i = 0; i < 64; ++i)
        {
            std::uint64_t key = (i << shift) | suffix;
            values.try_emplace(key, i);
            expected.emplace(key, i);
        }
        for (std::size_t step = 0; step < 64; ++step)
        {
            std::uint64_t removed = (((step * 17) % 64) << shift) | suffix;
            EXPECT_EQ(values.erase(removed), 1U);
            expected.erase(removed);
            for (std::size_t i = 0; i < 64; ++i)
            {
                std::uint64_t key = (i << shift) | suffix;
                EXPECT_EQ(values.contains(key), expected.contains(key));
                EXPECT_EQ(std::as_const(values).contains(key), expected.contains(key));
            }
            hadix_tests::verify_contents(values, expected);
        }
    }
}

TEST(hadix_map, partial_comparison_category_supports_ordered_floating_keys)
{
    rpnx::hadix_map< double, std::uint64_t > values;
    for (std::uint64_t i = 0; i < 128; ++i)
    {
        values.try_emplace(static_cast< double >(i), i);
    }
    EXPECT_FALSE(values.try_emplace(-0.0, 999).second);
    EXPECT_EQ(values.at(-0.0), 0U);
    for (std::uint64_t i = 0; i < 128; ++i)
    {
        EXPECT_EQ(values.at(static_cast< double >(i)), i);
    }
}

TEST(hadix_map, failed_allocations_and_constructions_preserve_owned_values)
{
    using allocator = hadix_tests::audit_allocator< std::pair< std::uint64_t const, std::uint64_t > >;
    using map = rpnx::hadix_map< std::uint64_t, std::uint64_t, hadix_tests::constant_hash, std::compare_three_way, allocator >;
    for (bool fail_construction : {false, true})
    {
        for (std::size_t initial : {0U, 8U, 15U, 16U, 24U, 31U, 64U})
        {
            for (std::int32_t budget = 0; budget < 6; ++budget)
            {
                auto state = std::make_shared< hadix_tests::allocation_state >();
                {
                    map values({}, {}, allocator(state));
                    for (std::uint64_t i = 0; i < initial; ++i)
                    {
                        values.try_emplace(i, i);
                    }
                    if (fail_construction)
                    {
                        state->construction_budget = budget;
                    }
                    else
                    {
                        state->allocation_budget = budget;
                    }
                    bool inserted = false;
                    try
                    {
                        inserted = values.try_emplace(999, 999).second;
                    }
                    catch (std::bad_alloc const&)
                    {
                    }
                    catch (std::runtime_error const&)
                    {
                    }
                    state->allocation_budget = -1;
                    state->construction_budget = -1;
                    EXPECT_EQ(values.size(), initial + (inserted ? 1 : 0));
                    for (std::uint64_t i = 0; i < initial; ++i)
                    {
                        EXPECT_EQ(values.at(i), i);
                    }
                    state->verify();
                    EXPECT_EQ(values.contains(999), inserted);
                    values.try_emplace(1000, 1000);
                    state->verify();
                }
                EXPECT_TRUE(state->live.empty());
                EXPECT_TRUE(state->checks.empty());
            }
        }
    }
}

TEST(hadix_map, failed_joins_preserve_retained_entries_and_subtree_counts)
{
    using allocator = hadix_tests::audit_allocator< std::pair< std::uint64_t const, std::uint64_t > >;
    using map = rpnx::hadix_map< std::uint64_t, std::uint64_t, hadix_tests::join_hash, std::compare_three_way, allocator >;
    for (bool fail_construction : {false, true})
    {
        for (std::int32_t budget = 0; budget <= 15; ++budget)
        {
            auto state = std::make_shared< hadix_tests::allocation_state >();
            {
                map values({}, {}, allocator(state));
                std::map< std::uint64_t, std::uint64_t > expected;
                for (std::uint64_t i = 0; i < 512; ++i)
                {
                    values.try_emplace(i, i);
                    expected.emplace(i, i);
                }
                for (std::uint64_t i = 0; i < 23; ++i)
                {
                    values.erase(i);
                    expected.erase(i);
                }
                ASSERT_EQ(values.bucket_count(), 64U);
                if (fail_construction)
                {
                    state->construction_budget = budget;
                }
                else
                {
                    state->allocation_budget = budget;
                }
                bool failed = false;
                try
                {
                    values.erase(23);
                }
                catch (std::bad_alloc const&)
                {
                    failed = true;
                }
                catch (std::runtime_error const&)
                {
                    failed = true;
                }
                EXPECT_EQ(failed, budget < 15);
                EXPECT_EQ(values.bucket_count(), failed ? 64U : 63U);
                expected.erase(23);
                state->allocation_budget = -1;
                state->construction_budget = -1;
                state->verify();
                hadix_tests::verify_contents(values, expected);
                values.erase(24);
                expected.erase(24);
                EXPECT_EQ(values.bucket_count(), 63U);
                state->verify();
                hadix_tests::verify_contents(values, expected);
            }
            EXPECT_TRUE(state->live.empty());
        }
    }
}

TEST(hadix_map, logarithmic_key_comparisons_under_total_hash_collision)
{
    auto calls = std::make_shared< std::size_t >(0);
    auto reject = std::make_shared< bool >(false);
    using map = rpnx::hadix_map< std::uint64_t, std::uint64_t, hadix_tests::constant_hash, hadix_tests::counted_compare_three_way >;
    map values({}, {calls, reject});
    for (std::uint64_t i = 0; i < 8192; ++i)
    {
        values.try_emplace(i, i);
    }
    for (std::uint64_t key : {0U, 15U, 127U, 4096U, 8191U, 9999U})
    {
        *calls = 0;
        EXPECT_EQ(values.contains(key), key < 8192);
        EXPECT_LE(*calls, 4U + 2U * std::bit_width(values.size()));
    }
    *calls = 0;
    values.try_emplace(9999, 9999);
    EXPECT_LT(*calls, 8U * std::bit_width(values.size()));
    *calls = 0;
    values.erase(4096);
    EXPECT_LT(*calls, 4U * std::bit_width(values.size()));
}

TEST(hadix_map, automatic_bucket_policy_copy_move_swap_and_clear)
{
    using map = rpnx::hadix_map< std::uint64_t, std::uint64_t >;
    map values{{1, 10}, {2, 20}, {3, 30}};
    EXPECT_EQ(values.max_load_factor(), 8U);
    for (std::uint64_t i = 4; i < 500; ++i)
    {
        values.try_emplace(i, i);
    }
    EXPECT_EQ(values.bucket_count(), 63U);
    map copy(values);
    EXPECT_EQ(copy, values);
    EXPECT_NE(&copy.at(1), &values.at(1));
    std::uint64_t* retained = &values.at(1);
    map moved(std::move(values));
    EXPECT_TRUE(values.empty());
    EXPECT_EQ(values.bucket_count(), 0U);
    EXPECT_EQ(&moved.at(1), retained);
    values.try_emplace(10000, 7);
    swap(values, moved);
    EXPECT_EQ(&values.at(1), retained);
    EXPECT_EQ(moved.at(10000), 7U);
    moved = values;
    EXPECT_EQ(moved, values);
    moved = {{90, 900}, {91, 910}};
    EXPECT_EQ(moved.size(), 2U);
    values = std::move(moved);
    EXPECT_TRUE(moved.empty());
    EXPECT_EQ(values.at(90), 900U);
    copy.clear();
    EXPECT_EQ(copy.bucket_count(), 0U);
    copy.try_emplace(90, 900);
    EXPECT_EQ(copy.bucket_count(), 1U);
    EXPECT_EQ(copy.erase(90), 1U);
    EXPECT_LE(copy.bucket_count(), 1U);
}

namespace hadix_tests
{
    /** @brief Exercises allocator propagation and matching deallocation ownership. */
    template < bool Propagate >
    void verify_allocator_ownership()
    {
        auto state = std::make_shared< allocation_state >();
        using allocator = audit_allocator< std::pair< std::uint64_t const, std::uint64_t >, Propagate >;
        using map = rpnx::hadix_map< std::uint64_t, std::uint64_t, constant_hash, std::compare_three_way, allocator >;
        {
            map source({}, {}, allocator(state, 1));
            for (std::uint64_t i = 0; i < 100; ++i)
            {
                source.try_emplace(i, i);
            }
            map copy(source);
            EXPECT_EQ(copy.get_allocator().owner, 101);
            EXPECT_EQ(copy, source);
            map target({}, {}, allocator(state, 2));
            target.try_emplace(999, 999);
            target = source;
            EXPECT_EQ(target.get_allocator().owner, Propagate ? 1 : 2);
            EXPECT_EQ(target, source);
            map moved({}, {}, allocator(state, 3));
            std::uint64_t* retained = &source.at(0);
            moved = std::move(source);
            EXPECT_TRUE(source.empty());
            EXPECT_EQ(moved.get_allocator().owner, Propagate ? 1 : 3);
            if constexpr (Propagate)
            {
                EXPECT_EQ(&moved.at(0), retained);
            }
            EXPECT_EQ(moved.at(99), 99U);
            state->verify();
            if constexpr (Propagate)
            {
                swap(copy, moved);
            }
            else
            {
                EXPECT_THROW(copy.swap(moved), std::invalid_argument);
                map matching({}, {}, allocator(state, 3));
                swap(matching, moved);
                EXPECT_EQ(matching.size(), 100U);
            }
            state->verify();
        }
        EXPECT_TRUE(state->live.empty());
        EXPECT_TRUE(state->checks.empty());
    }
} // namespace hadix_tests
TEST(hadix_map, propagating_allocators)
{
    hadix_tests::verify_allocator_ownership< true >();
}
TEST(hadix_map, nonpropagating_allocators)
{
    hadix_tests::verify_allocator_ownership< false >();
}

TEST(hadix_map, allocator_moves_preserve_immovable_values_when_storage_can_transfer)
{
    using map = rpnx::hadix_map< std::uint64_t, hadix_tests::immovable, hadix_tests::constant_hash >;
    map values;
    for (std::uint64_t i = 0; i < 128; ++i)
    {
        values.try_emplace(i, i);
    }
    hadix_tests::immovable* retained = &values.at(37);
    map extended(std::move(values), values.get_allocator());
    EXPECT_TRUE(values.empty());
    EXPECT_EQ(&extended.at(37), retained);
    values = std::move(extended);
    EXPECT_TRUE(extended.empty());
    EXPECT_EQ(&values.at(37), retained);
}

TEST(hadix_map, copy_allocator_propagation_independent_of_move_propagation)
{
    auto state = std::make_shared< hadix_tests::allocation_state >();
    using allocator = hadix_tests::audit_allocator< std::pair< std::uint64_t const, std::uint64_t >, true, false >;
    using map = rpnx::hadix_map< std::uint64_t, std::uint64_t, hadix_tests::constant_hash, std::compare_three_way, allocator >;
    {
        map source({}, {}, allocator(state, 1));
        map destination({}, {}, allocator(state, 2));
        for (std::uint64_t i = 0; i < 100; ++i)
        {
            source.try_emplace(i, i);
            destination.try_emplace(i + 100, i);
        }
        destination = source;
        EXPECT_EQ(destination.get_allocator().owner, 1);
        EXPECT_EQ(destination, source);
        state->verify();
        for (std::pair< void* const, hadix_tests::allocation_state::allocation > const& allocation : state->live)
        {
            EXPECT_EQ(allocation.second.owner, 1);
        }
        for (std::uint64_t i = 100; i < 500; ++i)
        {
            destination.try_emplace(i, i);
        }
        state->verify();
    }
    EXPECT_TRUE(state->live.empty());
}

TEST(hadix_map, polymorphic_allocator_and_nonpropagating_assignment)
{
    std::pmr::unsynchronized_pool_resource first_resource;
    std::pmr::unsynchronized_pool_resource second_resource;
    using allocator = std::pmr::polymorphic_allocator< std::pair< std::uint64_t const, std::uint64_t > >;
    using map = rpnx::hadix_map< std::uint64_t, std::uint64_t, hadix_tests::constant_hash, std::compare_three_way, allocator >;
    map first({}, {}, allocator(&first_resource));
    map second({}, {}, allocator(&second_resource));
    for (std::uint64_t i = 0; i < 100; ++i)
    {
        first.try_emplace(i, i);
    }
    second = first;
    EXPECT_EQ(second, first);
    EXPECT_EQ(second.get_allocator().resource(), &second_resource);
    second = std::move(first);
    EXPECT_TRUE(first.empty());
    EXPECT_EQ(second.size(), 100U);
    EXPECT_EQ(second.get_allocator().resource(), &second_resource);
}

TEST(hadix_map, full_width_hash_order_survives_automatic_splits_and_joins)
{
    rpnx::hadix_map< std::uint64_t, std::uint64_t, hadix_tests::identity_hash > values;
    std::map< std::uint64_t, std::uint64_t > expected;
    auto reverse_hash = [](std::size_t hash)
    {
        std::size_t result = 0;
        for (std::size_t i = 0; i < std::numeric_limits< std::size_t >::digits; ++i)
        {
            result = (result << 1) | (hash & 1);
            hash >>= 1;
        }
        return result;
    };
    auto verify_order = [&]()
    {
        std::vector< std::pair< std::uint64_t, std::uint64_t > > ordered(expected.begin(), expected.end());
        std::sort(ordered.begin(), ordered.end(),
                  [&](std::pair< std::uint64_t, std::uint64_t > const& left, std::pair< std::uint64_t, std::uint64_t > const& right)
                  {
                      return std::pair{reverse_hash(static_cast< std::size_t >(left.first)), left.first} < std::pair{reverse_hash(static_cast< std::size_t >(right.first)), right.first};
                  });
        std::vector< std::pair< std::uint64_t, std::uint64_t > > actual(values.begin(), values.end());
        EXPECT_EQ(actual, ordered);
        hadix_tests::verify_contents(values, expected);
    };
    std::size_t highest_bit = std::size_t{1} << (std::numeric_limits< std::size_t >::digits - 1);
    for (std::uint64_t key : {std::uint64_t{0}, UINT64_MAX, static_cast< std::uint64_t >(highest_bit), static_cast< std::uint64_t >(highest_bit - 1)})
    {
        values.try_emplace(key, key);
        expected.emplace(key, key);
    }
    std::mt19937_64 random(32991);
    std::vector< std::uint64_t > added;
    for (std::size_t i = 0; i < 1032; ++i)
    {
        std::uint64_t key = random();
        if (expected.contains(key))
        {
            continue;
        }
        std::size_t before = values.bucket_count();
        added.push_back(key);
        values.try_emplace(key, i);
        expected.emplace(key, i);
        if (values.bucket_count() != before)
        {
            verify_order();
        }
    }
    for (std::uint64_t key : added)
    {
        std::size_t before = values.bucket_count();
        values.erase(key);
        expected.erase(key);
        if (values.bucket_count() != before)
        {
            verify_order();
        }
    }
    verify_order();
}

TEST(hadix_map, failed_copy_construction_releases_partial_values_and_metadata)
{
    auto state = std::make_shared< hadix_tests::allocation_state >();
    using allocator = hadix_tests::audit_allocator< std::pair< std::uint64_t const, std::uint64_t > >;
    using map = rpnx::hadix_map< std::uint64_t, std::uint64_t, hadix_tests::constant_hash, std::compare_three_way, allocator >;
    {
        map original({}, {}, allocator(state));
        for (std::uint64_t i = 0; i < 64; ++i)
        {
            original.try_emplace(i, i);
        }
        std::size_t allocations = state->live.size();
        for (std::int32_t budget = 0; budget < 130; ++budget)
        {
            state->construction_budget = budget;
            try
            {
                map copy(original);
                EXPECT_EQ(copy, original);
            }
            catch (std::runtime_error const&)
            {
            }
            state->construction_budget = -1;
            EXPECT_EQ(state->live.size(), allocations);
            EXPECT_EQ(original.size(), 64U);
            state->verify();
        }
    }
    EXPECT_TRUE(state->live.empty());
}

TEST(hadix_map, throwing_hash_and_ordering_leave_existing_entries_intact)
{
    auto state = std::make_shared< hadix_tests::allocation_state >();
    auto hash_calls = std::make_shared< std::size_t >(0);
    auto compare_calls = std::make_shared< std::size_t >(0);
    auto reject_hash = std::make_shared< bool >(false);
    auto reject_compare = std::make_shared< bool >(false);
    using allocator = hadix_tests::audit_allocator< std::pair< std::uint64_t const, std::uint64_t > >;
    using map = rpnx::hadix_map< std::uint64_t, std::uint64_t, hadix_tests::counted_hash, hadix_tests::counted_compare_three_way, allocator >;
    {
        map values({hash_calls, reject_hash}, {compare_calls, reject_compare}, allocator(state));
        for (std::uint64_t i = 0; i < 100; ++i)
        {
            values.try_emplace(i, i);
        }
        std::size_t live = state->live.size();
        *reject_hash = true;
        EXPECT_THROW(values.emplace(1000, 1000), std::runtime_error);
        EXPECT_THROW(values.erase(0), std::runtime_error);
        *reject_hash = false;
        *reject_compare = true;
        EXPECT_THROW(values.emplace(1000, 1000), std::runtime_error);
        *reject_compare = false;
        EXPECT_EQ(state->live.size(), live);
        EXPECT_EQ(values.size(), 100U);
        for (std::uint64_t i = 0; i < 100; ++i)
        {
            EXPECT_EQ(values.at(i), i);
        }
        state->verify();
    }
    EXPECT_TRUE(state->live.empty());
}

TEST(hadix_map, failed_insert_does_not_repeat_a_completed_split)
{
    auto state = std::make_shared< hadix_tests::allocation_state >();
    using allocator = hadix_tests::audit_allocator< std::pair< std::uint64_t const, std::uint64_t > >;
    using map = rpnx::hadix_map< std::uint64_t, std::uint64_t, hadix_tests::constant_hash, std::compare_three_way, allocator >;
    {
        map values({}, {}, allocator(state));
        for (std::uint64_t i = 0; i < 64; ++i)
        {
            values.try_emplace(i, i);
        }
        ASSERT_EQ(values.bucket_count(), 8U);
        for (std::size_t attempt = 0; attempt < 10; ++attempt)
        {
            state->allocation_budget = 1;
            EXPECT_THROW(values.try_emplace(999, 999), std::bad_alloc);
            EXPECT_EQ(values.size(), 64U);
            EXPECT_EQ(values.bucket_count(), 9U);
            state->verify();
        }
        state->allocation_budget = -1;
        values.try_emplace(999, 999);
        EXPECT_EQ(values.bucket_count(), 9U);
        values.erase(999);
        EXPECT_EQ(values.bucket_count(), 9U);
        for (std::uint64_t i = 0; i < 64; ++i)
        {
            EXPECT_EQ(values.at(i), i);
        }
    }
    EXPECT_TRUE(state->live.empty());
}

TEST(hadix_map, repeated_failed_joins_do_not_increase_later_mutation_work)
{
    auto state = std::make_shared< hadix_tests::allocation_state >();
    using allocator = hadix_tests::audit_allocator< std::pair< std::uint64_t const, std::uint64_t > >;
    using map = rpnx::hadix_map< std::uint64_t, std::uint64_t, hadix_tests::join_hash, std::compare_three_way, allocator >;
    {
        map values({}, {}, allocator(state));
        for (std::uint64_t i = 0; i < 512; ++i)
        {
            values.try_emplace(i, i);
        }
        state->allocation_budget = 0;
        for (std::uint64_t i = 0; i < 300; ++i)
        {
            try
            {
                values.erase(i);
            }
            catch (std::bad_alloc const&)
            {
            }
            EXPECT_EQ(values.bucket_count(), 64U);
        }
        EXPECT_EQ(values.size(), 212U);
        state->allocation_budget = -1;
        values.try_emplace(10000, 10000);
        EXPECT_EQ(values.bucket_count(), 64U);
        values.erase(10000);
        EXPECT_EQ(values.bucket_count(), 63U);
        state->verify();
        for (std::uint64_t i = 300; i < 512; ++i)
        {
            EXPECT_EQ(values.at(i), i);
        }
    }
    EXPECT_TRUE(state->live.empty());
}
