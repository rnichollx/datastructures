// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0
#ifndef RPNX_HADIX_MAP_HPP
#define RPNX_HADIX_MAP_HPP

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <rpnx/geometric_array.hpp>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

namespace rpnx
{
    /**
     * @brief Hash radix map with stable value references and an unordered_map-style API.
     *
     * KeyCompareThreeWay defines both key equivalence and ordering. Equivalent
     * keys must hash equally. Ordering compares the reversed hash first and keys
     * only for equal hashes. Stored keys are immutable. The comparator returns a
     * standard comparison category and must not return unordered for input keys.
     *
     * Buckets contain the smallest 15 entries inline and an independent overflow
     * root. Hashes and counts use std::size_t. On 64-bit platforms each bucket is
     * 256 bytes aligned to 64 bytes; on 32-bit platforms it is 128 bytes aligned
     * to 32 bytes. Values are separate allocations; overflow nodes carry
     * only indexing metadata. Allocator rebinds manage all three allocation types.
     *
     * Insert and erase preserve references/pointers to retained values.
     * Structural changes may invalidate all iterators. Iteration follows
     * ordering-hash ranges, then key order within each equal-hash group. Traversal
     * costs O(size() + bucket_count()). No concurrent mutation is supported.
     *
     * Target occupancy is fixed at eight entries per bucket, with two spare buckets
     * of hysteresis. Each insertion splits at most one bucket; each single-key
     * erasure joins at most one pair. Insertion, lookup and erasure by key have
     * O(log(size()+1)) worst-case structural work and expected O(1) work with
     * well-distributed hashes. Allocation and user hash/comparison/construction
     * costs are excluded. Iterator erasure also locates the next occupied
     * hash range, which may scan empty buckets.
     *
     * Operations provide the basic exception guarantee. Failed insertions do not
     * add an entry, but may split buckets. Erasure may remove entries before a
     * shrinking allocation fails. Excess buckets after a failed join are reclaimed
     * incrementally by later erasures, preserving the bound on joins per erasure.
     * Retained values are never relocated by either.
     * Allocator-extended moves with unequal allocators move individual values.
     *
     * The API follows std::unordered_map with a three-way key comparison policy.
     * Bucket reservation, manual rehashing and load-policy setters are not provided.
     * Average occupancy is available as the ratio of size() to bucket_count();
     * no floating-point load_factor() observer is provided.
     * Node-handle extraction/merge and heterogeneous lookup are not provided.
     *
     * @tparam Key Immutable key type.
     * @tparam T Mapped value type.
     * @tparam Hash Key hash function returning an unsigned integer with exactly std::size_t value bits.
     * @tparam KeyCompareThreeWay Three-way comparison of keys with equal hashes.
     * @tparam Allocator Allocator for key/value pairs.
     */
    template < typename Key, typename T, typename Hash = std::hash< Key >, typename KeyCompareThreeWay = std::compare_three_way, typename Allocator = std::allocator< std::pair< Key const, T > > >
    class hadix_map
    {
      public:
        using key_type = Key;
        using mapped_type = T;
        using value_type = std::pair< Key const, T >;
        using hasher = Hash;
        using key_compare_three_way = KeyCompareThreeWay;
        using allocator_type = Allocator;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using reference = value_type&;
        using const_reference = value_type const&;
        using pointer = typename std::allocator_traits< Allocator >::pointer;
        using const_pointer = typename std::allocator_traits< Allocator >::const_pointer;

      private:
        using alloc_traits = std::allocator_traits< Allocator >;
        using hash_type = std::size_t;
        static constexpr size_type hash_bits = std::numeric_limits< hash_type >::digits;
        using comparison_category = std::common_comparison_category_t< std::strong_ordering, std::invoke_result_t< KeyCompareThreeWay const&, key_type const&, key_type const& > >;
        static_assert(!std::is_void_v< comparison_category >, "hadix_map key comparator must return a standard comparison category");
        static constexpr size_type inline_capacity = 15;
        static constexpr size_type target_occupancy = 8;
        static constexpr size_type bucket_hysteresis = 2;
        static_assert(std::is_same_v< value_type, typename alloc_traits::value_type >);
        using hash_result = std::invoke_result_t< Hash const&, key_type const& >;
        static_assert(std::is_integral_v< hash_result > && std::is_unsigned_v< hash_result > && std::numeric_limits< hash_result >::digits == hash_bits, "hadix_map hash must return an unsigned integer with exactly std::size_t value bits");

        /**
         * @brief AVL index with its lookup fields in a four-word aligned prefix.
         * The separately allocated value never moves when indexing nodes change.
         */
        struct alignas(4 * sizeof(void*)) tree_node
        {
            hash_type hash;
            tree_node* left = nullptr;
            tree_node* right = nullptr;
            value_type* value;
            tree_node* parent = nullptr;
            size_type count = 1;
            std::int32_t height = 1;

            /** @brief Creates detached indexing metadata for an existing value. */
            tree_node(value_type* input, hash_type ordering_hash) noexcept : hash(ordering_hash), value(input)
            {
            }

