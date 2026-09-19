// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0
#ifndef RPNX_INLINE_HADIX_MAP_HPP
#define RPNX_INLINE_HADIX_MAP_HPP

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
#include <optional>
#include <rpnx/geometric_array.hpp>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

namespace rpnx
{
    /**
     * @brief Hash radix map storing values directly in buckets and AVL overflow nodes.
     *
     * Each bucket keeps its smallest 8 entries in a sorted inline prefix. A
     * 8-byte fingerprint group filters lookups before full hashes and keys are
     * accessed. The remaining entries form an AVL tree ordered by reversed hash,
     * then KeyCompareThreeWay. Equivalent keys must hash equally; comparison must
     * never return unordered. Hash results have exactly std::size_t value bits.
     *
     * Single-element insertion, lookup and erasure perform O(log(size()+1))
     * worst-case structural work, expected O(1) for well-distributed hashes.
     * One insertion splits at most one bucket; one erasure joins at most one pair.
     * Allocation, user operations and a bounded number of value relocations are
     * excluded from this structural bound. There is no whole-table rehash.
     *
     * Key and T must be nothrow move constructible and nothrow destructible.
     * Internal pairs have movable keys; iterator reference proxies expose const
     * keys and mutable mapped values. Dereference returns a proxy by value, so
     * range loops should use auto&& or auto. Pointers returned by operator-> are
     * temporary proxies valid for that full expression.
     *
     * Insertion and erasure may invalidate all iterators, pointers and references,
     * including on failure after a structural change. Failed insertion adds no
     * entry. A failed shrinking allocation leaves the erased entry removed and
     * excess buckets for later erasures.
     * Iteration follows reversed-hash/key order and costs O(size()+bucket_count()).
     * Reservation, manual rehashing and load-policy setters are not provided.
     *
     * @tparam Key Immutable public key, movable internally.
     * @tparam T Mapped value stored alongside its key.
     * @tparam Hash Unsigned, size-width hash policy.
     * @tparam KeyCompareThreeWay Ordering and equivalence policy for equal hashes.
     * @tparam Allocator Allocator rebound for buckets and overflow nodes.
     */
    template < typename Key, typename T, typename Hash = std::hash< Key >, typename KeyCompareThreeWay = std::compare_three_way, typename Allocator = std::allocator< std::pair< Key const, T > > >
    class inline_hadix_map
    {
      public:
        using key_type = Key;
        using mapped_type = T;
        using value_type = std::pair< Key const, T >;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using hasher = Hash;
        using key_compare_three_way = KeyCompareThreeWay;
        using allocator_type = Allocator;

        /** @brief Reference pair exposing an immutable key and mutable mapped value. */
        using reference = std::pair< Key const&, T& >;
        /** @brief Reference pair exposing an immutable entry. */
        using const_reference = std::pair< Key const&, T const& >;

      private:
        using stored_type = std::pair< Key, T >;
        using alloc_traits = std::allocator_traits< Allocator >;
        using hash_type = std::size_t;
        using comparison_category = std::common_comparison_category_t< std::strong_ordering, std::invoke_result_t< KeyCompareThreeWay const&, Key const&, Key const& > >;
        using hash_result = std::invoke_result_t< Hash const&, Key const& >;
        static constexpr size_type hash_bits = std::numeric_limits< hash_type >::digits;
        static constexpr size_type inline_capacity = 8;
        static constexpr size_type target_occupancy = 4;
        static_assert(std::is_nothrow_move_constructible_v< Key > && std::is_nothrow_move_constructible_v< T >, "inline_hadix_map requires nothrow movable keys and mapped values");
        static_assert(std::is_nothrow_destructible_v< Key > && std::is_nothrow_destructible_v< T >);
        static_assert(std::is_same_v< value_type, typename alloc_traits::value_type >);
        static_assert(!std::is_void_v< comparison_category >);
        static_assert(std::is_integral_v< hash_result > && std::is_unsigned_v< hash_result > && std::numeric_limits< hash_result >::digits == hash_bits);
        /** @brief AVL node owning its value; rotations and splits only change links. */
        struct alignas(std::max(4 * sizeof(void*), alignof(stored_type))) tree_node
        {
            hash_type hash;
            tree_node* left = nullptr;
            tree_node* right = nullptr;
            std::optional< stored_type > value;
            tree_node* parent = nullptr;
            size_type count = 1;
            std::int32_t height = 1;

