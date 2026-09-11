// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>

#ifndef RPNXDATASTRUCTURES_MAP_HPP
#define RPNXDATASTRUCTURES_MAP_HPP

#include <compare>
#include <initializer_list>
#include <map>
#include <memory>
#include <type_traits>
#include <utility>

namespace rpnx
{
    /**
     * @brief Ordered key-value container with size-first ordering.
     *
     * `rpnx::map` is currently implemented as a wrapper around `std::map` and
     * exposes the same common container operations. Its relational comparisons
     * differ from `std::map`: a map with fewer elements compares less than a map
     * with more elements, regardless of the stored key-value pairs. Maps with
     * equal sizes compare through the underlying `std::map`.
     *
     * @tparam Key Stored key type.
     * @tparam T Stored mapped value type.
     * @tparam Compare Ordering predicate used by the underlying `std::map`.
     * @tparam Allocator Allocator used by the underlying `std::map`.
     */
    template < typename Key, typename T, typename Compare = std::less< Key >, typename Allocator = std::allocator< std::pair< Key const, T > > >
    class map
    {
      public:
        /// Underlying ordered associative container type.
        using underlying_type = std::map< Key, T, Compare, Allocator >;
        /// Key type used to order and identify elements.
        using key_type = typename underlying_type::key_type;
        /// Value associated with each key.
        using mapped_type = typename underlying_type::mapped_type;
        /// Stored key-value pair type.
        using value_type = typename underlying_type::value_type;
        /// Unsigned type used for element counts.
        using size_type = typename underlying_type::size_type;
        /// Signed type used for iterator distances.
        using difference_type = typename underlying_type::difference_type;
        /// Function object used to order keys.
        using key_compare = typename underlying_type::key_compare;
        /// Function object used to order stored key-value pairs by key.
        using value_compare = typename underlying_type::value_compare;
        /// Allocator type used to manage nodes.
        using allocator_type = typename underlying_type::allocator_type;
        /// Mutable stored-value reference type.
        using reference = typename underlying_type::reference;
        /// Immutable stored-value reference type.
        using const_reference = typename underlying_type::const_reference;
        /// Mutable stored-value pointer type.
        using pointer = typename underlying_type::pointer;
        /// Immutable stored-value pointer type.
        using const_pointer = typename underlying_type::const_pointer;
        /// Mutable bidirectional iterator type.
        using iterator = typename underlying_type::iterator;
        /// Immutable bidirectional iterator type.
        using const_iterator = typename underlying_type::const_iterator;
        /// Mutable reverse-iterator type.
        using reverse_iterator = typename underlying_type::reverse_iterator;
        /// Immutable reverse-iterator type.
        using const_reverse_iterator = typename underlying_type::const_reverse_iterator;
        /// Owning handle for an extracted node.
        using node_type = typename underlying_type::node_type;
        /// Result returned when inserting a node handle without a hint.
        using insert_return_type = typename underlying_type::insert_return_type;

      private:
        underlying_type m_map;

      public:
        /**
         * @brief Constructs an empty map.
         */
        map() = default;

        /**
         * @brief Constructs an empty map with a comparator and allocator.
         * @param comp Comparator used to order keys.
         * @param alloc Allocator used to allocate elements.
         */
        explicit map(Compare const& comp, Allocator const& alloc = Allocator()) : m_map(comp, alloc)
        {
        }

        /**
         * @brief Constructs an empty map with an allocator.
         * @param alloc Allocator used to allocate elements.
         */
        explicit map(Allocator const& alloc) : m_map(alloc)
        {
        }

        /**
         * @brief Constructs a map from an iterator range.
         * @tparam InputIt Input iterator type.
         * @param first First element in the source range.
         * @param last One-past-last element in the source range.
         * @param comp Comparator used to order keys.
         * @param alloc Allocator used to allocate elements.
         */
        template < typename InputIt >
        map(InputIt first, InputIt last, Compare const& comp = Compare(), Allocator const& alloc = Allocator()) : m_map(first, last, comp, alloc)
        {
        }