            /** @brief Returns the height of a possibly empty subtree. */
            static std::int32_t height_of(tree_node* node) noexcept
            {
                return node ? node->height : 0;
            }
            /** @brief Returns the size of a possibly empty subtree. */
            static size_type size_of(tree_node* node) noexcept
            {
                return node ? node->count : 0;
            }
            /** @brief Recomputes cached metadata and establishes child parent links. */
            void refresh() noexcept
            {
                height = 1 + std::max(height_of(left), height_of(right));
                count = 1 + size_of(left) + size_of(right);
                if (left)
                {
                    left->parent = this;
                }
                if (right)
                {
                    right->parent = this;
                }
            }
            /** @brief Rotates a subtree left without moving values. */
            static tree_node* rotate_left(tree_node* root) noexcept
            {
                tree_node* result = root->right;
                root->right = result->left;
                result->left = root;
                root->refresh();
                result->refresh();
                result->parent = nullptr;
                return result;
            }
            /** @brief Rotates a subtree right without moving values. */
            static tree_node* rotate_right(tree_node* root) noexcept
            {
                tree_node* result = root->left;
                root->left = result->right;
                result->right = root;
                root->refresh();
                result->refresh();
                result->parent = nullptr;
                return result;
            }
            /** @brief Restores AVL balance after a child height changes by one. */
            static tree_node* balance(tree_node* root) noexcept
            {
                root->refresh();
                root->parent = nullptr;
                if (height_of(root->left) > height_of(root->right) + 1)
                {
                    if (height_of(root->left->right) > height_of(root->left->left))
                    {
                        root->left = rotate_left(root->left);
                    }
                    return rotate_right(root);
                }
                if (height_of(root->right) > height_of(root->left) + 1)
                {
                    if (height_of(root->right->left) > height_of(root->right->right))
                    {
                        root->right = rotate_right(root->right);
                    }
                    return rotate_left(root);
                }
                return root;
            }
            /**
             * @brief Joins ordered subtrees around a pivot in O(abs(height difference)+1).
             * @pre Every left entry precedes pivot, which precedes every right entry.
             */
            static tree_node* join(tree_node* left, tree_node* pivot, tree_node* right) noexcept
            {
                if (height_of(left) > height_of(right) + 1)
                {
                    left->right = join(left->right, pivot, right);
                    return balance(left);
                }
                if (height_of(right) > height_of(left) + 1)
                {
                    right->left = join(left, pivot, right->left);
                    return balance(right);
                }
                pivot->left = left;
                pivot->right = right;
                pivot->refresh();
                pivot->parent = nullptr;
                return pivot;
            }
            /** @brief Detaches the minimum node, returning the remaining AVL root. */
            static tree_node* extract_minimum(tree_node* root, tree_node*& extracted) noexcept
            {
                if (!root->left)
                {
                    extracted = root;
                    tree_node* result = root->right;
                    if (result)
                    {
                        result->parent = nullptr;
                    }
                    root->right = nullptr;
                    root->parent = nullptr;
                    root->refresh();
                    return result;
                }
                root->left = extract_minimum(root->left, extracted);
                return balance(root);
            }
            /** @brief Concatenates two disjoint ordered trees in O(log(total size+1)). */
            static tree_node* concatenate(tree_node* left, tree_node* right) noexcept
            {
                if (!left)
                {
                    if (right)
                    {
                        right->parent = nullptr;
                    }
                    return right;
                }
                if (!right)
                {
                    left->parent = nullptr;
                    return left;
                }
                tree_node* pivot = nullptr;
                right = extract_minimum(right, pivot);
                return join(left, pivot, right);
            }
            /** @brief Inserts a detached node at a previously determined rank. */
            static tree_node* insert(tree_node* root, tree_node* entry, size_type rank) noexcept
            {
                if (!root)
                {
                    assert(rank == 0);
                    return entry;
                }
                size_type left_count = size_of(root->left);
                if (rank <= left_count)
                {
                    root->left = insert(root->left, entry, rank);
                }
                else
                {
                    root->right = insert(root->right, entry, rank - left_count - 1);
                }
                return balance(root);
            }
            /** @brief Detaches the node at rank while preserving every other node address. */
            static tree_node* erase(tree_node* root, size_type rank, tree_node*& removed) noexcept
            {
                size_type left_count = size_of(root->left);
                if (rank < left_count)
                {
                    root->left = erase(root->left, rank, removed);
                }
                else if (rank > left_count)
                {
                    root->right = erase(root->right, rank - left_count - 1, removed);
                }
                else
                {
                    removed = root;
                    return concatenate(root->left, root->right);
                }
                return balance(root);
            }
            /** @brief Partitions into hashes below boundary and hashes at/above boundary. */
            static std::pair< tree_node*, tree_node* > split(tree_node* root, hash_type boundary) noexcept
            {
                if (!root)
                {
                    return {nullptr, nullptr};
                }
                if (root->hash < boundary)
                {
                    std::pair< tree_node*, tree_node* > parts = split(root->right, boundary);
                    return {join(root->left, root, parts.first), parts.second};
                }
                std::pair< tree_node*, tree_node* > parts = split(root->left, boundary);
                return {parts.first, join(parts.second, root, root->right)};
            }
            /** @brief Links sorted detached nodes into a balanced tree in linear time. */
            static tree_node* from_sorted(tree_node** nodes, size_type count) noexcept
            {
                if (count == 0)
                {
                    return nullptr;
                }
                size_type middle = count / 2;
                tree_node* root = nodes[middle];
                root->left = from_sorted(nodes, middle);
                root->right = from_sorted(nodes + middle + 1, count - middle - 1);
                root->refresh();
                root->parent = nullptr;
                return root;
            }
            /** @brief Returns the minimum node of a possibly empty tree. */
            static tree_node* minimum(tree_node* root) noexcept
            {
                if (!root)
                {
                    return nullptr;
                }
                while (root->left)
                {
                    root = root->left;
                }
                return root;
            }
            /** @brief Returns the next node in tuple order, or null at the tree end. */
            static tree_node* successor(tree_node* node) noexcept
            {
                if (node->right)
                {
                    return minimum(node->right);
                }
                while (node->parent && node == node->parent->right)
                {
                    node = node->parent;
                }
                return node->parent;
            }
            /** @brief Returns the node's rank from subtree counts and parent links. */
            size_type rank() const noexcept
            {
                size_type result = size_of(left);
                tree_node const* node = this;
                while (node->parent)
                {
                    if (node == node->parent->right)
                    {
                        result += 1 + size_of(node->parent->left);
                    }
                    node = node->parent;
                }
                return result;
            }
            /** @brief Returns the node at a valid rank. */
            static tree_node* select(tree_node* root, size_type rank) noexcept
            {
                while (root)
                {
                    size_type left_count = size_of(root->left);
                    if (rank < left_count)
                    {
                        root = root->left;
                    }
                    else if (rank > left_count)
                    {
                        rank -= left_count + 1;
                        root = root->right;
                    }
                    else
                    {
                        return root;
                    }
                }
                return nullptr;
            }
        };

        /** @brief Aligned bucket owning inline values and its overflow tree. */
        struct alignas(8 * sizeof(size_type)) bucket_storage
        {
            size_type count = 0;
            std::array< hash_type, inline_capacity > hashes{};
            std::array< value_type*, inline_capacity > values{};
            tree_node* root = nullptr;
        };
        static_assert(sizeof(void*) != sizeof(size_type) || sizeof(bucket_storage) == 32 * sizeof(size_type));
        using bucket_allocator = typename alloc_traits::template rebind_alloc< bucket_storage >;
        using bucket_array = geometric_array< bucket_storage, bucket_allocator >;

        [[no_unique_address]] Allocator m_allocator;
        [[no_unique_address]] Hash m_hash;
        [[no_unique_address]] KeyCompareThreeWay m_compare_three_way;
        bucket_array m_buckets;
        size_type m_size = 0;

        /** @brief Search result with an inline slot, optional tree rank and matching value. */
        struct search_result
        {
            size_type rank;
            value_type* value;
            tree_node* tree;
        };

