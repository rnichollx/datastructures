// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0
#ifndef RPNX_CONCURRENT_HASH_TRIE_HPP
#define RPNX_CONCURRENT_HASH_TRIE_HPP

#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <rpnx/thin_shared_mutex.hpp>
#include <shared_mutex>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace rpnx
{
    /**
     * @brief Concurrent adaptive radix map indexed by 64-bit hashes with stable entry addresses.
     *
     * Hash returns an unsigned integer with at most 64 value bits. Narrow results
     * are zero-extended. KeyCompareThreeWay defines ordering and equivalence for
     * keys with identical hashes; equivalent keys must hash equally and comparison
     * must never return unordered. Hash and comparison are invoked through const
     * references. Hash, comparison and allocator state must support concurrent use
     * by different threads.
     *
     * Node links own compact shared mutexes. Operations retain shared ancestor
     * locks while accessing descendants. Child publication uses atomic slots;
     * replacement and reclamation acquire the owning link exclusively. Exact-hash
     * collisions share a locked ordered map. Structural key operations have
     * O(8 + log(c + 1)) work for c keys with the same hash, excluding allocation,
     * user operations and lock contention.
     *
     * InsertOnly retains Node256 layouts until destruction and publishes an
     * atomic view for traversal without shared locking at those nodes. Prefix
     * splitting reparents the existing layout. Erasure and clearing are disabled
     * in this mode; smaller layouts and collision leaves retain their locks.
     *
     * Insertion and routing changes preserve retained entry addresses. Returned
     * pointers and references release their locks before returning: callers must
     * coordinate their use with erasure, assignment, clearing and destruction.
     * with_read() and with_write() protect an entry throughout their callbacks.
     * Callbacks must not reenter this trie. Structural changes can invalidate all
     * iterators, including when an operation throws. Iteration requires external
     * exclusion of concurrent mutation; its order is unspecified.
     *
     * Operations provide basic exception safety. A failed insertion adds no entry
     * but may leave empty routing storage. A shrinking allocation can fail after
     * erasure commits; later erasures can reclaim the retained routing storage.
     * Destruction requires quiescent access to the trie and its entries.
     *
     * @tparam K Immutable key type.
     * @tparam V Mapped value type, constructed in stable ordered-map storage.
     * @tparam Hash Hash policy, defaulting to std::hash<K>.
     * @tparam KeyCompareThreeWay Three-way ordering and equivalence policy.
     * @tparam Allocator Allocator rebound for routing and collision storage.
     * @tparam InsertOnly Disable erasure and use permanent Node256 layouts for traversal without shared locks.
     */
    template < typename K, typename V, typename Hash = std::hash< K >, typename KeyCompareThreeWay = std::compare_three_way, typename Allocator = std::allocator< std::pair< K const, V > >, bool InsertOnly = false >
    class basic_concurrent_hash_trie
    {
      public:
        using key_type = K;
        using mapped_type = V;
        using value_type = std::pair< K const, V >;
        using hasher = Hash;
        using key_compare_three_way = KeyCompareThreeWay;
        using allocator_type = Allocator;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using const_pointer = value_type const*;
        using reference = value_type&;
        using const_reference = value_type const&;

      private:
        using hash_type = std::uint64_t;
        using hash_result = std::invoke_result_t< Hash const&, K const& >;
        using comparison_category = std::common_comparison_category_t< std::strong_ordering, std::invoke_result_t< KeyCompareThreeWay const&, K const&, K const& > >;
        static_assert(std::is_integral_v< hash_result > && std::is_unsigned_v< hash_result > && std::numeric_limits< hash_result >::digits <= 64);
        static_assert(!std::is_void_v< comparison_category >);
        static_assert(std::is_same_v< typename std::allocator_traits< Allocator >::value_type, value_type >);

        struct node_link;

        /** @brief Adapts the owning trie's three-way policy to strict weak ordering. */
        struct collision_compare
        {
            KeyCompareThreeWay const* compare;

            /** @brief Reports whether left precedes right under the shared comparison policy. */
            bool operator()(K const& left, K const& right) const
            { return std::invoke(*compare, left, right) < 0; }
        };

        /** @brief Stable entry storage for one complete hash value. */
        struct leaf_node
        {
            hash_type hash;
            std::map< K, V, collision_compare, Allocator > entries;

            /** @brief Constructs an empty collision group using the owning policies. */
            leaf_node(hash_type input, KeyCompareThreeWay const* compare, Allocator const& allocator) : hash(input), entries(collision_compare{compare}, allocator)
            {
            }
        };

        /** @brief Compressed prefix and child slots for a fixed adaptive capacity. */
        template < size_type Capacity >
        struct branch_node
        {
            static constexpr size_type capacity = Capacity;
            hash_type prefix_hash;
            std::uint8_t depth;
            std::array< std::atomic< node_link* >, Capacity > children{};

            /** @brief Constructs an empty branch at an absolute hash byte position. */
            branch_node(hash_type prefix, std::uint8_t position) noexcept : prefix_hash(prefix), depth(position)
            {
            }
        };

        /** @brief Forty-eight child slots with a byte-to-slot index; zero denotes absence. */
        struct node48 : branch_node< 48 >
        {
            std::array< std::atomic< std::uint8_t >, 256 > byte_to_slot{};

            /** @brief Constructs an empty indexed branch at a hash byte position. */
            node48(hash_type prefix, std::uint8_t position) noexcept : branch_node< 48 >(prefix, position)
            {
            }
        };

        /** @brief Tagged ownership of either a collision group or an adaptive branch. */
        using node_payload = std::variant< std::monostate, leaf_node*, branch_node< 4 >*, branch_node< 16 >*, node48*, branch_node< 256 >* >;

        /** @brief Stable synchronization owner for a replaceable node payload. */
        struct node_link
        {
            mutable thin_shared_mutex mutex;
            std::uint8_t const incoming_byte;
            node_payload payload;
            /** @brief Publish a permanent Node256 view whose lifetime survives prefix splitting. */
            [[no_unique_address]] std::conditional_t< InsertOnly, std::atomic< branch_node< 256 >* >, std::monostate > frozen{};

            /** @brief Constructs an empty link with an immutable parent edge. */
            explicit node_link(std::uint8_t edge) noexcept : incoming_byte(edge)
            {
            }
        };

        [[no_unique_address]] Allocator m_allocator;
        [[no_unique_address]] Hash const m_hash;
        [[no_unique_address]] KeyCompareThreeWay const m_compare;
        node_link m_root{0};

        /** @brief Allocates and constructs one object through a rebound allocator. */
        template < typename Object, typename... Args >
        Object* allocate_object(Args&&... args)
        {
            using object_allocator = typename std::allocator_traits< Allocator >::template rebind_alloc< Object >;
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

        /** @brief Destroys one object, leaving any raw child pointers it contains untouched. */
        template < typename Object >
        void destroy_object(Object* object) noexcept
        {
            using object_allocator = typename std::allocator_traits< Allocator >::template rebind_alloc< Object >;
            using object_traits = std::allocator_traits< object_allocator >;
            object_allocator allocator(m_allocator);
            typename object_traits::pointer allocation = std::pointer_traits< typename object_traits::pointer >::pointer_to(*object);
            object_traits::destroy(allocator, object);
            object_traits::deallocate(allocator, allocation, 1);
        }

        /** @brief Reclaims unpublished object storage without taking ownership of borrowed children. */
        template < typename Object >
        struct unpublished_deleter
        {
            basic_concurrent_hash_trie* owner;

            /** @brief Releases the staged object through its owning allocator. */
            void operator()(Object* object) const noexcept
            { owner->destroy_object(object); }
        };

        /** @brief Destroys a payload and all its descendants under exclusive or quiescent access. */
        void destroy_payload(node_payload& payload) noexcept
        {
            std::visit(
                [this](auto node)
                {
                    using node_type = decltype(node);
                    if constexpr (!std::is_same_v< node_type, std::monostate >)
                    {
                        if constexpr (!std::is_same_v< node_type, leaf_node* >)
                        {
                            for (std::atomic< node_link* >& slot : node->children)
                            {
                                if (node_link* child = slot.load(std::memory_order_relaxed))
                                {
                                    destroy_payload(child->payload);
                                    destroy_object(child);
                                }
                            }
                        }
                        destroy_object(node);
                    }
                },
                payload);
            payload = std::monostate{};
        }

        /** @brief Extracts one of the eight little-endian hash routing bytes. */
        static std::uint8_t hash_byte(hash_type hash, std::uint8_t depth) noexcept
        {
            assert(depth < 8);
            return static_cast< std::uint8_t >(hash >> (depth * 8));
        }

        /** @brief Tests the compressed prefix preceding a branch's routing byte. */
        template < typename Node >
        static bool matches_prefix(Node const& node, hash_type hash) noexcept
        {
            hash_type mask = (hash_type{1} << (node.depth * 8)) - 1;
            return ((node.prefix_hash ^ hash) & mask) == 0;
        }

        /** @brief Finds a published child while the owning branch is protected. */
        template < typename Node >
        static node_link* child_for(Node const& node, std::uint8_t edge) noexcept
        {
            if constexpr (Node::capacity == 256)
            {
                return node.children[edge].load(std::memory_order_acquire);
            }
            else if constexpr (Node::capacity == 48)
            {
                std::uint8_t index = node.byte_to_slot[edge].load(std::memory_order_acquire);
                return index == 0 ? nullptr : node.children[index - 1].load(std::memory_order_acquire);
            }
            else
            {
                for (std::atomic< node_link* > const& slot : node.children)
                {
                    node_link* child = slot.load(std::memory_order_acquire);
                    if (child && child->incoming_byte == edge)
                    {
                        return child;
                    }
                }
                return nullptr;
            }
        }

        /**
         * @brief Publishes or finds an empty child under shared branch protection.
         * Sparse insertions scan all slots before claiming the first vacancy.
         * Removals require exclusive branch protection, preventing slot reuse here.
         * Returns nullptr when promotion is required.
         */
        template < typename Node >
        node_link* publish_child(Node& node, std::uint8_t edge)
        {
            std::unique_ptr< node_link, unpublished_deleter< node_link > > candidate(nullptr, unpublished_deleter< node_link >{this});
            for (;;)
            {
                size_type vacant = Node::capacity;
                size_type first = Node::capacity == 256 ? edge : 0;
                size_type last = Node::capacity == 256 ? static_cast< size_type >(edge) + 1 : Node::capacity;
                for (size_type index = first; index < last; ++index)
                {
                    node_link* child = node.children[index].load(std::memory_order_acquire);
                    if (child && child->incoming_byte == edge)
                    {
                        if constexpr (Node::capacity == 48)
                        {
                            // A contender can observe a slot before its publisher
                            // has stored the index. Complete that publication too.
                            node.byte_to_slot[edge].store(static_cast< std::uint8_t >(index + 1), std::memory_order_release);
                        }
                        return child;
                    }
                    if (!child && vacant == Node::capacity)
                    {
                        vacant = index;
                    }
                }
                if (vacant == Node::capacity)
                {
                    return nullptr;
                }
                if (!candidate)
                {
                    candidate.reset(allocate_object< node_link >(edge));
                }
                node_link* expected = nullptr;
                if (node.children[vacant].compare_exchange_strong(expected, candidate.get(), std::memory_order_release, std::memory_order_relaxed))
                {
                    if constexpr (Node::capacity == 48)
                    {
                        node.byte_to_slot[edge].store(static_cast< std::uint8_t >(vacant + 1), std::memory_order_release);
                    }
                    return candidate.release();
                }
            }
        }

        /** @brief Copies child ownership into an unpublished layout without moving links or entries. */
        template < typename Target, typename Source >
        static void transfer_children(Target& target, Source const& source) noexcept
        {
            size_type next = 0;
            for (std::atomic< node_link* > const& slot : source.children)
            {
                if (node_link* child = slot.load(std::memory_order_relaxed))
                {
                    size_type index = Target::capacity == 256 ? child->incoming_byte : next++;
                    assert(index < Target::capacity);
                    target.children[index].store(child, std::memory_order_relaxed);
                    if constexpr (Target::capacity == 48)
                    {
                        target.byte_to_slot[child->incoming_byte].store(static_cast< std::uint8_t >(index + 1), std::memory_order_relaxed);
                    }
                }
            }
        }

        /** @brief Replaces a branch layout under its link's exclusive lock. */
        template < typename Replacement, typename Node >
        void replace_branch(node_link& link, Node* node)
        {
            Replacement* replacement = allocate_object< Replacement >(node->prefix_hash, node->depth);
            transfer_children(*replacement, *node);
            link.payload = replacement;
            destroy_object(node);
            if constexpr (InsertOnly && Replacement::capacity == 256)
            {
                link.frozen.store(replacement, std::memory_order_release);
            }
        }

        /** @brief Splits a leaf or compressed branch at its first differing hash byte. */
        void split_link(node_link& link, hash_type existing, hash_type incoming)
        {
            assert(existing != incoming);
            std::uint8_t depth = static_cast< std::uint8_t >(std::countr_zero(existing ^ incoming) / 8);
            std::unique_ptr< branch_node< 4 >, unpublished_deleter< branch_node< 4 > > > branch(allocate_object< branch_node< 4 > >(existing, depth), {this});
            std::unique_ptr< node_link, unpublished_deleter< node_link > > retained(allocate_object< node_link >(hash_byte(existing, depth)), {this});
            std::unique_ptr< node_link, unpublished_deleter< node_link > > added(allocate_object< node_link >(hash_byte(incoming, depth)), {this});
            retained->payload = link.payload;
            // Readers already using a frozen layout continue through the retained subtree.
            if constexpr (InsertOnly)
            {
                retained->frozen.store(link.frozen.load(std::memory_order_relaxed), std::memory_order_relaxed);
            }
            branch->children[0].store(retained.get(), std::memory_order_relaxed);
            branch->children[1].store(added.get(), std::memory_order_relaxed);
            link.payload = branch.release();
            if constexpr (InsertOnly) { link.frozen.store(nullptr, std::memory_order_release); }
            retained.release();
            added.release();
        }

        /** @brief Shared traversal decision: descend, acquire exclusive protection, or report absence. */
        struct write_route
        {
            node_link* child;
            bool upgrade;
        };

        /** @brief Revalidated terminal target, or a request to repeat shared traversal. */
        struct write_target
        {
            leaf_node* leaf;
            bool ready;
        };

        /** @brief Executes a read operation while retaining shared protection on the complete path. */
        template < typename Function >
        std::invoke_result_t< Function&, leaf_node const* > read_path(node_link const& link, hash_type hash, Function& function) const
        {
            if constexpr (InsertOnly)
            {
                if (branch_node< 256 >* node = link.frozen.load(std::memory_order_acquire))
                {
                    node_link* child = matches_prefix(*node, hash) ? child_for(*node, hash_byte(hash, node->depth)) : nullptr;
                    return child ? read_path(*child, hash, function) : function(nullptr);
                }
            }
            std::shared_lock< thin_shared_mutex > lock(link.mutex);
            return std::visit(
                [&](auto node) -> std::invoke_result_t< Function&, leaf_node const* >
                {
                    using node_type = decltype(node);
                    if constexpr (std::is_same_v< node_type, std::monostate >)
                    {
                        return function(nullptr);
                    }
                    else if constexpr (std::is_same_v< node_type, leaf_node* >)
                    {
                        return function(node->hash == hash ? node : nullptr);
                    }
                    else
                    {
                        node_link* child = matches_prefix(*node, hash) ? child_for(*node, hash_byte(hash, node->depth)) : nullptr;
                        return child ? read_path(*child, hash, function) : function(nullptr);
                    }
                },
                link.payload);
        }

        /** @brief Executes a terminal write, optionally constructing routing for an insertion. */
        template < bool Insert, typename Function >
        std::invoke_result_t< Function&, leaf_node* > write_path(node_link& link, hash_type hash, Function& function)
        {
            for (;;)
            {
                if constexpr (InsertOnly)
                {
                    if (branch_node< 256 >* node = link.frozen.load(std::memory_order_acquire))
                    {
                        if (matches_prefix(*node, hash))
                        {
                            node_link* child;
                            if constexpr (Insert) { child = publish_child(*node, hash_byte(hash, node->depth)); }
                            else { child = child_for(*node, hash_byte(hash, node->depth)); }
                            return child ? write_path< Insert >(*child, hash, function) : function(nullptr);
                        }
                        if constexpr (!Insert) { return function(nullptr); }
                    }
                }
                std::shared_lock< thin_shared_mutex > shared(link.mutex);
                write_route route = std::visit(
                    [&](auto node) -> write_route
                    {
                        using node_type = decltype(node);
                        if constexpr (std::is_same_v< node_type, std::monostate >)
                        {
                            return {nullptr, Insert};
                        }
                        else if constexpr (std::is_same_v< node_type, leaf_node* >)
                        {
                            return {nullptr, Insert || node->hash == hash};
                        }
                        else
                        {
                            if (!matches_prefix(*node, hash))
                            {
                                return {nullptr, Insert};
                            }
                            std::uint8_t edge = hash_byte(hash, node->depth);
                            node_link* child = child_for(*node, edge);
                            if constexpr (Insert)
                            {
                                if (!child)
                                {
                                    child = publish_child(*node, edge);
                                }
                            }
                            return {child, Insert && !child};
                        }
                    },
                    link.payload);
                if (route.child)
                {
                    return write_path< Insert >(*route.child, hash, function);
                }
                if (!route.upgrade)
                {
                    return function(nullptr);
                }
                shared.unlock();
                std::unique_lock< thin_shared_mutex > exclusive(link.mutex);
                node_payload snapshot = link.payload;
                write_target target = std::visit(
                    [&](auto node) -> write_target
                    {
                        using node_type = decltype(node);
                        if constexpr (std::is_same_v< node_type, std::monostate >)
                        {
                            if constexpr (Insert)
                            {
                                leaf_node* leaf = allocate_object< leaf_node >(hash, &m_compare, m_allocator);
                                link.payload = leaf;
                                return {leaf, true};
                            }
                            return {nullptr, true};
                        }
                        else if constexpr (std::is_same_v< node_type, leaf_node* >)
                        {
                            if constexpr (Insert)
                            {
                                if (node->entries.empty())
                                {
                                    node->hash = hash;
                                }
                                if (node->hash != hash)
                                {
                                    split_link(link, node->hash, hash);
                                    return {nullptr, false};
                                }
                            }
                            return {node->hash == hash ? node : nullptr, true};
                        }
                        else
                        {
                            if constexpr (Insert)
                            {
                                if (!matches_prefix(*node, hash))
                                {
                                    split_link(link, node->prefix_hash, hash);
                                }
                                else if (!publish_child(*node, hash_byte(hash, node->depth)))
                                {
                                    if constexpr (std::remove_pointer_t< node_type >::capacity == 4)
                                    {
                                        replace_branch< branch_node< 16 > >(link, node);
                                    }
                                    else if constexpr (std::remove_pointer_t< node_type >::capacity == 16)
                                    {
                                        replace_branch< node48 >(link, node);
                                    }
                                    else if constexpr (std::remove_pointer_t< node_type >::capacity == 48)
                                    {
                                        replace_branch< branch_node< 256 > >(link, node);
                                    }
                                    else
                                    {
                                        // Every edge has its own slot in Node256.
                                        std::terminate();
                                    }
                                }
                            }
                            return {nullptr, false};
                        }
                    },
                    snapshot);
                if (target.ready)
                {
                    return function(target.leaf);
                }
            }
        }

        /** @brief Detaches an empty child while the owning link is exclusively locked. */
        template < typename Node >
        void remove_child(Node& node, node_link* child) noexcept
        {
            for (std::atomic< node_link* >& slot : node.children)
            {
                if (slot.load(std::memory_order_relaxed) == child)
                {
                    slot.store(nullptr, std::memory_order_relaxed);
                    if constexpr (Node::capacity == 48)
                    {
                        node.byte_to_slot[child->incoming_byte].store(0, std::memory_order_relaxed);
                    }
                    destroy_object(child);
                    return;
                }
            }
            std::terminate();
        }

        /** @brief Prunes an affected path while an exclusive ancestor excludes all subtree access. */
        bool normalize_path(node_link& link, hash_type hash)
        {
            node_payload snapshot = link.payload;
            return std::visit(
                [&](auto node) -> bool
                {
                    using node_type = decltype(node);
                    if constexpr (std::is_same_v< node_type, std::monostate >)
                    {
                        return true;
                    }
                    else if constexpr (std::is_same_v< node_type, leaf_node* >)
                    {
                        if (node->entries.empty())
                        {
                            link.payload = std::monostate{};
                            destroy_object(node);
                            return true;
                        }
                        return false;
                    }
                    else
                    {
                        if (matches_prefix(*node, hash))
                        {
                            node_link* child = child_for(*node, hash_byte(hash, node->depth));
                            if (child && normalize_path(*child, hash))
                            {
                                remove_child(*node, child);
                            }
                        }
                        size_type count = 0;
                        node_link* retained = nullptr;
                        for (std::atomic< node_link* > const& slot : node->children)
                        {
                            if (node_link* child = slot.load(std::memory_order_relaxed))
                            {
                                ++count;
                                retained = child;
                            }
                        }
                        if (count == 0)
                        {
                            link.payload = std::monostate{};
                            destroy_object(node);
                            return true;
                        }
                        if (count == 1)
                        {
                            link.payload = retained->payload;
                            retained->payload = std::monostate{};
                            destroy_object(retained);
                            destroy_object(node);
                            return normalize_path(link, hash);
                        }
                        if constexpr (std::remove_pointer_t< node_type >::capacity == 256)
                        {
                            if (count <= 37)
                            {
                                replace_branch< node48 >(link, node);
                            }
                        }
                        else if constexpr (std::remove_pointer_t< node_type >::capacity == 48)
                        {
                            if (count <= 12)
                            {
                                replace_branch< branch_node< 16 > >(link, node);
                            }
                        }
                        else if constexpr (std::remove_pointer_t< node_type >::capacity == 16)
                        {
                            if (count <= 3)
                            {
                                replace_branch< branch_node< 4 > >(link, node);
                            }
                        }
                        return false;
                    }
                },
                snapshot);
        }

        /** @brief Reacquires only ancestors needing reclamation, retaining higher shared locks. */
        bool reclaim_path(node_link& link, hash_type hash)
        {
            std::shared_lock< thin_shared_mutex > shared(link.mutex);
            bool needs_exclusive = std::visit(
                [&](auto node) -> bool
                {
                    using node_type = decltype(node);
                    if constexpr (std::is_same_v< node_type, std::monostate >)
                    {
                        return true;
                    }
                    else if constexpr (std::is_same_v< node_type, leaf_node* >)
                    {
                        return node->entries.empty();
                    }
                    else
                    {
                        node_link* child = matches_prefix(*node, hash) ? child_for(*node, hash_byte(hash, node->depth)) : nullptr;
                        return child && reclaim_path(*child, hash);
                    }
                },
                link.payload);
            if (!needs_exclusive)
            {
                return false;
            }
            shared.unlock();
            std::unique_lock< thin_shared_mutex > exclusive(link.mutex);
            return normalize_path(link, hash);
        }

        /** @brief Sums leaf counts using shared path locks or externally guaranteed quiescence. */
        template < bool Locked >
        static size_type count_entries(node_link const& link)
        {
            std::shared_lock< thin_shared_mutex > lock(link.mutex, std::defer_lock);
            if constexpr (Locked)
            {
                lock.lock();
            }
            return std::visit(
                [](auto node) -> size_type
                {
                    using node_type = decltype(node);
                    if constexpr (std::is_same_v< node_type, std::monostate >)
                    {
                        return 0;
                    }
                    else if constexpr (std::is_same_v< node_type, leaf_node* >)
                    {
                        return node->entries.size();
                    }
                    else
                    {
                        size_type count = 0;
                        for (std::atomic< node_link* > const& slot : node->children)
                        {
                            if (node_link* child = slot.load(std::memory_order_acquire))
                            {
                                count += count_entries< Locked >(*child);
                            }
                        }
                        return count;
                    }
                },
                link.payload);
        }

        /** @brief One frame of quiescent depth-first routing traversal. */
        struct iteration_frame
        {
            node_link const* link = nullptr;
            size_type next_slot = 0;
        };

        /** @brief Forward traversal with an automatic stack bounded by the eight routing bytes. */
        template < bool IsConst >
        class basic_iterator
        {
            friend class basic_concurrent_hash_trie;
            template < bool >
            friend class basic_iterator;

            using leaf_reference = std::conditional_t< IsConst, leaf_node const&, leaf_node& >;
            using entry_iterator = decltype(std::declval< leaf_reference >().entries.begin());
            basic_concurrent_hash_trie const* m_owner = nullptr;
            std::array< iteration_frame, 9 > m_path{};
            size_type m_depth = 0;
            leaf_node* m_leaf = nullptr;
            entry_iterator m_entry{};

            /** @brief Advances the routing stack to the next nonempty collision group. */
            void seek_leaf()
            {
                while (m_depth != 0)
                {
                    iteration_frame& frame = m_path[m_depth - 1];
                    node_link* child = std::visit(
                        [&](auto node) -> node_link*
                        {
                            using node_type = decltype(node);
                            if constexpr (std::is_same_v< node_type, std::monostate >)
                            {
                                --m_depth;
                            }
                            else if constexpr (std::is_same_v< node_type, leaf_node* >)
                            {
                                --m_depth;
                                if (!node->entries.empty())
                                {
                                    m_leaf = node;
                                }
                            }
                            else
                            {
                                while (frame.next_slot < node->capacity)
                                {
                                    if (node_link* next = node->children[frame.next_slot++].load(std::memory_order_relaxed))
                                    {
                                        return next;
                                    }
                                }
                                --m_depth;
                            }
                            return nullptr;
                        },
                        frame.link->payload);
                    if (m_leaf)
                    {
                        m_entry = m_leaf->entries.begin();
                        return;
                    }
                    if (child)
                    {
                        assert(m_depth < m_path.size());
                        m_path[m_depth++] = {child, 0};
                    }
                }
            }

            /** @brief Constructs either the first iterator or the end iterator for an owner. */
            basic_iterator(basic_concurrent_hash_trie const* owner, bool end) : m_owner(owner)
            {
                if (!end)
                {
                    m_path[m_depth++] = {&owner->m_root, 0};
                    seek_leaf();
                }
            }

          public:
            using iterator_category = std::forward_iterator_tag;
            using iterator_concept = std::forward_iterator_tag;
            using value_type = typename basic_concurrent_hash_trie::value_type;
            using difference_type = std::ptrdiff_t;
            using pointer = std::conditional_t< IsConst, value_type const*, value_type* >;
            using reference = std::conditional_t< IsConst, value_type const&, value_type& >;

            /** @brief Constructs a singular iterator. */
            basic_iterator() = default;

            /** @brief Converts a mutable iterator to immutable traversal. */
            template < bool Other >
                requires(IsConst && !Other)
            basic_iterator(basic_iterator< Other > const& other) : m_owner(other.m_owner), m_path(other.m_path), m_depth(other.m_depth), m_leaf(other.m_leaf), m_entry(other.m_entry)
            {
            }

            /** @brief Returns the current entry with immutable key access. */
            reference operator*() const
            { return *m_entry; }

            /** @brief Returns the stable address of the current entry. */
            pointer operator->() const
            { return std::addressof(*m_entry); }

            /** @brief Advances to the next entry or collision group. */
            basic_iterator& operator++()
            {
                ++m_entry;
                if (m_entry == m_leaf->entries.end())
                {
                    m_leaf = nullptr;
                    seek_leaf();
                }
                return *this;
            }

            /** @brief Returns the previous position and advances to the next entry. */
            basic_iterator operator++(int)
            {
                basic_iterator previous = *this;
                ++*this;
                return previous;
            }

            /** @brief Compares positions belonging to the same owner. */
            template < bool Other >
            bool operator==(basic_iterator< Other > const& other) const
            { return m_owner == other.m_owner && m_leaf == other.m_leaf && (!m_leaf || m_entry == other.m_entry); }
        };

        /** @brief Non-owning quiescent traversal range. */
        template < typename Iterator >
        class basic_exclusive_range
        {
            Iterator m_begin;
            Iterator m_end;

          public:
            /** @brief Stores the bounds of a traversal requiring external synchronization. */
            basic_exclusive_range(Iterator first, Iterator last) : m_begin(std::move(first)), m_end(std::move(last))
            {
            }

            /** @brief Returns the first traversal position. */
            Iterator begin() const
            { return m_begin; }

            /** @brief Returns the past-the-end traversal position. */
            Iterator end() const
            { return m_end; }
        };

      public:
        using iterator = basic_iterator< false >;
        using const_iterator = basic_iterator< true >;
        using exclusive_range = basic_exclusive_range< iterator >;
        using const_exclusive_range = basic_exclusive_range< const_iterator >;

        /** @brief Constructs an empty trie with default policies. */
        basic_concurrent_hash_trie() : basic_concurrent_hash_trie(Hash(), KeyCompareThreeWay(), Allocator())
        {
        }

        /** @brief Constructs an empty trie using the supplied allocator. */
        explicit basic_concurrent_hash_trie(Allocator const& allocator) : basic_concurrent_hash_trie(Hash(), KeyCompareThreeWay(), allocator)
        {
        }

        /** @brief Constructs an empty trie with shared hash, comparison and allocation policies. */
        explicit basic_concurrent_hash_trie(Hash const& hash, KeyCompareThreeWay const& compare = KeyCompareThreeWay(), Allocator const& allocator = Allocator()) : m_allocator(allocator), m_hash(hash), m_compare(compare)
        {
        }

        /** @brief Prevents copying synchronization ownership. */
        basic_concurrent_hash_trie(basic_concurrent_hash_trie const&) = delete;
        /** @brief Prevents copying synchronization ownership. */
        basic_concurrent_hash_trie& operator=(basic_concurrent_hash_trie const&) = delete;
        /** @brief Prevents relocating synchronization ownership and policy addresses. */
        basic_concurrent_hash_trie(basic_concurrent_hash_trie&&) = delete;
        /** @brief Prevents relocating synchronization ownership and policy addresses. */
        basic_concurrent_hash_trie& operator=(basic_concurrent_hash_trie&&) = delete;

        /** @brief Releases all storage; callers must guarantee quiescent access. */
        ~basic_concurrent_hash_trie()
        { destroy_payload(m_root.payload); }

        /** @brief Returns the allocator used for entries and routing storage. */
        allocator_type get_allocator() const noexcept
        { return m_allocator; }

        /** @brief Returns the hash policy. */
        hasher hash_function() const
        { return m_hash; }

        /** @brief Returns the three-way key comparison policy. */
        key_compare_three_way key_comp_three_way() const
        { return m_compare; }

        /** @brief Copies a missing key and constructs its mapped value without consuming duplicate arguments. */
        template < typename... Args >
        std::pair< pointer, bool > try_emplace(K const& key, Args&&... args)
        {
            auto operation = [&](leaf_node* leaf) -> std::pair< pointer, bool >
            {
                auto result = leaf->entries.try_emplace(key, std::forward< Args >(args)...);
                return {std::addressof(*result.first), result.second};
            };
            return write_path< true >(m_root, static_cast< hash_type >(m_hash(key)), operation);
        }

        /** @brief Moves a missing key and constructs its mapped value without consuming duplicate arguments. */
        template < typename... Args >
        std::pair< pointer, bool > try_emplace(K&& key, Args&&... args)
        {
            auto operation = [&](leaf_node* leaf) -> std::pair< pointer, bool >
            {
                auto result = leaf->entries.try_emplace(std::move(key), std::forward< Args >(args)...);
                return {std::addressof(*result.first), result.second};
            };
            return write_path< true >(m_root, static_cast< hash_type >(m_hash(key)), operation);
        }

        /** @brief Copies a missing key or assigns its existing value under one exclusive terminal lock. */
        template < typename Mapped >
        std::pair< pointer, bool > insert_or_assign(K const& key, Mapped&& value)
        {
            auto operation = [&](leaf_node* leaf) -> std::pair< pointer, bool >
            {
                auto result = leaf->entries.insert_or_assign(key, std::forward< Mapped >(value));
                return {std::addressof(*result.first), result.second};
            };
            return write_path< true >(m_root, static_cast< hash_type >(m_hash(key)), operation);
        }

        /** @brief Moves a missing key or assigns its existing value under one exclusive terminal lock. */
        template < typename Mapped >
        std::pair< pointer, bool > insert_or_assign(K&& key, Mapped&& value)
        {
            auto operation = [&](leaf_node* leaf) -> std::pair< pointer, bool >
            {
                auto result = leaf->entries.insert_or_assign(std::move(key), std::forward< Mapped >(value));
                return {std::addressof(*result.first), result.second};
            };
            return write_path< true >(m_root, static_cast< hash_type >(m_hash(key)), operation);
        }

        /** @brief Returns an entry pointer or nullptr, releasing protection before returning. */
        pointer find(K const& key)
        {
            auto operation = [&](leaf_node const* leaf) -> pointer
            {
                if (!leaf)
                {
                    return nullptr;
                }
                auto found = leaf->entries.find(key);
                return found == leaf->entries.end() ? nullptr : const_cast< pointer >(std::addressof(*found));
            };
            return read_path(m_root, static_cast< hash_type >(m_hash(key)), operation);
        }

        /** @brief Returns an immutable entry pointer or nullptr, releasing protection before returning. */
        const_pointer find(K const& key) const
        {
            auto operation = [&](leaf_node const* leaf) -> const_pointer
            {
                if (!leaf)
                {
                    return nullptr;
                }
                auto found = leaf->entries.find(key);
                return found == leaf->entries.end() ? nullptr : std::addressof(*found);
            };
            return read_path(m_root, static_cast< hash_type >(m_hash(key)), operation);
        }

        /** @brief Reports whether key was present during protected lookup. */
        bool contains(K const& key) const
        {
            auto operation = [&](leaf_node const* leaf)
            {
                return leaf && leaf->entries.find(key) != leaf->entries.end();
            };
            return read_path(m_root, static_cast< hash_type >(m_hash(key)), operation);
        }

        /** @brief Returns a mapped reference, throwing std::out_of_range when absent. */
        V& at(K const& key)
        {
            auto operation = [&](leaf_node const* leaf) -> V&
            {
                if (leaf)
                {
                    auto found = leaf->entries.find(key);
                    if (found != leaf->entries.end())
                    {
                        return const_cast< V& >(found->second);
                    }
                }
                throw std::out_of_range("basic_concurrent_hash_trie::at key not found");
            };
            return read_path(m_root, static_cast< hash_type >(m_hash(key)), operation);
        }

        /** @brief Returns an immutable mapped reference, throwing std::out_of_range when absent. */
        V const& at(K const& key) const
        {
            auto operation = [&](leaf_node const* leaf) -> V const&
            {
                if (leaf)
                {
                    auto found = leaf->entries.find(key);
                    if (found != leaf->entries.end())
                    {
                        return found->second;
                    }
                }
                throw std::out_of_range("basic_concurrent_hash_trie::at key not found");
            };
            return read_path(m_root, static_cast< hash_type >(m_hash(key)), operation);
        }

        /** @brief Copies a missing key and default-constructs its mapped value. */
        V& operator[](K const& key)
        {
            auto operation = [&](leaf_node* leaf) -> V&
            {
                return leaf->entries.try_emplace(key).first->second;
            };
            return write_path< true >(m_root, static_cast< hash_type >(m_hash(key)), operation);
        }

        /** @brief Moves a missing key and default-constructs its mapped value. */
        V& operator[](K&& key)
        {
            auto operation = [&](leaf_node* leaf) -> V&
            {
                return leaf->entries.try_emplace(std::move(key)).first->second;
            };
            return write_path< true >(m_root, static_cast< hash_type >(m_hash(key)), operation);
        }

        /** @brief Calls function(entry const&) under shared protection when the key exists. */
        template < typename Function >
        bool with_read(K const& key, Function&& function) const
        {
            auto operation = [&](leaf_node const* leaf)
            {
                if (leaf)
                {
                    auto found = leaf->entries.find(key);
                    if (found != leaf->entries.end())
                    {
                        std::invoke(std::forward< Function >(function), *found);
                        return true;
                    }
                }
                return false;
            };
            return read_path(m_root, static_cast< hash_type >(m_hash(key)), operation);
        }

        /** @brief Calls function(entry&) under exclusive terminal protection when the key exists. */
        template < typename Function >
        bool with_write(K const& key, Function&& function)
        {
            auto operation = [&](leaf_node* leaf)
            {
                if (leaf)
                {
                    auto found = leaf->entries.find(key);
                    if (found != leaf->entries.end())
                    {
                        std::invoke(std::forward< Function >(function), *found);
                        return true;
                    }
                }
                return false;
            };
            return write_path< false >(m_root, static_cast< hash_type >(m_hash(key)), operation);
        }

        /** @brief Return an existing value or calculate a missing value under its terminal lock. */
        template < typename Function >
        V& get_or_create(K const& key, Function&& function)
        {
            if (pointer entry = find(key)) { return entry->second; }
            auto operation = [&](leaf_node* leaf) -> V&
            {
                auto found = leaf->entries.find(key);
                if (found != leaf->entries.end()) { return found->second; }
                return leaf->entries.try_emplace(key, std::invoke(std::forward< Function >(function))).first->second;
            };
            return write_path< true >(m_root, static_cast< hash_type >(m_hash(key)), operation);
        }

        /** @brief Erases key and reclaims affected routing; allocation failure may follow committed erasure. */
        size_type erase(K const& key) requires (!InsertOnly)
        {
            hash_type hash = static_cast< hash_type >(m_hash(key));
            bool reclaim = false;
            auto operation = [&](leaf_node* leaf) -> size_type
            {
                size_type removed = leaf ? leaf->entries.erase(key) : 0;
                reclaim = !leaf || leaf->entries.empty();
                return removed;
            };
            size_type removed = write_path< false >(m_root, hash, operation);
            if (reclaim)
            {
                reclaim_path(m_root, hash);
            }
            return removed;
        }

        /** @brief Clears all entries under the root's exclusive lock. */
        void clear() requires (!InsertOnly)
        {
            std::unique_lock< thin_shared_mutex > lock(m_root.mutex);
            destroy_payload(m_root.payload);
        }

        /** @brief Sums protected leaf counts; concurrent mutations can produce a non-snapshot result. */
        size_type estimate_size() const
        { return count_entries< true >(m_root); }

        /** @brief Returns the exact count; callers must exclude concurrent mutation. */
        size_type size_exclusive() const
        { return count_entries< false >(m_root); }

        /** @brief Returns a mutable range; callers must exclude concurrent mutation and preserve routing throughout iterator use. */
        exclusive_range range_exclusive()
        { return {iterator(this, false), iterator(this, true)}; }

        /** @brief Returns an immutable range; callers must exclude concurrent mutation and preserve routing throughout iterator use. */
        const_exclusive_range range_exclusive() const
        { return {const_iterator(this, false), const_iterator(this, true)}; }
    };
    /** @brief Adaptive concurrent hash trie supporting insertion and erasure. */
    template < typename K, typename V, typename Hash = std::hash< K >, typename KeyCompareThreeWay = std::compare_three_way, typename Allocator = std::allocator< std::pair< K const, V > > >
    using concurrent_hash_trie = basic_concurrent_hash_trie< K, V, Hash, KeyCompareThreeWay, Allocator, false >;
} // namespace rpnx

#endif