        /**
         * @brief Constructs a map from an iterator range and allocator.
         * @tparam InputIt Input iterator type.
         * @param first First element in the source range.
         * @param last One-past-last element in the source range.
         * @param alloc Allocator used to allocate elements.
         */
        template < typename InputIt >
        map(InputIt first, InputIt last, Allocator const& alloc) : m_map(first, last, alloc)
        {
        }

        /**
         * @brief Constructs a map from an initializer list.
         * @param init Values to insert.
         * @param comp Comparator used to order keys.
         * @param alloc Allocator used to allocate elements.
         */
        map(std::initializer_list< value_type > init, Compare const& comp = Compare(), Allocator const& alloc = Allocator()) : m_map(init, comp, alloc)
        {
        }

        /**
         * @brief Constructs a map from an initializer list and allocator.
         * @param init Values to insert.
         * @param alloc Allocator used to allocate elements.
         */
        map(std::initializer_list< value_type > init, Allocator const& alloc) : m_map(init, alloc)
        {
        }

        /**
         * @brief Copy-constructs a map.
         */
        map(map const&) = default;

        /**
         * @brief Move-constructs a map.
         */
        map(map&&) noexcept(std::is_nothrow_move_constructible_v< underlying_type >) = default;

        /**
         * @brief Copy-constructs a map using the specified allocator.
         * @param other Map to copy.
         * @param alloc Allocator used to allocate elements.
         */
        map(map const& other, Allocator const& alloc) : m_map(other.m_map, alloc)
        {
        }

        /**
         * @brief Move-constructs a map using the specified allocator.
         * @param other Map to move from.
         * @param alloc Allocator used to allocate elements.
         */
        map(map&& other, Allocator const& alloc) : m_map(std::move(other.m_map), alloc)
        {
        }

        /**
         * @brief Copy-assigns another map.
         * @return Reference to this map.
         */
        map& operator=(map const&) = default;

        /**
         * @brief Move-assigns another map.
         * @return Reference to this map.
         */
        map& operator=(map&&) noexcept(std::is_nothrow_move_assignable_v< underlying_type >) = default;

        /**
         * @brief Replaces the contents with an initializer list.
         * @param init Values to insert.
         * @return Reference to this map.
         */
        map& operator=(std::initializer_list< value_type > init)
        {
            m_map = init;
            return *this;
        }

        /**
         * @brief Returns the allocator used by this map.
         * @return Allocator associated with the map.
         */
        allocator_type get_allocator() const noexcept
        {
            return m_map.get_allocator();
        }

        /**
         * @brief Returns the mapped value for an existing key.
         * @param key Key to locate.
         * @return A mutable reference to the mapped value.
         * @throws std::out_of_range if the key is absent.
         */
        T& at(Key const& key)
        {
            return m_map.at(key);
        }

        /** @brief Returns the mapped value for an existing key. @param key Key to locate. @return An immutable reference to the mapped value. @throws std::out_of_range if the key is absent. */
        T const& at(Key const& key) const
        {
            return m_map.at(key);
        }

        /**
         * @brief Returns the mapped value, inserting a default value when absent.
         * @param key Key to find or copy into a new element.
         * @return A mutable reference to the mapped value.
         */
        T& operator[](Key const& key)
        {
            return m_map[key];
        }

        /**
         * @brief Returns the mapped value, moving the key into a new element when absent.
         * @param key Key to find or move into a new element.
         * @return A mutable reference to the mapped value.
         */
        T& operator[](Key&& key)
        {
            return m_map[std::move(key)];
        }

        /** @brief Returns an iterator to the first key-value pair. @return Beginning iterator. */
        iterator begin() noexcept
        {
            return m_map.begin();
        }

        /** @brief Returns an immutable iterator to the first key-value pair. @return Beginning iterator. */
        const_iterator begin() const noexcept
        {
            return m_map.begin();
        }

        /** @brief Returns an immutable iterator to the first key-value pair. @return Beginning iterator. */
        const_iterator cbegin() const noexcept
        {
            return m_map.cbegin();
        }