        /** @brief Allocates and constructs one value or indexing node with allocator rebinding. */
        template < typename Object, typename... Args >
        Object* allocate_object(Args&&... args)
        {
            using object_allocator = typename alloc_traits::template rebind_alloc< Object >;
            using object_traits = std::allocator_traits< object_allocator >;
            object_allocator allocator(m_allocator);
            typename object_traits::pointer allocation = object_traits::allocate(allocator, 1);
            try
            {
                object_traits::construct(allocator, std::to_address(allocation), std::forward< Args >(args)...);
            }
            catch (...)
            {
                object_traits::deallocate(allocator, allocation, 1);
                throw;
            }
            return std::to_address(allocation);
        }
        /** @brief Destroys and deallocates one object through its rebound allocator. */
        template < typename Object >
        void destroy_object(Object* object) noexcept
        {
            using object_allocator = typename alloc_traits::template rebind_alloc< Object >;
            using object_traits = std::allocator_traits< object_allocator >;
            object_allocator allocator(m_allocator);
            typename object_traits::pointer allocation = std::pointer_traits< typename object_traits::pointer >::pointer_to(*object);
            object_traits::destroy(allocator, object);
            object_traits::deallocate(allocator, allocation, 1);
        }
        /** @brief Releases an uncommitted value if insertion throws or finds a duplicate. */
        struct value_deleter
        {
            hadix_map* owner;
            /** @brief Returns the value to its owning allocator. */
            void operator()(value_type* value) const noexcept
            {
                owner->destroy_object(value);
            }
        };
        /** @brief Destroys all overflow values and indexing nodes in postorder. */
        void destroy_tree(tree_node* root) noexcept
        {
            if (!root)
            {
                return;
            }
            destroy_tree(root->left);
            destroy_tree(root->right);
            destroy_object(root->value);
            destroy_object(root);
        }
        /** @brief Reverses every std::size_t value bit between indexing and ordering hashes. */
        static hash_type reverse_bits(hash_type value) noexcept
        {
            for (size_type shift = 1; shift < hash_bits; shift *= 2)
            {
                hash_type mask = std::numeric_limits< hash_type >::max() / ((hash_type{1} << shift) + 1);
                value = ((value & mask) << shift) | ((value >> shift) & mask);
            }
            return value;
        }
        /** @brief Folds an indexing hash into an arbitrary positive bucket count. */
        size_type bucket_index(hash_type indexing_hash) const noexcept
        {
            size_type count = m_buckets.size();
            assert(count != 0);
            size_type high_mask = std::bit_ceil(count) - 1;
            size_type candidate = static_cast< size_type >(indexing_hash) & high_mask;
            return candidate < count ? candidate : candidate & (high_mask >> 1);
        }
        /** @brief Returns the next bucket in ordering-hash range order, or bucket_count(). */
        size_type next_bucket(size_type index) const noexcept
        {
            size_type base = std::bit_floor(m_buckets.size());
            size_type split_count = m_buckets.size() - base;
            size_type bits = std::bit_width(base) - 1;
            if ((index & (base - 1)) < split_count)
            {
                ++bits;
            }
            if (bits == 0)
            {
                return m_buckets.size();
            }
            hash_type prefix = reverse_bits(static_cast< hash_type >(index));
            hash_type next = prefix + (hash_type{1} << (hash_bits - bits));
            return next == 0 ? m_buckets.size() : bucket_index(reverse_bits(next));
        }
        /** @brief Compares ordering hashes first and keys only when hashes match. */
        comparison_category compare_entry(hash_type hash, key_type const& key, hash_type stored_hash, value_type const& stored) const
        {
            if (hash != stored_hash)
            {
                return hash <=> stored_hash;
            }
            return m_compare_three_way(key, stored.first);
        }
        /**
         * @brief Searches a bucket, optionally accumulating the tree insertion rank.
         * Independent hash comparisons form a bit mask of the inline equal-hash
         * range before any keys are accessed. Only occupied slots remain in the mask.
         * Lookup-only searches use inline_capacity for tree slots and access only
         * the four lookup fields in each node, without reading subtree counts.
         * Inlining avoids passing the intermediate search result through memory.
         */
        template < bool ComputeRank >
#if __has_cpp_attribute(gnu::always_inline)
        [[gnu::always_inline]]
#endif
        search_result search(bucket_storage const& bucket, hash_type hash, key_type const& key) const
        {
            std::uint32_t matches = 0;
            size_type occupied = std::min(bucket.count, inline_capacity);
            for (size_type i = 0; i < inline_capacity; ++i)
            {
                matches |= static_cast< std::uint32_t >(bucket.hashes[i] == hash) << i;
            }
            matches &= (std::uint32_t{1} << occupied) - 1;
            size_type first = std::countr_zero(matches);
            size_type last = std::bit_width(matches);
            while (first < last)
            {
                size_type middle = first + (last - first) / 2;
                comparison_category comparison = m_compare_three_way(key, bucket.values[middle]->first);
                if (comparison == 0)
                {
                    return {middle, bucket.values[middle], nullptr};
                }
                if (comparison < 0)
                {
                    last = middle;
                }
                else
                {
                    first = middle + 1;
                }
            }
            if constexpr (ComputeRank)
            {
                if (matches == 0)
                {
                    first = 0;
                    last = occupied;
                    while (first < last)
                    {
                        size_type middle = first + (last - first) / 2;
                        if (bucket.hashes[middle] < hash)
                        {
                            first = middle + 1;
                        }
                        else
                        {
                            last = middle;
                        }
                    }
                }
            }
            if (first < inline_capacity || !bucket.root)
            {
                return {first, nullptr, nullptr};
            }
            tree_node* node = bucket.root;
            size_type rank = inline_capacity;
            while (node)
            {
                comparison_category comparison = compare_entry(hash, key, node->hash, *node->value);
                if (comparison == 0)
                {
                    if constexpr (ComputeRank)
                    {
                        rank += tree_node::size_of(node->left);
                    }
                    return {rank, node->value, node};
                }
                if (comparison < 0)
                {
                    node = node->left;
                }
                else
                {
                    if constexpr (ComputeRank)
                    {
                        rank += tree_node::size_of(node->left) + 1;
                    }
                    node = node->right;
                }
            }
            return {rank, nullptr, nullptr};
        }
        /** @brief Restores the smallest entries to the inline prefix without allocating. */
        void fill_inline(bucket_storage& bucket, size_type occupied) noexcept
        {
            size_type target = std::min(bucket.count, inline_capacity);
            while (occupied < target)
            {
                tree_node* extracted = nullptr;
                bucket.root = tree_node::extract_minimum(bucket.root, extracted);
                bucket.hashes[occupied] = extracted->hash;
                bucket.values[occupied] = extracted->value;
                ++occupied;
                destroy_object(extracted);
            }
        }
        /**
         * @brief Appends one bucket by splitting its partner at a reversed-hash boundary.
         * Allocation precedes mutation; tree splitting and prefix restoration cannot throw.
         */
        void split_bucket()
        {
            size_type index = m_buckets.size();
            if (index == 0)
            {
                m_buckets.emplace_back();
                return;
            }
            size_type partner = index - std::bit_floor(index);
            hash_type boundary = reverse_bits(static_cast< hash_type >(index));
            m_buckets.emplace_back();
            bucket_storage& left = m_buckets[partner];
            bucket_storage& right = m_buckets[index];
            size_type occupied = std::min(left.count, inline_capacity);
            size_type cut = 0;
            while (cut < occupied && left.hashes[cut] < boundary)
            {
                ++cut;
            }
            for (size_type i = cut; i < occupied; ++i)
            {
                right.hashes[i - cut] = left.hashes[i];
                right.values[i - cut] = left.values[i];
                left.values[i] = nullptr;
            }
            std::pair< tree_node*, tree_node* > parts = tree_node::split(left.root, boundary);
            left.root = parts.first;
            right.root = parts.second;
            left.count = cut + tree_node::size_of(left.root);
            right.count = occupied - cut + tree_node::size_of(right.root);
            fill_inline(left, cut);
            fill_inline(right, occupied - cut);
        }
        /**
         * @brief Removes the final bucket by joining it to its lower-hash partner.
         * At most 15 new indexing nodes are staged before any live storage changes.
         */
        void join_bucket()
        {
            size_type index = m_buckets.size() - 1;
            assert(index != 0);
            size_type partner = index - std::bit_floor(index);
            bucket_storage& left = m_buckets[partner];
            bucket_storage& right = m_buckets[index];
            size_type left_inline = std::min(left.count, inline_capacity);
            size_type right_inline = std::min(right.count, inline_capacity);
            size_type retained = std::min(inline_capacity - left_inline, right_inline);
            std::array< tree_node*, inline_capacity > staged{};
            size_type allocated = 0;
            try
            {
                for (size_type i = retained; i < right_inline; ++i)
                {
                    staged[allocated] = allocate_object< tree_node >(right.values[i], right.hashes[i]);
                    ++allocated;
                }
            }
            catch (...)
            {
                for (size_type i = 0; i < allocated; ++i)
                {
                    destroy_object(staged[i]);
                }
                throw;
            }
            for (size_type i = 0; i < retained; ++i)
            {
                left.hashes[left_inline + i] = right.hashes[i];
                left.values[left_inline + i] = right.values[i];
            }
            tree_node* suffix = tree_node::concatenate(tree_node::from_sorted(staged.data(), allocated), right.root);
            left.root = tree_node::concatenate(left.root, suffix);
            left.count += right.count;
            fill_inline(left, left_inline + retained);
            right = bucket_storage{};
            m_buckets.pop_back();
        }
        /** @brief Computes the fixed occupancy requirement without rounding or overflow. */
        static size_type required_buckets(size_type count) noexcept
        {
            return std::max(size_type{1}, count / target_occupancy + (count % target_occupancy != 0));
        }
        /** @brief Adds at most one bucket to meet the next insertion's occupancy requirement. */
        void grow_for_insert()
        {
            if (m_size == max_size())
            {
                throw std::length_error("hadix_map size exceeds max_size");
            }
            if (m_buckets.size() < required_buckets(m_size + 1))
            {
                split_bucket();
            }
        }
        /**
         * @brief Joins at most ceil(erased_count / target_occupancy) bucket pairs.
         * Failed joins leave excess buckets for later erasures without increasing
         * the amount of resizing performed by a subsequent single-key erasure.
         */
        void shrink_after_erase(size_type erased_count)
        {
            size_type target = required_buckets(m_size);
            size_type remaining = required_buckets(erased_count);
            while (remaining != 0 && m_buckets.size() > target && m_buckets.size() - target > bucket_hysteresis)
            {
                join_bucket();
                --remaining;
            }
        }

