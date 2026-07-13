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
        using underlying_type = std::map< Key, T, Compare, Allocator >;
        using key_type = typename underlying_type::key_type;
        using mapped_type = typename underlying_type::mapped_type;
        using value_type = typename underlying_type::value_type;
        using size_type = typename underlying_type::size_type;
        using difference_type = typename underlying_type::difference_type;
        using key_compare = typename underlying_type::key_compare;
        using value_compare = typename underlying_type::value_compare;
        using allocator_type = typename underlying_type::allocator_type;
        using reference = typename underlying_type::reference;
        using const_reference = typename underlying_type::const_reference;
        using pointer = typename underlying_type::pointer;
        using const_pointer = typename underlying_type::const_pointer;
        using iterator = typename underlying_type::iterator;
        using const_iterator = typename underlying_type::const_iterator;
        using reverse_iterator = typename underlying_type::reverse_iterator;
        using const_reverse_iterator = typename underlying_type::const_reverse_iterator;
        using node_type = typename underlying_type::node_type;
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
         */
        map& operator=(map const&) = default;

        /**
         * @brief Move-assigns another map.
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
         */
        allocator_type get_allocator() const noexcept
        {
            return m_map.get_allocator();
        }

        T& at(Key const& key)
        {
            return m_map.at(key);
        }

        T const& at(Key const& key) const
        {
            return m_map.at(key);
        }

        T& operator[](Key const& key)
        {
            return m_map[key];
        }

        T& operator[](Key&& key)
        {
            return m_map[std::move(key)];
        }

        iterator begin() noexcept
        {
            return m_map.begin();
        }

        const_iterator begin() const noexcept
        {
            return m_map.begin();
        }

        const_iterator cbegin() const noexcept
        {
            return m_map.cbegin();
        }

        iterator end() noexcept
        {
            return m_map.end();
        }

        const_iterator end() const noexcept
        {
            return m_map.end();
        }

        const_iterator cend() const noexcept
        {
            return m_map.cend();
        }

        reverse_iterator rbegin() noexcept
        {
            return m_map.rbegin();
        }

        const_reverse_iterator rbegin() const noexcept
        {
            return m_map.rbegin();
        }

        const_reverse_iterator crbegin() const noexcept
        {
            return m_map.crbegin();
        }

        reverse_iterator rend() noexcept
        {
            return m_map.rend();
        }

        const_reverse_iterator rend() const noexcept
        {
            return m_map.rend();
        }

        const_reverse_iterator crend() const noexcept
        {
            return m_map.crend();
        }

        bool empty() const noexcept
        {
            return m_map.empty();
        }

        size_type size() const noexcept
        {
            return m_map.size();
        }

        size_type max_size() const noexcept
        {
            return m_map.max_size();
        }

        void clear() noexcept
        {
            m_map.clear();
        }

        std::pair< iterator, bool > insert(value_type const& value)
        {
            return m_map.insert(value);
        }

        std::pair< iterator, bool > insert(value_type&& value)
        {
            return m_map.insert(std::move(value));
        }

        template < typename P >
            requires std::is_constructible_v< value_type, P&& >
        std::pair< iterator, bool > insert(P&& value)
        {
            return m_map.insert(std::forward< P >(value));
        }

        iterator insert(const_iterator hint, value_type const& value)
        {
            return m_map.insert(hint, value);
        }

        iterator insert(const_iterator hint, value_type&& value)
        {
            return m_map.insert(hint, std::move(value));
        }

        template < typename P >
            requires std::is_constructible_v< value_type, P&& >
        iterator insert(const_iterator hint, P&& value)
        {
            return m_map.insert(hint, std::forward< P >(value));
        }

        template < typename InputIt >
        void insert(InputIt first, InputIt last)
        {
            m_map.insert(first, last);
        }

        void insert(std::initializer_list< value_type > init)
        {
            m_map.insert(init);
        }

        insert_return_type insert(node_type&& node)
        {
            return m_map.insert(std::move(node));
        }

        iterator insert(const_iterator hint, node_type&& node)
        {
            return m_map.insert(hint, std::move(node));
        }

        template < typename... Args >
        std::pair< iterator, bool > emplace(Args&&... args)
        {
            return m_map.emplace(std::forward< Args >(args)...);
        }

        template < typename... Args >
        iterator emplace_hint(const_iterator hint, Args&&... args)
        {
            return m_map.emplace_hint(hint, std::forward< Args >(args)...);
        }

        template < typename... Args >
        std::pair< iterator, bool > try_emplace(Key const& key, Args&&... args)
        {
            return m_map.try_emplace(key, std::forward< Args >(args)...);
        }

        template < typename... Args >
        std::pair< iterator, bool > try_emplace(Key&& key, Args&&... args)
        {
            return m_map.try_emplace(std::move(key), std::forward< Args >(args)...);
        }

        template < typename... Args >
        iterator try_emplace(const_iterator hint, Key const& key, Args&&... args)
        {
            return m_map.try_emplace(hint, key, std::forward< Args >(args)...);
        }

        template < typename... Args >
        iterator try_emplace(const_iterator hint, Key&& key, Args&&... args)
        {
            return m_map.try_emplace(hint, std::move(key), std::forward< Args >(args)...);
        }

        template < typename M >
        std::pair< iterator, bool > insert_or_assign(Key const& key, M&& obj)
        {
            return m_map.insert_or_assign(key, std::forward< M >(obj));
        }

        template < typename M >
        std::pair< iterator, bool > insert_or_assign(Key&& key, M&& obj)
        {
            return m_map.insert_or_assign(std::move(key), std::forward< M >(obj));
        }

        template < typename M >
        iterator insert_or_assign(const_iterator hint, Key const& key, M&& obj)
        {
            return m_map.insert_or_assign(hint, key, std::forward< M >(obj));
        }

        template < typename M >
        iterator insert_or_assign(const_iterator hint, Key&& key, M&& obj)
        {
            return m_map.insert_or_assign(hint, std::move(key), std::forward< M >(obj));
        }

        iterator erase(const_iterator pos)
        {
            return m_map.erase(pos);
        }

        iterator erase(const_iterator first, const_iterator last)
        {
            return m_map.erase(first, last);
        }

        size_type erase(Key const& key)
        {
            return m_map.erase(key);
        }

        void swap(map& other) noexcept(noexcept(m_map.swap(other.m_map)))
        {
            m_map.swap(other.m_map);
        }

        node_type extract(const_iterator pos)
        {
            return m_map.extract(pos);
        }

        node_type extract(Key const& key)
        {
            return m_map.extract(key);
        }

        template < typename C2 >
        void merge(map< Key, T, C2, Allocator >& source)
        {
            m_map.merge(source.m_map);
        }

        template < typename C2 >
        void merge(map< Key, T, C2, Allocator >&& source)
        {
            m_map.merge(source.m_map);
        }

        template < typename C2 >
        void merge(std::map< Key, T, C2, Allocator >& source)
        {
            m_map.merge(source);
        }

        template < typename C2 >
        void merge(std::map< Key, T, C2, Allocator >&& source)
        {
            m_map.merge(source);
        }

        size_type count(Key const& key) const
        {
            return m_map.count(key);
        }

        iterator find(Key const& key)
        {
            return m_map.find(key);
        }

        const_iterator find(Key const& key) const
        {
            return m_map.find(key);
        }

        bool contains(Key const& key) const
        {
            return m_map.contains(key);
        }

        std::pair< iterator, iterator > equal_range(Key const& key)
        {
            return m_map.equal_range(key);
        }

        std::pair< const_iterator, const_iterator > equal_range(Key const& key) const
        {
            return m_map.equal_range(key);
        }

        iterator lower_bound(Key const& key)
        {
            return m_map.lower_bound(key);
        }

        const_iterator lower_bound(Key const& key) const
        {
            return m_map.lower_bound(key);
        }

        iterator upper_bound(Key const& key)
        {
            return m_map.upper_bound(key);
        }

        const_iterator upper_bound(Key const& key) const
        {
            return m_map.upper_bound(key);
        }

        key_compare key_comp() const
        {
            return m_map.key_comp();
        }

        value_compare value_comp() const
        {
            return m_map.value_comp();
        }

        template < typename K, typename U, typename C, typename A >
        friend class map;

        template < typename K, typename U, typename C, typename A >
        friend bool operator==(map< K, U, C, A > const& lhs, map< K, U, C, A > const& rhs);

        template < typename K, typename U, typename C, typename A >
        friend bool operator<(map< K, U, C, A > const& lhs, map< K, U, C, A > const& rhs);

        template < typename K, typename U, typename C, typename A >
        friend auto operator<=>(map< K, U, C, A > const& lhs, map< K, U, C, A > const& rhs) -> decltype(std::declval< typename map< K, U, C, A >::underlying_type const& >() <=> std::declval< typename map< K, U, C, A >::underlying_type const& >());
    };

    /**
     * @brief Swaps two maps.
     */
    template < typename Key, typename T, typename Compare, typename Allocator >
    void swap(map< Key, T, Compare, Allocator >& lhs, map< Key, T, Compare, Allocator >& rhs) noexcept(noexcept(lhs.swap(rhs)))
    {
        lhs.swap(rhs);
    }

    /**
     * @brief Compares two maps for equality.
     */
    template < typename Key, typename T, typename Compare, typename Allocator >
    bool operator==(map< Key, T, Compare, Allocator > const& lhs, map< Key, T, Compare, Allocator > const& rhs)
    {
        return lhs.m_map == rhs.m_map;
    }

    /**
     * @brief Compares two maps for inequality.
     */
    template < typename Key, typename T, typename Compare, typename Allocator >
    bool operator!=(map< Key, T, Compare, Allocator > const& lhs, map< Key, T, Compare, Allocator > const& rhs)
    {
        return !(lhs == rhs);
    }

    /**
     * @brief Orders two maps by size first, then by the underlying map ordering.
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
     */
    template < typename Key, typename T, typename Compare, typename Allocator >
    bool operator>(map< Key, T, Compare, Allocator > const& lhs, map< Key, T, Compare, Allocator > const& rhs)
    {
        return rhs < lhs;
    }

    /**
     * @brief Orders two maps by size first, then by the underlying map ordering.
     */
    template < typename Key, typename T, typename Compare, typename Allocator >
    bool operator<=(map< Key, T, Compare, Allocator > const& lhs, map< Key, T, Compare, Allocator > const& rhs)
    {
        return !(rhs < lhs);
    }

    /**
     * @brief Orders two maps by size first, then by the underlying map ordering.
     */
    template < typename Key, typename T, typename Compare, typename Allocator >
    bool operator>=(map< Key, T, Compare, Allocator > const& lhs, map< Key, T, Compare, Allocator > const& rhs)
    {
        return !(lhs < rhs);
    }

    /**
     * @brief Three-way compares two maps by size first, then by the underlying map ordering.
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