        /** @brief Returns an iterator one past the final key-value pair. @return Ending iterator. */
        iterator end() noexcept
        {
            return m_map.end();
        }

        /** @brief Returns an immutable iterator one past the final key-value pair. @return Ending iterator. */
        const_iterator end() const noexcept
        {
            return m_map.end();
        }

        /** @brief Returns an immutable iterator one past the final key-value pair. @return Ending iterator. */
        const_iterator cend() const noexcept
        {
            return m_map.cend();
        }

        /** @brief Returns a reverse iterator to the final key-value pair. @return Beginning reverse iterator. */
        reverse_iterator rbegin() noexcept
        {
            return m_map.rbegin();
        }

        /** @brief Returns an immutable reverse iterator to the final key-value pair. @return Beginning reverse iterator. */
        const_reverse_iterator rbegin() const noexcept
        {
            return m_map.rbegin();
        }

        /** @brief Returns an immutable reverse iterator to the final key-value pair. @return Beginning reverse iterator. */
        const_reverse_iterator crbegin() const noexcept
        {
            return m_map.crbegin();
        }

        /** @brief Returns a reverse iterator preceding the first key-value pair. @return Ending reverse iterator. */
        reverse_iterator rend() noexcept
        {
            return m_map.rend();
        }

        /** @brief Returns an immutable reverse iterator preceding the first key-value pair. @return Ending reverse iterator. */
        const_reverse_iterator rend() const noexcept
        {
            return m_map.rend();
        }

        /** @brief Returns an immutable reverse iterator preceding the first key-value pair. @return Ending reverse iterator. */
        const_reverse_iterator crend() const noexcept
        {
            return m_map.crend();
        }

        /** @brief Returns whether the map has no elements. @return `true` when empty. */
        bool empty() const noexcept
        {
            return m_map.empty();
        }

        /** @brief Returns the number of stored key-value pairs. @return Element count. */
        size_type size() const noexcept
        {
            return m_map.size();
        }

        /** @brief Returns the maximum number of elements supported by the implementation. @return Maximum element count. */
        size_type max_size() const noexcept
        {
            return m_map.max_size();
        }

        /** @brief Removes all key-value pairs. */
        void clear() noexcept
        {
            m_map.clear();
        }

        /**
         * @brief Inserts a key-value pair if its key is absent.
         * @param value Pair to copy.
         * @return Iterator to the matching element and whether insertion occurred.
         */
        std::pair< iterator, bool > insert(value_type const& value)
        {
            return m_map.insert(value);
        }

        /**
         * @brief Inserts a key-value pair by moving it if its key is absent.
         * @param value Pair to move.
         * @return Iterator to the matching element and whether insertion occurred.
         */
        std::pair< iterator, bool > insert(value_type&& value)
        {
            return m_map.insert(std::move(value));
        }

        /**
         * @brief Inserts a value constructible as a key-value pair if its key is absent.
         * @tparam P Source value type.
         * @param value Value forwarded to the stored pair constructor.
         * @return Iterator to the matching element and whether insertion occurred.
         */
        template < typename P >
            requires std::is_constructible_v< value_type, P&& >
        std::pair< iterator, bool > insert(P&& value)
        {
            return m_map.insert(std::forward< P >(value));
        }

        /**
         * @brief Inserts a copied key-value pair using an ordering hint.
         * @param hint Position immediately before which insertion may be efficient.
         * @param value Pair to copy.
         * @return Iterator to the matching element.
         */
        iterator insert(const_iterator hint, value_type const& value)
        {
            return m_map.insert(hint, value);
        }

        /**
         * @brief Inserts a moved key-value pair using an ordering hint.
         * @param hint Position immediately before which insertion may be efficient.
         * @param value Pair to move.
         * @return Iterator to the matching element.
         */
        iterator insert(const_iterator hint, value_type&& value)
        {
            return m_map.insert(hint, std::move(value));
        }