      public:
        /** @brief Forward iterator through inline entries and parent-linked AVL nodes. */
        template < bool Const, bool Local = false >
        class iterator_impl
        {
            friend class hadix_map;
            template < bool, bool >
            friend class iterator_impl;
            using owner_type = std::conditional_t< Const, hadix_map const, hadix_map >;
            owner_type* m_owner = nullptr;
            size_type m_bucket = 0;
            size_type m_slot = inline_capacity;
            tree_node* m_tree = nullptr;
            /** @brief Cached value at the current slot or tree node, or null at end. */
            typename hadix_map::value_type* m_value = nullptr;

            /** @brief Constructs an iterator at an already located entry or end position. */
            iterator_impl(owner_type* owner, size_type bucket, size_type slot, tree_node* tree, typename hadix_map::value_type* value) noexcept : m_owner(owner), m_bucket(bucket), m_slot(slot), m_tree(tree), m_value(value)
            {
            }
            /** @brief Locates the first entry in or after a bucket. */
            iterator_impl(owner_type* owner, size_type bucket) noexcept : m_owner(owner), m_bucket(bucket), m_slot(0)
            {
                seek_bucket();
            }
            /** @brief Skips empty buckets, respecting a local iterator's boundary. */
            void seek_bucket() noexcept
            {
                while (m_bucket < m_owner->m_buckets.size())
                {
                    if (m_owner->m_buckets[m_bucket].count != 0)
                    {
                        m_slot = 0;
                        m_value = m_owner->m_buckets[m_bucket].values[0];
                        return;
                    }
                    if constexpr (Local)
                    {
                        break;
                    }
                    else
                    {
                        m_bucket = m_owner->next_bucket(m_bucket);
                    }
                }
                m_slot = inline_capacity;
                m_value = nullptr;
            }
            /** @brief Returns the cached hash of a dereferenceable iterator. */
            hash_type ordering_hash() const noexcept
            {
                return m_slot < inline_capacity ? m_owner->m_buckets[m_bucket].hashes[m_slot] : m_tree->hash;
            }
            /** @brief Returns the current rank within its bucket. */
            size_type rank() const noexcept
            {
                return m_slot < inline_capacity ? m_slot : inline_capacity + m_tree->rank();
            }

          public:
            using value_type = typename hadix_map::value_type;
            using difference_type = typename hadix_map::difference_type;
            using reference = std::conditional_t< Const, value_type const&, value_type& >;
            using pointer = std::conditional_t< Const, value_type const*, value_type* >;
            using iterator_category = std::forward_iterator_tag;
            using iterator_concept = std::forward_iterator_tag;