            /** @brief Creates an empty detached node for staged relocation. */
            explicit tree_node(hash_type ordering_hash) noexcept : hash(ordering_hash)
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
        };

        /** @brief Uninitialized value storage whose lifetime is tracked by its bucket count. */
        union value_slot
        {
            std::byte empty;
            stored_type value;

            /** @brief Initializes a slot without constructing a key or mapped value. */
            value_slot() noexcept : empty{}
            {
            }
            /** @brief Leaves active-value destruction to the owning bucket. */
            ~value_slot() noexcept
            {
            }
        };
        /**
         * @brief Sorted inline values and metadata for one hash range.
         * Hashes, fingerprint bytes and inline values form contiguous blocks.
         * Exactly min(count, inline_capacity) value slots are live outside a
         * nonthrowing relocation sequence.
         */
        struct alignas(std::max(size_type{64}, alignof(stored_type))) bucket_storage
        {
            std::array< hash_type, inline_capacity > hashes{};
            alignas(8) std::array< std::uint8_t, inline_capacity > controls;

            std::array< value_slot, inline_capacity > values;
            size_type count = 0;
            tree_node* root = nullptr;

            /** @brief Initializes all fingerprint slots as empty. */
            bucket_storage() noexcept
            {
                controls.fill(0x80);
            }
            /** @brief Transfers a bucket's live values and overflow ownership. */
            bucket_storage(bucket_storage&& other) noexcept : hashes(other.hashes), controls(other.controls), count(std::exchange(other.count, 0)), root(std::exchange(other.root, nullptr))
            {
                for (size_type i = 0; i < std::min(count, inline_capacity); ++i)
                {
                    std::construct_at(&values[i].value, std::move(other.values[i].value));
                    std::destroy_at(&other.values[i].value);
                }
                other.controls.fill(0x80);
            }
            /** @brief Destroys the live inline prefix; the map releases the overflow tree. */
            ~bucket_storage() noexcept
            {
                for (size_type i = 0; i < std::min(count, inline_capacity); ++i)
                {
                    std::destroy_at(&values[i].value);
                }
            }
        };
        using bucket_allocator = typename alloc_traits::template rebind_alloc< bucket_storage >;
        using bucket_array = geometric_array< bucket_storage, bucket_allocator >;
        [[no_unique_address]] Allocator m_allocator;
        [[no_unique_address]] Hash m_hash;
        [[no_unique_address]] KeyCompareThreeWay m_compare_three_way;
        bucket_array m_buckets;
        size_type m_size = 0;