        /**
         * @brief Inserts a pair-like value using an ordering hint.
         * @tparam P Source value type.
         * @param hint Position immediately before which insertion may be efficient.
         * @param value Value forwarded to the stored pair constructor.
         * @return Iterator to the matching element.
         */
        template < typename P >
            requires std::is_constructible_v< value_type, P&& >
        iterator insert(const_iterator hint, P&& value)
        {
            return m_map.insert(hint, std::forward< P >(value));
        }

        /**
         * @brief Inserts every key-value pair in an iterator range.
         * @tparam InputIt Input iterator type.
         * @param first First source element.
         * @param last One-past-last source element.
         */
        template < typename InputIt >
        void insert(InputIt first, InputIt last)
        {
            m_map.insert(first, last);
        }

        /** @brief Inserts key-value pairs from an initializer list. @param init Values to insert. */
        void insert(std::initializer_list< value_type > init)
        {
            m_map.insert(init);
        }

        /**
         * @brief Inserts an extracted node if its key is absent.
         * @param node Owning node handle to insert.
         * @return Insertion result containing the position, insertion state, and any uninserted node.
         * @pre `node.empty()` or `get_allocator() == node.get_allocator()`.
         */
        insert_return_type insert(node_type&& node)
        {
            return m_map.insert(std::move(node));
        }

        /**
         * @brief Inserts an extracted node using an ordering hint.
         * @param hint Position immediately before which insertion may be efficient.
         * @param node Owning node handle to insert.
         * @return Iterator to the matching element.
         * @pre `node.empty()` or `get_allocator() == node.get_allocator()`.
         */
        iterator insert(const_iterator hint, node_type&& node)
        {
            return m_map.insert(hint, std::move(node));
        }

        /**
         * @brief Constructs a key-value pair in place if its key is absent.
         * @tparam Args Stored pair constructor argument types.
         * @param args Arguments forwarded to the stored pair constructor.
         * @return Iterator to the matching element and whether insertion occurred.
         */
        template < typename... Args >
        std::pair< iterator, bool > emplace(Args&&... args)
        {
            return m_map.emplace(std::forward< Args >(args)...);
        }

        /**
         * @brief Constructs a key-value pair in place using an ordering hint.
         * @tparam Args Stored pair constructor argument types.
         * @param hint Position immediately before which insertion may be efficient.
         * @param args Arguments forwarded to the stored pair constructor.
         * @return Iterator to the matching element.
         */
        template < typename... Args >
        iterator emplace_hint(const_iterator hint, Args&&... args)
        {
            return m_map.emplace_hint(hint, std::forward< Args >(args)...);
        }

        /**
         * @brief Constructs a mapped value only when a copied key is absent.
         * @tparam Args Mapped-value constructor argument types.
         * @param key Key to find or copy into a new element.
         * @param args Arguments forwarded to the mapped-value constructor.
         * @return Iterator to the matching element and whether insertion occurred.
         */
        template < typename... Args >
        std::pair< iterator, bool > try_emplace(Key const& key, Args&&... args)
        {
            return m_map.try_emplace(key, std::forward< Args >(args)...);
        }

        /**
         * @brief Constructs a mapped value only when a moved key is absent.
         * @tparam Args Mapped-value constructor argument types.
         * @param key Key to find or move into a new element.
         * @param args Arguments forwarded to the mapped-value constructor.
         * @return Iterator to the matching element and whether insertion occurred.
         */
        template < typename... Args >
        std::pair< iterator, bool > try_emplace(Key&& key, Args&&... args)
        {
            return m_map.try_emplace(std::move(key), std::forward< Args >(args)...);
        }

        /**
         * @brief Constructs a mapped value for an absent copied key using an ordering hint.
         * @tparam Args Mapped-value constructor argument types.
         * @param hint Position immediately before which insertion may be efficient.
         * @param key Key to find or copy into a new element.
         * @param args Arguments forwarded to the mapped-value constructor.
         * @return Iterator to the matching element.
         */
        template < typename... Args >
        iterator try_emplace(const_iterator hint, Key const& key, Args&&... args)
        {
            return m_map.try_emplace(hint, key, std::forward< Args >(args)...);
        }