            /** @brief Creates a singular iterator. */
            iterator_impl() = default;
            /** @brief Converts a mutable iterator to a const iterator. */
            template < bool Other >
                requires(Const && !Other)
            iterator_impl(iterator_impl< Other, Local > const& other) noexcept : m_owner(other.m_owner), m_bucket(other.m_bucket), m_slot(other.m_slot), m_tree(other.m_tree), m_value(other.m_value)
            {
            }
            /** @brief Accesses the stored value. */
            reference operator*() const noexcept
            {
                return *m_value;
            }
            /** @brief Returns a pointer to the stored value. */
            pointer operator->() const noexcept
            {
                return m_value;
            }
            /** @brief Advances in ordering-hash order and then key order. */
            iterator_impl& operator++() noexcept
            {
                assert(m_value != nullptr);
                if (m_slot < inline_capacity)
                {
                    ++m_slot;
                    if (m_slot < std::min(m_owner->m_buckets[m_bucket].count, inline_capacity))
                    {
                        m_value = m_owner->m_buckets[m_bucket].values[m_slot];
                        return *this;
                    }
                    m_slot = inline_capacity;
                    m_tree = tree_node::minimum(m_owner->m_buckets[m_bucket].root);
                }
                else
                {
                    m_tree = tree_node::successor(m_tree);
                }
                m_value = m_tree ? m_tree->value : nullptr;
                if (!m_tree)
                {
                    if constexpr (!Local)
                    {
                        m_bucket = m_owner->next_bucket(m_bucket);
                        seek_bucket();
                    }
                }
                return *this;
            }
            /** @brief Advances and returns the previous iterator. */
            iterator_impl operator++(int) noexcept
            {
                iterator_impl previous = *this;
                ++*this;
                return previous;
            }
            /** @brief Compares positions, permitting mutable/const iterator comparison. */
            template < bool Other >
            bool operator==(iterator_impl< Other, Local > const& other) const noexcept
            {
                return m_owner == other.m_owner && m_bucket == other.m_bucket && m_slot == other.m_slot && m_tree == other.m_tree;
            }
        };

        using iterator = iterator_impl< false >;
        using const_iterator = iterator_impl< true >;
        using local_iterator = iterator_impl< false, true >;
        using const_local_iterator = iterator_impl< true, true >;

      private:
        /** @brief Finds an entry using its cached ordering hash. */
        iterator locate(key_type const& key, hash_type hash)
        {
            if (m_size == 0)
            {
                return end();
            }
            size_type index = bucket_index(reverse_bits(hash));
            search_result found = search< false >(m_buckets[index], hash, key);
            if (!found.value)
            {
                return end();
            }
            return iterator(this, index, std::min(found.rank, inline_capacity), found.tree, found.value);
        }
        /** @brief Commits an owned candidate or destroys it on duplicate/failure. */
        std::pair< iterator, bool > insert_candidate(value_type* value)
        {
            std::unique_ptr< value_type, value_deleter > candidate(value, value_deleter{this});
            key_type const& key = value->first;
            hash_type indexing_hash = static_cast< hash_type >(m_hash(key));
            hash_type hash = reverse_bits(indexing_hash);
            iterator existing = locate(key, hash);
            if (existing != end())
            {
                return {existing, false};
            }
            grow_for_insert();
            size_type index = bucket_index(indexing_hash);
            bucket_storage& bucket = m_buckets[index];
            search_result position = search< true >(bucket, hash, key);
            size_type occupied = std::min(bucket.count, inline_capacity);
            tree_node* inserted_tree = nullptr;
            if (occupied == inline_capacity)
            {
                if (position.rank < inline_capacity)
                {
                    tree_node* displaced = allocate_object< tree_node >(bucket.values.back(), bucket.hashes.back());
                    bucket.root = tree_node::insert(bucket.root, displaced, 0);
                }
                else
                {
                    inserted_tree = allocate_object< tree_node >(value, hash);
                    bucket.root = tree_node::insert(bucket.root, inserted_tree, position.rank - inline_capacity);
                }
            }
            if (position.rank < inline_capacity)
            {
                size_type last = std::min(occupied, inline_capacity - 1);
                for (size_type i = last; i > position.rank; --i)
                {
                    bucket.hashes[i] = bucket.hashes[i - 1];
                    bucket.values[i] = bucket.values[i - 1];
                }
                bucket.hashes[position.rank] = hash;
                bucket.values[position.rank] = value;
            }
            ++bucket.count;
            ++m_size;
            candidate.release();
            return {iterator(this, index, std::min(position.rank, inline_capacity), inserted_tree, value), true};
        }
        /** @brief Removes an entry by rank without hashing, comparing keys or scanning buckets. */
        void erase_rank(size_type index, size_type rank) noexcept
        {
            bucket_storage& bucket = m_buckets[index];
            if (rank < inline_capacity)
            {
                destroy_object(bucket.values[rank]);
                size_type occupied = std::min(bucket.count, inline_capacity);
                for (size_type i = rank + 1; i < occupied; ++i)
                {
                    bucket.hashes[i - 1] = bucket.hashes[i];
                    bucket.values[i - 1] = bucket.values[i];
                }
                bucket.values[occupied - 1] = nullptr;
                --bucket.count;
                fill_inline(bucket, occupied - 1);
            }
            else
            {
                tree_node* removed = nullptr;
                bucket.root = tree_node::erase(bucket.root, rank - inline_capacity, removed);
                destroy_object(removed->value);
                destroy_object(removed);
                --bucket.count;
            }
            --m_size;
        }
        /** @brief Locates an existing rank, advancing to the next occupied range at a bucket end. */
        iterator iterator_at_rank(size_type index, size_type rank) noexcept
        {
            if (rank == m_buckets[index].count)
            {
                return iterator(this, next_bucket(index));
            }
            if (rank < inline_capacity)
            {
                return iterator(this, index, rank, nullptr, m_buckets[index].values[rank]);
            }
            tree_node* node = tree_node::select(m_buckets[index].root, rank - inline_capacity);
            return iterator(this, index, inline_capacity, node, node->value);
        }
        /** @brief Transfers indexing storage and counters into an empty map. */
        void take_storage(hadix_map& other)
        {
            assert(m_size == 0);
            m_buckets = std::move(other.m_buckets);
            m_size = std::exchange(other.m_size, 0);
        }
        /** @brief Replaces an empty map's policies before transferring prepared storage. */
        void adopt(hadix_map& replacement)
        {
            assert(m_size == 0);
            m_hash = replacement.m_hash;
            m_compare_three_way = replacement.m_compare_three_way;
            take_storage(replacement);
        }