        /** @brief Location and insertion rank obtained without modifying storage. */
        struct search_result
        {
            size_type rank;
            stored_type const* value;
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
        /** @brief Releases an overflow tree and all of its inline values. */
        void destroy_tree(tree_node* root) noexcept
        {
            if (!root)
            {
                return;
            }
            destroy_tree(root->left);
            destroy_tree(root->right);
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
        comparison_category compare_entry(hash_type hash, key_type const& key, hash_type stored_hash, stored_type const& stored) const
        {
            if (hash != stored_hash)
            {
                return hash <=> stored_hash;
            }
            return m_compare_three_way(key, stored.first);
        }
        /** @brief Uses bits opposite the low indexing bits for an eight-bit fingerprint. */
        static std::uint8_t fingerprint(hash_type hash) noexcept
        {
            return static_cast< std::uint8_t >(hash & 0xff);
        }
        /** @brief Tests the full fingerprint group with an independent byte comparison reduction. */
        static bool contains_fingerprint(bucket_storage const& bucket, hash_type hash) noexcept
        {
            bool matches = false;
            for (size_type i = 0; i < inline_capacity; ++i)
            {
                matches |= bucket.controls[i] == fingerprint(hash);
            }
            return matches;
        }
        /**
         * @brief Packs matching byte positions into a candidate mask.
         * The seven-bit additions cannot carry between bytes. Their high bits,
         * combined with the original differences, identify exact zero bytes.
         * Multiplication gathers the eight independent bits in each word.
         */
        static std::uint32_t matching_slots(bucket_storage const& bucket, hash_type hash) noexcept
        {
            std::array< std::uint64_t, inline_capacity / sizeof(std::uint64_t) > words = std::bit_cast< std::array< std::uint64_t, inline_capacity / sizeof(std::uint64_t) > >(bucket.controls);
            std::uint64_t repeated = UINT64_C(0x0101010101010101) * fingerprint(hash);
            for (size_type i = 0; i < words.size(); ++i)
            {
                std::uint64_t difference = words[i] ^ repeated;
                words[i] = ~(((difference & UINT64_C(0x7f7f7f7f7f7f7f7f)) + UINT64_C(0x7f7f7f7f7f7f7f7f)) | difference | UINT64_C(0x7f7f7f7f7f7f7f7f));
                if constexpr (std::endian::native == std::endian::big)
                {
                    words[i] = std::byteswap(words[i]);
                }
            }
            std::uint32_t matches = 0;
            for (size_type i = 0; i < words.size(); ++i)
            {
                std::uint64_t byte = ((words[i] >> 7) * UINT64_C(0x0102040810204080)) >> 56;
                matches |= static_cast< std::uint32_t >(byte) << (i * 8);
            }
            return matches;
        }
        /** @brief Searches the prefix and tree, returning a rank for insertion or erasure. */
        template < bool ComputeRank >
        search_result search(bucket_storage const& bucket, hash_type hash, Key const& key) const
        {
            size_type occupied = std::min(bucket.count, inline_capacity);
            if (contains_fingerprint(bucket, hash))
            {
                std::uint32_t matches = matching_slots(bucket, hash) & ((std::uint32_t{1} << occupied) - 1);
                while (matches != 0)
                {
                    size_type slot = std::countr_zero(matches);
                    matches &= matches - 1;
                    if (bucket.hashes[slot] == hash && m_compare_three_way(key, bucket.values[slot].value.first) == 0)
                    {
                        return {slot, &bucket.values[slot].value, nullptr};
                    }
                }
            }
            if constexpr (ComputeRank)
            {
                size_type first = 0;
                size_type last = occupied;
                while (first < last)
                {
                    size_type middle = first + (last - first) / 2;
                    if (compare_entry(hash, key, bucket.hashes[middle], bucket.values[middle].value) < 0)
                    {
                        last = middle;
                    }
                    else
                    {
                        first = middle + 1;
                    }
                }
                if (first < inline_capacity)
                {
                    return {first, nullptr, nullptr};
                }
            }
            size_type rank = inline_capacity;
            tree_node* node = bucket.root;
            while (node)
            {
                comparison_category comparison = compare_entry(hash, key, node->hash, *node->value);
                if (comparison == 0)
                {
                    return {rank + tree_node::size_of(node->left), &*node->value, node};
                }
                if (comparison < 0)
                {
                    node = node->left;
                }
                else
                {
                    rank += 1 + tree_node::size_of(node->left);
                    node = node->right;
                }
            }
            return {rank, nullptr, nullptr};
        }
        /** @brief Relocates one prefix entry into an empty destination slot. */
        static void relocate_slot(bucket_storage& destination, size_type to, bucket_storage& source, size_type from) noexcept
        {
            assert(destination.controls[to] == 0x80);
            std::construct_at(&destination.values[to].value, std::move(source.values[from].value));
            destination.hashes[to] = source.hashes[from];
            destination.controls[to] = source.controls[from];
            std::destroy_at(&source.values[from].value);
            source.controls[from] = 0x80;
        }
        /** @brief Promotes the smallest overflow entries into empty prefix slots. */
        void fill_inline(bucket_storage& bucket, size_type occupied) noexcept
        {
            size_type target = std::min(bucket.count, inline_capacity);
            while (occupied < target)
            {
                tree_node* extracted = nullptr;
                bucket.root = tree_node::extract_minimum(bucket.root, extracted);
                std::construct_at(&bucket.values[occupied].value, std::move(*extracted->value));
                bucket.hashes[occupied] = extracted->hash;
                bucket.controls[occupied] = fingerprint(extracted->hash);
                destroy_object(extracted);
                ++occupied;
            }
        }
        /** @brief Splits one hash range after allocating its new bucket. */
        void split_bucket()
        {
            size_type index = m_buckets.size();
            m_buckets.emplace_back();
            if (index == 0)
            {
                return;
            }
            size_type partner = index - std::bit_floor(index);
            hash_type boundary = reverse_bits(index);
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
                relocate_slot(right, i - cut, left, i);
            }
            std::pair< tree_node*, tree_node* > parts = tree_node::split(left.root, boundary);
            left.root = parts.first;
            right.root = parts.second;
            left.count = cut + tree_node::size_of(left.root);
            right.count = occupied - cut + tree_node::size_of(right.root);
            fill_inline(left, cut);
            fill_inline(right, occupied - cut);
        }
        /** @brief Joins two ranges, staging every allocation before relocating values. */
        void join_bucket()
        {
            size_type index = m_buckets.size() - 1;
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
                    staged[allocated] = allocate_object< tree_node >(right.hashes[i]);
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
            for (size_type i = retained; i < right_inline; ++i)
            {
                staged[i - retained]->value.emplace(std::move(right.values[i].value));
                std::destroy_at(&right.values[i].value);
            }
            for (size_type i = 0; i < retained; ++i)
            {
                relocate_slot(left, left_inline + i, right, i);
            }
            tree_node* suffix = tree_node::concatenate(tree_node::from_sorted(staged.data(), allocated), right.root);
            left.root = tree_node::concatenate(left.root, suffix);
            left.count += right.count;
            fill_inline(left, left_inline + retained);
            right.root = nullptr;
            right.count = 0;
            m_buckets.pop_back();
        }
        /** @brief Computes the occupancy target with two spare buckets of shrink hysteresis. */
        static size_type required_buckets(size_type count) noexcept
        {
            return std::max(size_type{1}, count / target_occupancy + (count % target_occupancy != 0));
        }
        /** @brief Performs at most one scheduled split for an insertion. */
        void grow_for_insert()
        {
            if (m_size == max_size())
            {
                throw std::length_error("inline_hadix_map size exceeds max_size");
            }
            if (m_buckets.size() < required_buckets(m_size + 1))
            {
                split_bucket();
            }
        }
        /** @brief Performs at most one scheduled join after an erasure. */
        void shrink_after_erase()
        {
            size_type target = required_buckets(m_size);
            if (m_buckets.size() > target && m_buckets.size() - target > 2)
            {
                join_bucket();
            }
        }