        /**
         * @brief Constructs a mapped value for an absent moved key using an ordering hint.
         * @tparam Args Mapped-value constructor argument types.
         * @param hint Position immediately before which insertion may be efficient.
         * @param key Key to find or move into a new element.
         * @param args Arguments forwarded to the mapped-value constructor.
         * @return Iterator to the matching element.
         */
        template < typename... Args >
        iterator try_emplace(const_iterator hint, Key&& key, Args&&... args)
        {
            return m_map.try_emplace(hint, std::move(key), std::forward< Args >(args)...);
        }

        /**
         * @brief Inserts a copied key or assigns its existing mapped value.
         * @tparam M Source mapped-value type.
         * @param key Key to find or copy into a new element.
         * @param obj Value to forward into the mapped value.
         * @return Iterator to the element and whether insertion occurred.
         */
        template < typename M >
        std::pair< iterator, bool > insert_or_assign(Key const& key, M&& obj)
        {
            return m_map.insert_or_assign(key, std::forward< M >(obj));
        }

        /**
         * @brief Inserts a moved key or assigns its existing mapped value.
         * @tparam M Source mapped-value type.
         * @param key Key to find or move into a new element.
         * @param obj Value to forward into the mapped value.
         * @return Iterator to the element and whether insertion occurred.
         */
        template < typename M >
        std::pair< iterator, bool > insert_or_assign(Key&& key, M&& obj)
        {
            return m_map.insert_or_assign(std::move(key), std::forward< M >(obj));
        }

        /**
         * @brief Inserts a copied key or assigns its value using an ordering hint.
         * @tparam M Source mapped-value type.
         * @param hint Position immediately before which insertion may be efficient.
         * @param key Key to find or copy into a new element.
         * @param obj Value to forward into the mapped value.
         * @return Iterator to the matching element.
         */
        template < typename M >
        iterator insert_or_assign(const_iterator hint, Key const& key, M&& obj)
        {
            return m_map.insert_or_assign(hint, key, std::forward< M >(obj));
        }

        /**
         * @brief Inserts a moved key or assigns its value using an ordering hint.
         * @tparam M Source mapped-value type.
         * @param hint Position immediately before which insertion may be efficient.
         * @param key Key to find or move into a new element.
         * @param obj Value to forward into the mapped value.
         * @return Iterator to the matching element.
         */
        template < typename M >
        iterator insert_or_assign(const_iterator hint, Key&& key, M&& obj)
        {
            return m_map.insert_or_assign(hint, std::move(key), std::forward< M >(obj));
        }

        /** @brief Erases one element. @param pos Element to erase. @return Iterator following the erased element. */
        iterator erase(const_iterator pos)
        {
            return m_map.erase(pos);
        }

        /** @brief Erases a range. @param first First element to erase. @param last One-past-last element to erase. @return Iterator following the erased range. */
        iterator erase(const_iterator first, const_iterator last)
        {
            return m_map.erase(first, last);
        }

        /** @brief Erases the element with a key. @param key Key to erase. @return One if erased, otherwise zero. */
        size_type erase(Key const& key)
        {
            return m_map.erase(key);
        }

        /** @brief Exchanges contents with another map. @param other Map to exchange with. @pre Allocator propagation on swap is enabled, or `get_allocator() == other.get_allocator()`. */
        void swap(map& other) noexcept(noexcept(m_map.swap(other.m_map)))
        {
            m_map.swap(other.m_map);
        }

        /** @brief Removes an element without destroying it. @param pos Element to extract. @return Owning handle for the extracted node. */
        node_type extract(const_iterator pos)
        {
            return m_map.extract(pos);
        }

        /** @brief Removes an element by key without destroying it. @param key Key to extract. @return Owning handle, empty if absent. */
        node_type extract(Key const& key)
        {
            return m_map.extract(key);
        }

        /** @brief Transfers non-duplicate nodes from another RPNX map. @tparam C2 Source comparator type. @param source Map from which nodes are transferred. @pre `get_allocator() == source.get_allocator()`. */
        template < typename C2 >
        void merge(map< Key, T, C2, Allocator >& source)
        {
            m_map.merge(source.m_map);
        }