        /**
         * @brief Constructs a value only if its key is absent.
         * @pre The supplied constructor arguments produce a value with the supplied key.
         */
        template < typename... Args >
        std::pair< iterator, bool > emplace_key(key_type const& key, Args&&... args)
        {
            iterator existing = find(key);
            if (existing != end())
            {
                return {existing, false};
            }
            return insert_candidate(allocate_object< value_type >(std::forward< Args >(args)...));
        }

      public:
        /** @brief Creates an allocation-free empty table with default policies. */
        hadix_map() : hadix_map(Hash(), KeyCompareThreeWay(), Allocator())
        {
        }
        /** @brief Creates an empty table with an allocator. */
        explicit hadix_map(Allocator const& allocator) : hadix_map(Hash(), KeyCompareThreeWay(), allocator)
        {
        }
        /** @brief Creates an allocation-free empty table with specified policy objects. */
        explicit hadix_map(Hash const& hash, KeyCompareThreeWay const& compare = KeyCompareThreeWay(), Allocator const& allocator = Allocator()) : m_allocator(allocator), m_hash(hash), m_compare_three_way(compare), m_buckets(bucket_allocator(allocator))
        {
        }
        /** @brief Creates an empty table with a hash function and allocator. */
        hadix_map(Hash const& hash, Allocator const& allocator) : hadix_map(hash, KeyCompareThreeWay(), allocator)
        {
        }
        /** @brief Inserts an iterator range, retaining the first value for each key. */
        template < std::input_iterator Input >
        hadix_map(Input first, Input last, Hash const& hash = Hash(), KeyCompareThreeWay const& compare = KeyCompareThreeWay(), Allocator const& allocator = Allocator()) : hadix_map(hash, compare, allocator)
        {
            insert(first, last);
        }
        /** @brief Copies an initializer list, retaining the first value for each key. */
        hadix_map(std::initializer_list< value_type > values, Hash const& hash = Hash(), KeyCompareThreeWay const& compare = KeyCompareThreeWay(), Allocator const& allocator = Allocator()) : hadix_map(values.begin(), values.end(), hash, compare, allocator)
        {
        }
        /** @brief Copies values using allocator copy-construction selection. */
        hadix_map(hadix_map const& other) : hadix_map(other, alloc_traits::select_on_container_copy_construction(other.m_allocator))
        {
        }
        /** @brief Copies values with a specified allocator. */
        hadix_map(hadix_map const& other, Allocator const& allocator) : hadix_map(other.m_hash, other.m_compare_three_way, allocator)
        {
            insert(other.begin(), other.end());
        }
        /** @brief Transfers storage while leaving the source empty and reusable. */
        hadix_map(hadix_map&& other) noexcept(std::is_nothrow_copy_constructible_v< Allocator > && std::is_nothrow_copy_constructible_v< Hash > && std::is_nothrow_copy_constructible_v< KeyCompareThreeWay >) : m_allocator(other.m_allocator), m_hash(other.m_hash), m_compare_three_way(other.m_compare_three_way), m_buckets(std::move(other.m_buckets)), m_size(std::exchange(other.m_size, 0))
        {
        }
        /** @brief Transfers storage for equal allocators, otherwise moves individual values. */
        hadix_map(hadix_map&& other, Allocator const& allocator) : hadix_map(other.m_hash, other.m_compare_three_way, allocator)
        {
            if constexpr (!alloc_traits::is_always_equal::value)
            {
                if (m_allocator != other.m_allocator)
                {
                    if constexpr (std::is_constructible_v< value_type, value_type&& >)
                    {
                        for (value_type& value : other)
                        {
                            emplace(std::move(value));
                        }
                        other.clear();
                    }
                    else
                    {
                        throw std::invalid_argument("hadix_map unequal-allocator move requires movable values");
                    }
                    return;
                }
            }
            take_storage(other);
        }
        /** @brief Releases all values, indexing nodes and bucket allocations. */
        ~hadix_map()
        {
            clear();
        }
        /** @brief Copies values and policies, respecting allocator propagation. */
        hadix_map& operator=(hadix_map const& other)
        {
            if (this == &other)
            {
                return *this;
            }
            Allocator allocator = m_allocator;
            if constexpr (alloc_traits::propagate_on_container_copy_assignment::value)
            {
                allocator = other.m_allocator;
            }
            hadix_map replacement(other, allocator);
            clear();
            if constexpr (alloc_traits::propagate_on_container_copy_assignment::value)
            {
                m_allocator = allocator;
                bucket_array empty_buckets{bucket_allocator(allocator)};
                m_buckets = empty_buckets;
            }
            adopt(replacement);
            return *this;
        }
        /** @brief Transfers or moves values according to allocator propagation. */
        hadix_map& operator=(hadix_map&& other)
        {
            if (this == &other)
            {
                return *this;
            }
            Allocator allocator = m_allocator;
            if constexpr (alloc_traits::propagate_on_container_move_assignment::value)
            {
                allocator = other.m_allocator;
            }
            hadix_map replacement(std::move(other), allocator);
            clear();
            if constexpr (alloc_traits::propagate_on_container_move_assignment::value)
            {
                m_allocator = allocator;
                m_buckets = bucket_array(bucket_allocator(allocator));
            }
            adopt(replacement);
            return *this;
        }
        /** @brief Replaces entries from an initializer list using the current policies. */
        hadix_map& operator=(std::initializer_list< value_type > values)
        {
            hadix_map replacement(values, m_hash, m_compare_three_way, m_allocator);
            clear();
            adopt(replacement);
            return *this;
        }
        /** @brief Returns the mapped value for key, throwing if absent. */
        T& at(Key const& key)
        {
            iterator found = find(key);
            if (found == end())
            {
                throw std::out_of_range("hadix_map::at key not found");
            }
            return found->second;
        }
        /** @brief Returns the immutable mapped value for key, throwing if absent. */
        T const& at(Key const& key) const
        {
            const_iterator found = find(key);
            if (found == end())
            {
                throw std::out_of_range("hadix_map::at key not found");
            }
            return found->second;
        }
        /** @brief Copies a missing key and constructs its mapped value in place. */
        template < typename... Args >
        std::pair< iterator, bool > try_emplace(Key const& key, Args&&... args)
        {
            return emplace_key(key, std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(std::forward< Args >(args)...));
        }
        /** @brief Moves a missing key and constructs its mapped value in place. */
        template < typename... Args >
        std::pair< iterator, bool > try_emplace(Key&& key, Args&&... args)
        {
            return emplace_key(key, std::piecewise_construct, std::forward_as_tuple(std::move(key)), std::forward_as_tuple(std::forward< Args >(args)...));
        }
        /** @brief Constructs a missing mapped value, ignoring the optional hint. */
        template < typename... Args >
        iterator try_emplace(const_iterator, Key const& key, Args&&... args)
        {
            return try_emplace(key, std::forward< Args >(args)...).first;
        }
        /** @brief Moves a missing key, ignoring the optional hint. */
        template < typename... Args >
        iterator try_emplace(const_iterator, Key&& key, Args&&... args)
        {
            return try_emplace(std::move(key), std::forward< Args >(args)...).first;
        }
        /** @brief Inserts a missing key or assigns the existing mapped value. */
        template < typename Mapped >
        std::pair< iterator, bool > insert_or_assign(Key const& key, Mapped&& value)
        {
            std::pair< iterator, bool > result = try_emplace(key, std::forward< Mapped >(value));
            if (!result.second)
            {
                result.first->second = std::forward< Mapped >(value);
            }
            return result;
        }
        /** @brief Moves a missing key or assigns the existing mapped value. */
        template < typename Mapped >
        std::pair< iterator, bool > insert_or_assign(Key&& key, Mapped&& value)
        {
            std::pair< iterator, bool > result = try_emplace(std::move(key), std::forward< Mapped >(value));
            if (!result.second)
            {
                result.first->second = std::forward< Mapped >(value);
            }
            return result;
        }
        /** @brief Inserts or assigns, ignoring the optional hint. */
        template < typename Mapped >
        iterator insert_or_assign(const_iterator, Key const& key, Mapped&& value)
        {
            return insert_or_assign(key, std::forward< Mapped >(value)).first;
        }
        /** @brief Inserts or assigns a moved key, ignoring the optional hint. */
        template < typename Mapped >
        iterator insert_or_assign(const_iterator, Key&& key, Mapped&& value)
        {
            return insert_or_assign(std::move(key), std::forward< Mapped >(value)).first;
        }
        /** @brief Returns the mapped value, default-constructing it if the key is absent. */
        T& operator[](Key const& key)
        {
            return try_emplace(key).first->second;
        }
        /** @brief Returns the mapped value, moving a missing key into the map. */
        T& operator[](Key&& key)
        {
            return try_emplace(std::move(key)).first->second;
        }