      public:
        /** @brief Iterator returning immutable-key proxies in reversed-hash/key order. */
        template < bool Const >
        class iterator_impl
        {
            friend class inline_hadix_map;
            template < bool >
            friend class iterator_impl;
            using owner_type = std::conditional_t< Const, inline_hadix_map const, inline_hadix_map >;
            owner_type* m_owner = nullptr;
            size_type m_bucket = 0;
            size_type m_slot = 0;
            tree_node* m_tree = nullptr;

            /** @brief Constructs an already located iterator. */
            iterator_impl(owner_type* owner, size_type bucket, size_type slot = 0, tree_node* tree = nullptr) noexcept : m_owner(owner), m_bucket(bucket), m_slot(slot), m_tree(tree)
            {
            }
            /** @brief Skips empty buckets in reversed-hash order. */
            void seek_bucket() noexcept
            {
                while (m_bucket < m_owner->bucket_count() && m_owner->m_buckets[m_bucket].count == 0)
                {
                    m_bucket = m_owner->next_bucket(m_bucket);
                }
            }

          public:
            using value_type = typename inline_hadix_map::value_type;
            using difference_type = std::ptrdiff_t;
            using reference = std::conditional_t< Const, typename inline_hadix_map::const_reference, typename inline_hadix_map::reference >;
            using iterator_category = std::input_iterator_tag;
            using iterator_concept = std::forward_iterator_tag;
            /** @brief Owns a temporary reference proxy for arrow access. */
            struct pointer
            {
                reference entry;
                /** @brief Returns the pair-like proxy for this full expression. */
                reference const* operator->() const noexcept
                {
                    return &entry;
                }
            };
            /** @brief Constructs a singular iterator. */
            iterator_impl() noexcept = default;
            /** @brief Converts a mutable iterator to an immutable iterator. */
            template < bool Other >
                requires(Const && !Other)
            iterator_impl(iterator_impl< Other > const& other) noexcept : m_owner(other.m_owner), m_bucket(other.m_bucket), m_slot(other.m_slot), m_tree(other.m_tree)
            {
            }
            /** @brief Returns references to the located key and mapped value. */
            reference operator*() const noexcept
            {
                assert(m_owner && m_bucket < m_owner->bucket_count());
                if (m_tree)
                {
                    return {m_tree->value->first, m_tree->value->second};
                }
                return {m_owner->m_buckets[m_bucket].values[m_slot].value.first, m_owner->m_buckets[m_bucket].values[m_slot].value.second};
            }
            /** @brief Exposes pair member access through a temporary proxy. */
            pointer operator->() const noexcept
            {
                return {**this};
            }
            /** @brief Advances within the prefix, overflow tree, then subsequent ranges. */
            iterator_impl& operator++() noexcept
            {
                if (m_tree)
                {
                    m_tree = tree_node::successor(m_tree);
                }
                else
                {
                    ++m_slot;
                    if (m_slot < std::min(m_owner->m_buckets[m_bucket].count, inline_capacity))
                    {
                        return *this;
                    }
                    m_tree = tree_node::minimum(m_owner->m_buckets[m_bucket].root);
                }
                if (!m_tree)
                {
                    m_slot = 0;
                    m_bucket = m_owner->next_bucket(m_bucket);
                    seek_bucket();
                }
                return *this;
            }
            /** @brief Advances and returns the previous position. */
            iterator_impl operator++(std::int32_t) noexcept
            {
                iterator_impl previous = *this;
                ++*this;
                return previous;
            }
            /** @brief Compares positions belonging to the same map. */
            template < bool Other >
            bool operator==(iterator_impl< Other > const& other) const noexcept
            {
                return m_owner == other.m_owner && m_bucket == other.m_bucket && m_tree == other.m_tree && (m_tree || m_slot == other.m_slot);
            }
        };
        using iterator = iterator_impl< false >;
        using const_iterator = iterator_impl< true >;