        /** @brief Transfers non-duplicate nodes from another RPNX map. @tparam C2 Source comparator type. @param source Map from which nodes are transferred. @pre `get_allocator() == source.get_allocator()`. */
        template < typename C2 >
        void merge(map< Key, T, C2, Allocator >&& source)
        {
            m_map.merge(source.m_map);
        }

        /** @brief Transfers non-duplicate nodes from a standard map. @tparam C2 Source comparator type. @param source Map from which nodes are transferred. @pre `get_allocator() == source.get_allocator()`. */
        template < typename C2 >
        void merge(std::map< Key, T, C2, Allocator >& source)
        {
            m_map.merge(source);
        }

        /** @brief Transfers non-duplicate nodes from a standard map. @tparam C2 Source comparator type. @param source Map from which nodes are transferred. @pre `get_allocator() == source.get_allocator()`. */
        template < typename C2 >
        void merge(std::map< Key, T, C2, Allocator >&& source)
        {
            m_map.merge(source);
        }

        /** @brief Counts elements matching a key. @param key Key to find. @return One when present, otherwise zero. */
        size_type count(Key const& key) const
        {
            return m_map.count(key);
        }

        /** @brief Finds an element by key. @param key Key to find. @return Iterator to the element, or `end()`. */
        iterator find(Key const& key)
        {
            return m_map.find(key);
        }

        /** @copydoc find(Key const&) */
        const_iterator find(Key const& key) const
        {
            return m_map.find(key);
        }

        /** @brief Tests whether a key is present. @param key Key to find. @return `true` if present. */
        bool contains(Key const& key) const
        {
            return m_map.contains(key);
        }

        /** @brief Finds the range matching a key. @param key Key to find. @return Pair of lower and upper bounds. */
        std::pair< iterator, iterator > equal_range(Key const& key)
        {
            return m_map.equal_range(key);
        }

        /** @copydoc equal_range(Key const&) */
        std::pair< const_iterator, const_iterator > equal_range(Key const& key) const
        {
            return m_map.equal_range(key);
        }

        /** @brief Finds the first element not ordered before a key. @param key Boundary key. @return Boundary iterator. */
        iterator lower_bound(Key const& key)
        {
            return m_map.lower_bound(key);
        }

        /** @copydoc lower_bound(Key const&) */
        const_iterator lower_bound(Key const& key) const
        {
            return m_map.lower_bound(key);
        }

        /** @brief Finds the first element ordered after a key. @param key Boundary key. @return Boundary iterator. */
        iterator upper_bound(Key const& key)
        {
            return m_map.upper_bound(key);
        }

        /** @copydoc upper_bound(Key const&) */
        const_iterator upper_bound(Key const& key) const
        {
            return m_map.upper_bound(key);
        }

        /** @brief Returns the key-ordering predicate. @return Copy of the comparator. */
        key_compare key_comp() const
        {
            return m_map.key_comp();
        }

        /** @brief Returns the stored-value ordering predicate. @return Copy of the value comparator. */
        value_compare value_comp() const
        {
            return m_map.value_comp();
        }

        template < typename K, typename U, typename C, typename A >
        friend class map;

        /** @brief Grants equality comparison access to the underlying map. */
        template < typename K, typename U, typename C, typename A >
        friend bool operator==(map< K, U, C, A > const& lhs, map< K, U, C, A > const& rhs);

        /** @brief Grants less-than comparison access to the underlying map. */
        template < typename K, typename U, typename C, typename A >
        friend bool operator<(map< K, U, C, A > const& lhs, map< K, U, C, A > const& rhs);

        /** @brief Grants three-way comparison access to the underlying map. */
        template < typename K, typename U, typename C, typename A >
        friend auto operator<=>(map< K, U, C, A > const& lhs, map< K, U, C, A > const& rhs) -> decltype(std::declval< typename map< K, U, C, A >::underlying_type const& >() <=> std::declval< typename map< K, U, C, A >::underlying_type const& >());
    };