        /** @brief Returns the allocator used for values and rebound indexing storage. */
        allocator_type get_allocator() const noexcept
        {
            return m_allocator;
        }
        /** @brief Returns the current hash policy. */
        hasher hash_function() const
        {
            return m_hash;
        }
        /** @brief Returns the three-way policy defining key equivalence and ordering. */
        key_compare_three_way key_comp_three_way() const
        {
            return m_compare_three_way;
        }
        /** @brief Returns the number of stored entries. */
        size_type size() const noexcept
        {
            return m_size;
        }
        /** @brief Reports whether no entries are stored. */
        bool empty() const noexcept
        {
            return m_size == 0;
        }
        /** @brief Returns the maximum count representable by nodes, allocators and iterators. */
        size_type max_size() const noexcept
        {
            using node_allocator = typename alloc_traits::template rebind_alloc< tree_node >;
            node_allocator allocator(m_allocator);
            return std::min({static_cast< size_type >(alloc_traits::max_size(m_allocator)), static_cast< size_type >(std::allocator_traits< node_allocator >::max_size(allocator)), static_cast< size_type >(std::numeric_limits< difference_type >::max())});
        }
        /** @brief Returns the first entry, skipping empty buckets. */
        iterator begin() noexcept
        {
            return iterator(this, 0);
        }
        /** @brief Returns the first immutable entry, skipping empty buckets. */
        const_iterator begin() const noexcept
        {
            return const_iterator(this, 0);
        }
        /** @brief Returns the immutable beginning. */
        const_iterator cbegin() const noexcept
        {
            return begin();
        }
        /** @brief Returns the past-the-end iterator. */
        iterator end() noexcept
        {
            return iterator(this, m_buckets.size(), inline_capacity, nullptr, nullptr);
        }
        /** @brief Returns the immutable past-the-end iterator. */
        const_iterator end() const noexcept
        {
            return const_iterator(this, m_buckets.size(), inline_capacity, nullptr, nullptr);
        }
        /** @brief Returns the immutable end. */
        const_iterator cend() const noexcept
        {
            return end();
        }
        /** @brief Returns the first entry in bucket index. */
        local_iterator begin(size_type index) noexcept
        {
            assert(index < bucket_count());
            return local_iterator(this, index);
        }
        /** @brief Returns the first immutable entry in bucket index. */
        const_local_iterator begin(size_type index) const noexcept
        {
            assert(index < bucket_count());
            return const_local_iterator(this, index);
        }
        /** @brief Returns the immutable beginning of a bucket. */
        const_local_iterator cbegin(size_type index) const noexcept
        {
            return begin(index);
        }
        /** @brief Returns the past-the-end iterator for a bucket. */
        local_iterator end(size_type index) noexcept
        {
            assert(index < bucket_count());
            return local_iterator(this, index, inline_capacity, nullptr, nullptr);
        }
        /** @brief Returns the immutable past-the-end iterator for a bucket. */
        const_local_iterator end(size_type index) const noexcept
        {
            assert(index < bucket_count());
            return const_local_iterator(this, index, inline_capacity, nullptr, nullptr);
        }
        /** @brief Returns the immutable end of a bucket. */
        const_local_iterator cend(size_type index) const noexcept
        {
            return end(index);
        }
        /** @brief Finds an entry by key. */
        iterator find(key_type const& key)
        {
            return locate(key, reverse_bits(static_cast< hash_type >(m_hash(key))));
        }
        /** @brief Finds an immutable entry by key. */
        const_iterator find(key_type const& key) const
        {
            if (m_size == 0)
            {
                return end();
            }
            hash_type hash = static_cast< hash_type >(m_hash(key));
            size_type index = bucket_index(hash);
            search_result found = search< false >(m_buckets[index], reverse_bits(hash), key);
            if (!found.value)
            {
                return end();
            }
            return const_iterator(this, index, std::min(found.rank, inline_capacity), found.tree, found.value);
        }
        /** @brief Reports whether the key is present. */
        bool contains(key_type const& key) const
        {
            return find(key) != end();
        }
        /** @brief Returns one for a present key and zero otherwise. */
        size_type count(key_type const& key) const
        {
            return contains(key) ? 1 : 0;
        }
        /** @brief Returns the single matching entry's range, or two end iterators. */
        std::pair< iterator, iterator > equal_range(key_type const& key)
        {
            iterator first = find(key);
            iterator last = first;
            if (last != end())
            {
                ++last;
            }
            return {first, last};
        }
        /** @brief Returns the immutable matching range. */
        std::pair< const_iterator, const_iterator > equal_range(key_type const& key) const
        {
            const_iterator first = find(key);
            const_iterator last = first;
            if (last != end())
            {
                ++last;
            }
            return {first, last};
        }
        /** @brief Constructs a candidate entry, inserting it only if its key is absent. */
        template < typename... Args >
        std::pair< iterator, bool > emplace(Args&&... args)
        {
            return insert_candidate(allocate_object< value_type >(std::forward< Args >(args)...));
        }
        /** @brief Constructs an entry; the optional position hint is ignored. */
        template < typename... Args >
        iterator emplace_hint(const_iterator, Args&&... args)
        {
            return emplace(std::forward< Args >(args)...).first;
        }
        /** @brief Inserts a copy unless the key already exists. */
        std::pair< iterator, bool > insert(value_type const& value)
        {
            return emplace(value);
        }
        /** @brief Inserts a moved value unless the key already exists. */
        std::pair< iterator, bool > insert(value_type&& value)
        {
            return emplace(std::move(value));
        }
        /** @brief Inserts a value constructible as the stored type. */
        template < typename Value >
            requires std::is_constructible_v< value_type, Value&& >
        std::pair< iterator, bool > insert(Value&& value)
        {
            return emplace(std::forward< Value >(value));
        }
        /** @brief Inserts a copy, ignoring the optional hint. */
        iterator insert(const_iterator, value_type const& value)
        {
            return insert(value).first;
        }
        /** @brief Inserts a moved value, ignoring the optional hint. */
        iterator insert(const_iterator, value_type&& value)
        {
            return insert(std::move(value)).first;
        }
        /** @brief Inserts an iterator range without replacing duplicate keys. */
        template < std::input_iterator Input >
        void insert(Input first, Input last)
        {
            for (; first != last; ++first)
            {
                emplace(*first);
            }
        }
        /** @brief Inserts an initializer list without replacing duplicate keys. */
        void insert(std::initializer_list< value_type > values)
        {
            insert(values.begin(), values.end());
        }
        /** @brief Erases a key, returning one if found; shrinking can allocate. */
        size_type erase(key_type const& key)
        {
            iterator found = find(key);
            if (found == end())
            {
                return 0;
            }
            erase_rank(found.m_bucket, found.rank());
            shrink_after_erase(1);
            return 1;
        }
        /** @brief Erases the referenced entry without finding a successor; shrinking can allocate. */
        void erase(const_iterator position)
        {
            assert(position.m_owner == this && position != cend());
            erase_rank(position.m_bucket, position.rank());
            shrink_after_erase(1);
        }
        /** @brief Erases the referenced entry through a mutable iterator. */
        void erase(iterator position)
        {
            erase(const_iterator(position));
        }
        /** @brief Erases the original iterator range, then shrinks the bucket directory. */
        iterator erase(const_iterator first, const_iterator last)
        {
            assert(first.m_owner == this && last.m_owner == this);
            value_type* stop = last.m_value;
            hash_type hash = stop ? last.ordering_hash() : 0;
            iterator current(this, first.m_bucket, first.m_slot, first.m_tree, first.m_value);
            if (first == last)
            {
                return current;
            }
            size_type previous_size = m_size;
            while (current.m_value != stop)
            {
                size_type index = current.m_bucket;
                size_type rank = current.rank();
                erase_rank(index, rank);
                current = iterator_at_rank(index, rank);
            }
            shrink_after_erase(previous_size - m_size);
            return stop ? locate(stop->first, hash) : end();
        }
        /** @brief Removes all entries and clears the bucket directory. */
        void clear() noexcept
        {
            for (bucket_storage& bucket : m_buckets)
            {
                for (size_type i = 0; i < std::min(bucket.count, inline_capacity); ++i)
                {
                    destroy_object(bucket.values[i]);
                }
                destroy_tree(bucket.root);
                bucket = bucket_storage{};
            }
            m_size = 0;
            m_buckets.clear();
        }
        /** @brief Returns the number of currently addressable buckets. */
        size_type bucket_count() const noexcept
        {
            return m_buckets.size();
        }
        /** @brief Returns the largest safe geometric bucket directory. */
        size_type max_bucket_count() const noexcept
        {
            return m_buckets.max_size();
        }
        /** @brief Returns the entry count of a valid bucket. */
        size_type bucket_size(size_type index) const noexcept
        {
            assert(index < bucket_count());
            return m_buckets[index].count;
        }
        /** @brief Returns the bucket for key, or zero when no buckets are allocated. */
        size_type bucket(key_type const& key) const
        {
            return m_buckets.empty() ? 0 : bucket_index(static_cast< hash_type >(m_hash(key)));
        }
        /** @brief Returns the fixed target maximum average occupancy of eight. */
        size_type max_load_factor() const noexcept
        {
            return target_occupancy;
        }
        /**
         * @brief Swaps values, storage and policies without moving individual values.
         * @pre Nonpropagating allocators compare equal; policy swaps must not throw.
         */
        void swap(hadix_map& other) noexcept(alloc_traits::propagate_on_container_swap::value || alloc_traits::is_always_equal::value)
        {
            static_assert(std::is_nothrow_swappable_v< Hash > && std::is_nothrow_swappable_v< KeyCompareThreeWay >, "hadix_map swap requires nonthrowing policy swaps");
            if constexpr (!alloc_traits::propagate_on_container_swap::value && !alloc_traits::is_always_equal::value)
            {
                if (m_allocator != other.m_allocator)
                {
                    throw std::invalid_argument("hadix_map swap requires equal nonpropagating allocators");
                }
            }
            using std::swap;
            swap(m_hash, other.m_hash);
            swap(m_compare_three_way, other.m_compare_three_way);
            if constexpr (alloc_traits::propagate_on_container_swap::value)
            {
                swap(m_allocator, other.m_allocator);
            }
            m_buckets.swap(other.m_buckets);
            swap(m_size, other.m_size);
        }
        /** @brief Swaps tables through argument-dependent lookup. */
        friend void swap(hadix_map& left, hadix_map& right) noexcept(noexcept(left.swap(right)))
        {
            left.swap(right);
        }
        /**
         * @brief Compares equivalent keys and equal mapped values, independently of bucket layout.
         * @pre Both maps' policies define the same key equivalence relation.
         */
        friend bool operator==(hadix_map const& left, hadix_map const& right)
        {
            if (left.size() != right.size())
            {
                return false;
            }
            for (value_type const& value : left)
            {
                const_iterator found = right.find(value.first);
                if (found == right.end() || !(found->second == value.second))
                {
                    return false;
                }
            }
            return true;
        }
    };
} // namespace rpnx
#endif