      private:
        /** @brief Commits an absent, fully constructed automatic-storage candidate. */
        std::pair< iterator, bool > insert_absent(stored_type& value, hash_type hash)
        {
            grow_for_insert();
            size_type index = bucket_index(reverse_bits(hash));
            bucket_storage& bucket = m_buckets[index];
            search_result position = search< true >(bucket, hash, value.first);
            size_type occupied = std::min(bucket.count, inline_capacity);
            tree_node* inserted = nullptr;
            if (occupied == inline_capacity)
            {
                if (position.rank < inline_capacity)
                {
                    tree_node* displaced = allocate_object< tree_node >(bucket.hashes.back());
                    displaced->value.emplace(std::move(bucket.values.back().value));
                    std::destroy_at(&bucket.values.back().value);
                    bucket.controls.back() = 0x80;
                    bucket.root = tree_node::insert(bucket.root, displaced, 0);
                }
                else
                {
                    inserted = allocate_object< tree_node >(hash);
                    inserted->value.emplace(std::move(value));
                    bucket.root = tree_node::insert(bucket.root, inserted, position.rank - inline_capacity);
                }
            }
            if (position.rank < inline_capacity)
            {
                size_type last = std::min(occupied, inline_capacity - 1);
                for (size_type i = last; i > position.rank; --i)
                {
                    relocate_slot(bucket, i, bucket, i - 1);
                }
                std::construct_at(&bucket.values[position.rank].value, std::move(value));
                bucket.hashes[position.rank] = hash;
                bucket.controls[position.rank] = fingerprint(hash);
            }
            ++bucket.count;
            ++m_size;
            return {iterator(this, index, std::min(position.rank, inline_capacity), inserted), true};
        }
        /** @brief Removes a located entry without hashing or comparing keys. */
        void erase_position(size_type index, size_type slot, tree_node* tree) noexcept
        {
            bucket_storage& bucket = m_buckets[index];
            if (tree)
            {
                tree_node* removed = nullptr;
                bucket.root = tree_node::erase(bucket.root, tree->rank(), removed);
                destroy_object(removed);
                --bucket.count;
            }
            else
            {
                size_type occupied = std::min(bucket.count, inline_capacity);
                std::destroy_at(&bucket.values[slot].value);
                bucket.controls[slot] = 0x80;
                for (size_type i = slot + 1; i < occupied; ++i)
                {
                    relocate_slot(bucket, i - 1, bucket, i);
                }
                --bucket.count;
                fill_inline(bucket, occupied - 1);
            }
            --m_size;
        }