    /**
     * @brief Swaps two maps.
     * @param lhs First map.
     * @param rhs Second map.
     */
    template < typename Key, typename T, typename Compare, typename Allocator >
    void swap(map< Key, T, Compare, Allocator >& lhs, map< Key, T, Compare, Allocator >& rhs) noexcept(noexcept(lhs.swap(rhs)))
    {
        lhs.swap(rhs);
    }

    /**
     * @brief Compares two maps for equality.
     * @param lhs Left operand.
     * @param rhs Right operand.
     * @return `true` when the contents are equal.
     */
    template < typename Key, typename T, typename Compare, typename Allocator >
    bool operator==(map< Key, T, Compare, Allocator > const& lhs, map< Key, T, Compare, Allocator > const& rhs)
    {
        return lhs.m_map == rhs.m_map;
    }

    /**
     * @brief Compares two maps for inequality.
     * @param lhs Left operand.
     * @param rhs Right operand.
     * @return `true` when the contents differ.
     */
    template < typename Key, typename T, typename Compare, typename Allocator >
    bool operator!=(map< Key, T, Compare, Allocator > const& lhs, map< Key, T, Compare, Allocator > const& rhs)
    {
        return !(lhs == rhs);
    }

    /**
     * @brief Orders two maps by size first, then by the underlying map ordering.
     * @param lhs Left operand.
     * @param rhs Right operand.
     * @return `true` when `lhs` orders before `rhs`.
     */
    template < typename Key, typename T, typename Compare, typename Allocator >
    bool operator<(map< Key, T, Compare, Allocator > const& lhs, map< Key, T, Compare, Allocator > const& rhs)
    {
        if (lhs.size() != rhs.size())
        {
            return lhs.size() < rhs.size();
        }

        return lhs.m_map < rhs.m_map;
    }

    /**
     * @brief Orders two maps by size first, then by the underlying map ordering.
     * @param lhs Left operand.
     * @param rhs Right operand.
     * @return `true` when `lhs` orders after `rhs`.
     */
    template < typename Key, typename T, typename Compare, typename Allocator >
    bool operator>(map< Key, T, Compare, Allocator > const& lhs, map< Key, T, Compare, Allocator > const& rhs)
    {
        return rhs < lhs;
    }

    /**
     * @brief Orders two maps by size first, then by the underlying map ordering.
     * @param lhs Left operand.
     * @param rhs Right operand.
     * @return `true` when `lhs` does not order after `rhs`.
     */
    template < typename Key, typename T, typename Compare, typename Allocator >
    bool operator<=(map< Key, T, Compare, Allocator > const& lhs, map< Key, T, Compare, Allocator > const& rhs)
    {
        return !(rhs < lhs);
    }

    /**
     * @brief Orders two maps by size first, then by the underlying map ordering.
     * @param lhs Left operand.
     * @param rhs Right operand.
     * @return `true` when `lhs` does not order before `rhs`.
     */
    template < typename Key, typename T, typename Compare, typename Allocator >
    bool operator>=(map< Key, T, Compare, Allocator > const& lhs, map< Key, T, Compare, Allocator > const& rhs)
    {
        return !(lhs < rhs);
    }

    /**
     * @brief Three-way compares two maps by size first, then by the underlying map ordering.
     * @param lhs Left operand.
     * @param rhs Right operand.
     * @return Ordering category result.
     */
    template < typename Key, typename T, typename Compare, typename Allocator >
    auto operator<=>(map< Key, T, Compare, Allocator > const& lhs, map< Key, T, Compare, Allocator > const& rhs) -> decltype(std::declval< typename map< Key, T, Compare, Allocator >::underlying_type const& >() <=> std::declval< typename map< Key, T, Compare, Allocator >::underlying_type const& >())
    {
        using ordering_type = decltype(lhs.m_map <=> rhs.m_map);

        if (lhs.size() < rhs.size())
        {
            return ordering_type::less;
        }

        if (rhs.size() < lhs.size())
        {
            return ordering_type::greater;
        }

        return lhs.m_map <=> rhs.m_map;
    }
} // namespace rpnx

#endif // RPNXDATASTRUCTURES_MAP_HPP