      public:
        /** @brief Constructs an empty map with the specified policies. */
        explicit inline_hadix_map(Hash const& hash = Hash(), KeyCompareThreeWay const& compare = KeyCompareThreeWay(), Allocator const& allocator = Allocator()) : m_allocator(allocator), m_hash(hash), m_compare_three_way(compare), m_buckets(bucket_allocator(allocator))
        {
        }
        /** @brief Constructs an empty map using a specified allocator. */
        explicit inline_hadix_map(Allocator const& allocator) : inline_hadix_map(Hash(), KeyCompareThreeWay(), allocator)
        {
        }
        /** @brief Copies entries from an initializer list. */
        inline_hadix_map(std::initializer_list< value_type > values, Hash const& hash = Hash(), KeyCompareThreeWay const& compare = KeyCompareThreeWay(), Allocator const& allocator = Allocator()) : inline_hadix_map(hash, compare, allocator)
        {
            try
            {
                for (value_type const& value : values)
                {
                    insert(value);
                }
            }
            catch (...)
            {
                clear();
                throw;
            }
        }
        /** @brief Copies entries with the allocator selected by allocator traits. */
        inline_hadix_map(inline_hadix_map const& other)
            requires(std::is_copy_constructible_v< Key > && std::is_copy_constructible_v< T >)
            : inline_hadix_map(other, alloc_traits::select_on_container_copy_construction(other.m_allocator))
        {
        }
        /** @brief Copies entries using the specified allocator. */
        inline_hadix_map(inline_hadix_map const& other, Allocator const& allocator)
            requires(std::is_copy_constructible_v< Key > && std::is_copy_constructible_v< T >)
            : inline_hadix_map(other.m_hash, other.m_compare_three_way, allocator)
        {
            try
            {
                for (const_reference value : other)
                {
                    try_emplace(value.first, value.second);
                }
            }
            catch (...)
            {
                clear();
                throw;
            }
        }
        /** @brief Transfers the directory and policies, leaving the source empty. */
        inline_hadix_map(inline_hadix_map&& other) noexcept(std::is_nothrow_move_constructible_v< Allocator > && std::is_nothrow_move_constructible_v< Hash > && std::is_nothrow_move_constructible_v< KeyCompareThreeWay >) : m_allocator(std::move(other.m_allocator)), m_hash(std::move(other.m_hash)), m_compare_three_way(std::move(other.m_compare_three_way)), m_buckets(std::move(other.m_buckets)), m_size(std::exchange(other.m_size, 0))
        {
        }
        /** @brief Destroys every entry and releases all directory segments. */
        ~inline_hadix_map()
        {
            clear();
        }
        /** @brief Copies entries, honoring allocator propagation. */
        inline_hadix_map& operator=(inline_hadix_map const& other)
            requires(std::is_copy_constructible_v< Key > && std::is_copy_constructible_v< T >)
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
            inline_hadix_map replacement(other, allocator);
            clear();
            if constexpr (alloc_traits::propagate_on_container_copy_assignment::value)
            {
                m_allocator = allocator;
            }
            m_hash = other.m_hash;
            m_compare_three_way = other.m_compare_three_way;
            std::destroy_at(&m_buckets);
            std::construct_at(&m_buckets, std::move(replacement.m_buckets));
            m_size = std::exchange(replacement.m_size, 0);
            return *this;
        }
        /** @brief Transfers storage when allocators permit, otherwise relocates entries. */
        inline_hadix_map& operator=(inline_hadix_map&& other)
        {
            if (this == &other)
            {
                return *this;
            }
            clear();
            m_hash = other.m_hash;
            m_compare_three_way = other.m_compare_three_way;
            if constexpr (alloc_traits::propagate_on_container_move_assignment::value)
            {
                m_allocator = std::move(other.m_allocator);
                m_buckets = std::move(other.m_buckets);
            }
            else
            {
                if (m_allocator != other.m_allocator)
                {
                    // Erase each source entry before attempting a potentially throwing destination insertion.
                    while (!other.empty())
                    {
                        iterator first = other.begin();
                        stored_type& source = first.m_tree ? *first.m_tree->value : other.m_buckets[first.m_bucket].values[first.m_slot].value;
                        stored_type value(std::move(source));
                        other.erase_position(first.m_bucket, first.m_slot, first.m_tree);
                        emplace(std::move(value));
                    }
                    return *this;
                }
                m_buckets = std::move(other.m_buckets);
            }
            m_size = std::exchange(other.m_size, 0);
            return *this;
        }
        /** @brief Returns the number of entries. */
        size_type size() const noexcept
        {
            return m_size;
        }
        /** @brief Reports whether the map contains no entries. */
        bool empty() const noexcept
        {
            return m_size == 0;
        }
        /** @brief Returns the allocator's maximum supported element count. */
        size_type max_size() const noexcept
        {
            return std::min(alloc_traits::max_size(m_allocator), std::numeric_limits< size_type >::max() / 2);
        }
        /** @brief Returns the number of addressable radix buckets. */
        size_type bucket_count() const noexcept
        {
            return m_buckets.size();
        }
        /** @brief Returns the entry count in a valid bucket. */
        size_type bucket_size(size_type index) const noexcept
        {
            assert(index < bucket_count());
            return m_buckets[index].count;
        }
        /** @brief Returns a key's bucket index, or zero for an unallocated directory. */
        size_type bucket(Key const& key) const
        {
            return m_buckets.empty() ? 0 : bucket_index(static_cast< hash_type >(m_hash(key)));
        }
        /** @brief Returns the allocator policy. */
        Allocator get_allocator() const
        {
            return m_allocator;
        }
        /** @brief Returns the hash policy. */
        Hash hash_function() const
        {
            return m_hash;
        }
        /** @brief Returns the key ordering and equivalence policy. */
        KeyCompareThreeWay key_comp_three_way() const
        {
            return m_compare_three_way;
        }
        /** @brief Returns the first entry in reversed-hash/key order. */
        iterator begin() noexcept
        {
            iterator result(this, 0);
            result.seek_bucket();
            return result;
        }
        /** @brief Returns the first immutable entry in reversed-hash/key order. */
        const_iterator begin() const noexcept
        {
            const_iterator result(this, 0);
            result.seek_bucket();
            return result;
        }
        /** @brief Returns the immutable beginning. */
        const_iterator cbegin() const noexcept
        {
            return begin();
        }
        /** @brief Returns the past-the-end position. */
        iterator end() noexcept
        {
            return iterator(this, bucket_count());
        }
        /** @brief Returns the immutable past-the-end position. */
        const_iterator end() const noexcept
        {
            return const_iterator(this, bucket_count());
        }
        /** @brief Returns the immutable end. */
        const_iterator cend() const noexcept
        {
            return end();
        }
        /** @brief Locates a key without modifying entries. */
        const_iterator find(Key const& key) const
        {
            if (empty())
            {
                return end();
            }
            hash_type hash = static_cast< hash_type >(m_hash(key));
            size_type index = bucket_index(hash);
            search_result found = search< false >(m_buckets[index], reverse_bits(hash), key);
            return found.value ? const_iterator(this, index, std::min(found.rank, inline_capacity), found.tree) : end();
        }
        /** @brief Locates a mutable mapped value. */
        iterator find(Key const& key)
        {
            const_iterator found = std::as_const(*this).find(key);
            return iterator(this, found.m_bucket, found.m_slot, found.m_tree);
        }
        /** @brief Reports whether an equivalent key exists. */
        bool contains(Key const& key) const
        {
            return find(key) != end();
        }
        /** @brief Returns one for a present key and zero otherwise. */
        size_type count(Key const& key) const
        {
            return contains(key) ? 1 : 0;
        }
        /** @brief Returns a mapped value or throws std::out_of_range. */
        T& at(Key const& key)
        {
            iterator found = find(key);
            if (found == end())
            {
                throw std::out_of_range("inline_hadix_map::at");
            }
            return found->second;
        }
        /** @brief Returns an immutable mapped value or throws std::out_of_range. */
        T const& at(Key const& key) const
        {
            const_iterator found = find(key);
            if (found == end())
            {
                throw std::out_of_range("inline_hadix_map::at");
            }
            return found->second;
        }
        /** @brief Inserts a default mapped value for a missing copied key. */
        T& operator[](Key const& key)
        {
            return try_emplace(key).first->second;
        }
        /** @brief Inserts a default mapped value for a missing moved key. */
        T& operator[](Key&& key)
        {
            return try_emplace(std::move(key)).first->second;
        }
        /** @brief Constructs a candidate pair and inserts it unless its key exists. */
        template < typename... Args >
        std::pair< iterator, bool > emplace(Args&&... args)
        {
            stored_type value(std::forward< Args >(args)...);
            hash_type hash = reverse_bits(static_cast< hash_type >(m_hash(value.first)));
            if (!empty())
            {
                size_type index = bucket_index(reverse_bits(hash));
                search_result found = search< false >(m_buckets[index], hash, value.first);
                if (found.value)
                {
                    return {iterator(this, index, std::min(found.rank, inline_capacity), found.tree), false};
                }
            }
            return insert_absent(value, hash);
        }
        /** @brief Inserts a copied conventional map value. */
        std::pair< iterator, bool > insert(value_type const& value)
        {
            return emplace(value.first, value.second);
        }
        /** @brief Inserts a moved conventional map value, copying its immutable key. */
        std::pair< iterator, bool > insert(value_type&& value)
        {
            return emplace(value.first, std::move(value.second));
        }
        /** @brief Inserts a pair-like value with a movable key. */
        template < typename Pair >
            requires std::is_constructible_v< stored_type, Pair&& >
        std::pair< iterator, bool > insert(Pair&& value)
        {
            return emplace(std::forward< Pair >(value));
        }
        /** @brief Constructs a mapped value only for a missing key; consumes arguments before structural changes. */
        template < typename K, typename... Args >
            requires std::is_same_v< std::remove_cvref_t< K >, Key >
        std::pair< iterator, bool > try_emplace(K&& key, Args&&... args)
        {
            hash_type hash = reverse_bits(static_cast< hash_type >(m_hash(key)));
            if (!empty())
            {
                size_type index = bucket_index(reverse_bits(hash));
                search_result found = search< false >(m_buckets[index], hash, key);
                if (found.value)
                {
                    return {iterator(this, index, std::min(found.rank, inline_capacity), found.tree), false};
                }
            }
            stored_type value(std::piecewise_construct, std::forward_as_tuple(std::forward< K >(key)), std::forward_as_tuple(std::forward< Args >(args)...));
            return insert_absent(value, hash);
        }
        /** @brief Accepts convertible keys using the conventional const-key overload. */
        template < typename... Args >
        std::pair< iterator, bool > try_emplace(Key const& key, Args&&... args)
        {
            hash_type hash = reverse_bits(static_cast< hash_type >(m_hash(key)));
            if (!empty())
            {
                size_type index = bucket_index(reverse_bits(hash));
                search_result found = search< false >(m_buckets[index], hash, key);
                if (found.value)
                {
                    return {iterator(this, index, std::min(found.rank, inline_capacity), found.tree), false};
                }
            }
            stored_type value(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(std::forward< Args >(args)...));
            return insert_absent(value, hash);
        }
        /** @brief Assigns an existing mapped value or constructs a new entry. */
        template < typename K, typename M >
        std::pair< iterator, bool > insert_or_assign(K&& key, M&& mapped)
        {
            iterator found = find(key);
            if (found != end())
            {
                found->second = std::forward< M >(mapped);
                return {found, false};
            }
            return try_emplace(std::forward< K >(key), std::forward< M >(mapped));
        }
        /** @brief Removes an equivalent key and performs at most one shrinking join. */
        size_type erase(Key const& key)
        {
            iterator found = find(key);
            if (found == end())
            {
                return 0;
            }
            erase(found);
            return 1;
        }
        /** @brief Erases a located entry; all other iterators may be invalidated. */
        void erase(const_iterator position)
        {
            assert(position.m_owner == this && position.m_bucket < bucket_count());
            erase_position(position.m_bucket, position.m_slot, position.m_tree);
            shrink_after_erase();
        }
        /** @brief Destroys every value and releases directory storage. */
        void clear() noexcept
        {
            for (bucket_storage& bucket : m_buckets)
            {
                destroy_tree(bucket.root);
                bucket.root = nullptr;
            }
            m_buckets.clear();
            m_size = 0;
        }
    };
} // namespace rpnx
#endif
