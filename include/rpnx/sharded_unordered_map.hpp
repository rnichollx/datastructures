// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>

#ifndef RPNXDATASTRUCTURES_SHARDED_MAP_HPP
#define RPNXDATASTRUCTURES_SHARDED_MAP_HPP

#include <iterator>
#include <mutex>
#include <new>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rpnx
{
    /**
     * A sharded unordered map that serializes operations per shard.
     *
     * @tparam Key The key type used to index values.
     * @tparam Value The mapped value type.
     * @tparam Hash The hash functor used to choose shards and hash keys inside each shard.
     * @tparam KeyEqual The key equality predicate used inside each shard.
     * @tparam Alloc The allocator used by the shard maps and rebound for shard storage.
     *
     * Operations that access the container structure lock the affected shard. Methods that return a reference release
     * that lock before returning; the caller is responsible for ensuring the referenced element is not erased, replaced,
     * or otherwise concurrently modified while the reference is being used.
     *
     * References to existing elements are not invalidated by inserting other elements into the map, including insertions
     * that rehash the underlying std::unordered_map. They are invalidated by erasing the referenced element and by
     * destroying the map.
     */
    template < typename Key, typename Value, typename Hash = std::hash< Key >, typename KeyEqual = std::equal_to< Key >, typename Alloc = std::allocator< std::pair< const Key, Value > > >
    class conc_sharded_unordered_map
    {
        struct alignas(std::hardware_destructive_interference_size) shard
        {
            mutable std::mutex m_mutex;
            std::unordered_map< Key, Value, Hash, KeyEqual, Alloc > m_map;

            std::mutex& get_mutex() const
            {
                return m_mutex;
            }

            shard(Hash const& hash, KeyEqual const& key_equal, Alloc const& alloc) : m_map(0, hash, key_equal, alloc)
            {
            }

            shard(shard const&) = delete;
            shard& operator=(shard const&) = delete;

            shard(shard&& other) noexcept(std::is_nothrow_move_constructible_v< decltype(m_map) >) : m_map(std::move(other.m_map))
            {
            }

            shard& operator=(shard&&) = delete;
        };

        [[no_unique_address]] Hash m_hash;
        [[no_unique_address]] KeyEqual m_key_equal;
        std::vector< shard, typename std::allocator_traits< Alloc >::template rebind_alloc< shard > > m_shards;

        template < bool IsConst >
        class basic_iterator
        {
            using map_type = std::conditional_t< IsConst, const conc_sharded_unordered_map, conc_sharded_unordered_map >;
            using shard_type = std::conditional_t< IsConst, const shard, shard >;
            using inner_iterator = std::conditional_t< IsConst, typename std::unordered_map< Key, Value, Hash, KeyEqual, Alloc >::const_iterator, typename std::unordered_map< Key, Value, Hash, KeyEqual, Alloc >::iterator >;

            map_type* m_map;
            std::size_t m_shard_index;
            inner_iterator m_inner;

            void advance_to_next_valid()
            {
                while (m_inner == m_map->m_shards[m_shard_index].m_map.end())
                {
                    m_shard_index++;
                    if (m_shard_index >= m_map->m_shards.size())
                    {
                        break;
                    }
                    m_inner = m_map->m_shards[m_shard_index].m_map.begin();
                }
            }

          public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = std::pair< const Key, Value >;
            using difference_type = std::ptrdiff_t;
            using pointer = std::conditional_t< IsConst, const value_type*, value_type* >;
            using reference = std::conditional_t< IsConst, const value_type&, value_type& >;

            basic_iterator() : m_map(nullptr), m_shard_index(0), m_inner()
            {
            }
            basic_iterator(map_type* map, std::size_t shard_index, inner_iterator inner) : m_map(map), m_shard_index(shard_index), m_inner(inner)
            {
                if (m_map && m_shard_index < m_map->m_shards.size())
                {
                    advance_to_next_valid();
                }
            }

            reference operator*() const
            {
                return *m_inner;
            }
            pointer operator->() const
            {
                return &(*m_inner);
            }

            basic_iterator& operator++()
            {
                ++m_inner;
                advance_to_next_valid();
                return *this;
            }

            basic_iterator operator++(int)
            {
                basic_iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            [[nodiscard]] bool operator==(const basic_iterator& other) const
            {
                if (m_map != other.m_map)
                    return false;
                if (m_shard_index != other.m_shard_index)
                    return false;
                if (m_shard_index >= (m_map ? m_map->m_shards.size() : 0))
                    return true;
                return m_inner == other.m_inner;
            }

            bool operator!=(const basic_iterator& other) const
            {
                return !(*this == other);
            }
        };

        template < typename Iterator >
        struct range
        {
            Iterator m_begin;
            Iterator m_end;
            Iterator begin()
            {
                return std::move(m_begin);
            }
            Iterator end()
            {
                return std::move(m_end);
            }
        };

      public:
        /**
         * Mutable forward iterator type for exclusive whole-map iteration.
         *
         * @note Iterators are only safe to use with the range returned by range_exclusive() while no other thread is
         * mutating the map.
         */
        using iterator = basic_iterator< false >;

        /**
         * Const forward iterator type for exclusive whole-map iteration.
         *
         * @note Iterators are only safe to use with the range returned by range_exclusive() while no other thread is
         * mutating the map.
         */
        using const_iterator = basic_iterator< true >;

        /**
         * Returns a mutable range over all shards without locking.
         *
         * @return A range that iterates over every element in the map.
         * @pre No other thread may mutate the map while the returned range or its iterators are used.
         * @note This method is intended for exclusive access phases such as setup, teardown, or single-threaded
         * inspection.
         */
        [[nodiscard]] range< iterator > range_exclusive()
        {
            return {iterator(this, 0, m_shards[0].m_map.begin()), iterator(this, m_shards.size(), {})};
        }

        /**
         * Returns a const range over all shards without locking.
         *
         * @return A range that iterates over every element in the map.
         * @pre No other thread may mutate the map while the returned range or its iterators are used.
         * @note This method is intended for exclusive access phases such as setup, teardown, or single-threaded
         * inspection.
         */
        [[nodiscard]] range< const_iterator > range_exclusive() const
        {
            return {const_iterator(this, 0, m_shards[0].m_map.begin()), const_iterator(this, m_shards.size(), {})};
        }

        /**
         * Estimates the number of elements in the map while locking each shard independently.
         *
         * @return The sum of the shard sizes observed during the call.
         * @note If no other threads are concurrently modifying the map, this method will always return the correct size.
         * @note This operation is thread-safe, but non-atomic. Because it works by non-atomically adding counts from
         * each shard, the returned value may be sequentially inconsistent if other threads are concurrently modifying
         * the map. For  example, if a thread removes an element from shard 0 and then adds one to shard 5, the
         * estimate_size() function may observe shard 0 before the removal and shard 5 after the insertion, resulting in
         * an estimated size that is larger than the actual size was at any point in time.
         */
        [[nodiscard]] std::size_t estimate_size() const
        {
            std::size_t total_size = 0;
            for (const auto& shard : m_shards)
            {
                std::lock_guard< std::mutex > lock(shard.get_mutex());
                total_size += shard.m_map.size();
            }
            return total_size;
        }

        /**
         * Returns the number of elements in the map without locking.
         *
         * @return The total number of elements.
         * @pre No other thread may mutate the map while this function runs.
         * @note Use estimate_size() when concurrent mutation is possible.
         */
        [[nodiscard]] std::size_t size_exclusive() const
        {
            std::size_t total_size = 0;
            for (const auto& shard : m_shards)
            {
                total_size += shard.m_map.size();
            }
            return total_size;
        }

        /**
         * Constructs a sharded map.
         *
         * @param shard_count Requested number of shards. If zero or not a power of two, it is rounded up to the next
         * power of two so shard selection can use a mask.
         * @param alloc Allocator used for the underlying unordered maps and rebound for shard storage.
         * @param hash Hash function used both for shard selection and within each shard.
         * @param key_equal Equality predicate used within each shard.
         */
        explicit conc_sharded_unordered_map(std::size_t shard_count = std::thread::hardware_concurrency() * 2, Alloc const& alloc = Alloc(), Hash const& hash = Hash(), KeyEqual const& key_equal = KeyEqual()) : m_hash(hash), m_key_equal(key_equal), m_shards((typename std::allocator_traits< Alloc >::template rebind_alloc< shard >)(alloc))
        {
            if (shard_count == 0 || (shard_count & (shard_count - 1)) != 0)
            {
                // Ensure shard_count is a power of two
                std::size_t power = 1;
                while (power < shard_count)
                {
                    power <<= 1;
                }
                shard_count = power;
            }
            m_shards.reserve(shard_count);
            for (std::size_t i = 0; i < shard_count; ++i)
            {
                m_shards.emplace_back(m_hash, m_key_equal, alloc);
            }
        }

        /**
         * Copy construction is disabled because shards contain mutexes and define synchronization ownership.
         */
        conc_sharded_unordered_map(conc_sharded_unordered_map const&) = delete;

        /**
         * Copy assignment is disabled because shards contain mutexes and define synchronization ownership.
         */
        conc_sharded_unordered_map& operator=(conc_sharded_unordered_map const&) = delete;

        /**
         * Move construction is disabled so references and shard synchronization state cannot be relocated.
         */
        conc_sharded_unordered_map(conc_sharded_unordered_map&&) = delete;

        /**
         * Move assignment is disabled so references and shard synchronization state cannot be relocated.
         */
        conc_sharded_unordered_map& operator=(conc_sharded_unordered_map&&) = delete;

        /**
         * Destroys the map and all stored elements.
         *
         * @pre No other thread may access the map or any reference obtained from it during destruction.
         */
        ~conc_sharded_unordered_map() = default;

        /**
         * Inserts or replaces the value for a key.
         *
         * @param key Key to update.
         * @param value Value to store.
         * @note This operation locks only the shard selected by the key.
         * @note Replacing an existing key modifies that key's stored Value. Other threads must not concurrently use a
         * reference to that Value unless Value provides its own synchronization.
         */
        void put(Key const& key, Value value)
        {
            std::size_t shard_index = m_hash(key) & (m_shards.size() - 1);
            shard& target_shard = m_shards[shard_index];
            std::lock_guard< std::mutex > lock(target_shard.get_mutex());
            target_shard.m_map[key] = std::move(value);
        }

        /**
         * Computes and stores a value for a key, replacing any existing value.
         *
         * @tparam Func Nullary callable type used to create the value.
         * @param key Key to update.
         * @param func Callable invoked while the target shard is locked.
         * @note The callable must not call back into this map for a key in the same shard, otherwise it may deadlock.
         * @note Replacing an existing key modifies that key's stored Value. Other threads must not concurrently use a
         * reference to that Value unless Value provides its own synchronization.
         */
        template < typename Func >
        void put_exec(Key const& key, Func func)
        {
            std::size_t shard_index = m_hash(key) & (m_shards.size() - 1);
            shard& target_shard = m_shards[shard_index];
            std::lock_guard< std::mutex > lock(target_shard.get_mutex());
            target_shard.m_map[key] = func();
        }

        /**
         * Computes and stores a value only if the key is absent.
         *
         * @tparam Func Nullary callable type used to create the value.
         * @param key Key to insert.
         * @param func Callable invoked while the target shard is locked if the key is absent.
         * @return true if a new value was inserted, otherwise false.
         * @note The callable must not call back into this map for a key in the same shard, otherwise it may deadlock.
         */
        template < typename Func >
        bool try_put_exec(Key const& key, Func func)
        {
            std::size_t shard_index = m_hash(key) & (m_shards.size() - 1);
            shard& target_shard = m_shards[shard_index];
            std::lock_guard< std::mutex > lock(target_shard.get_mutex());
            if (target_shard.m_map.find(key) != target_shard.m_map.end())
            {
                return false;
            }
            target_shard.m_map[key] = func();
            return true;
        }

        /**
         * Returns the value for a key, creating it if missing.
         *
         * @tparam Func Nullary callable type used to create the value when the key is absent.
         * @param key Key to look up or insert.
         * @param func Callable invoked while the target shard is locked if the key is absent.
         * @return A reference to the stored value.
         * @note The callable must not call back into this map for a key in the same shard, otherwise it may deadlock.
         * @note The shard lock is released before this function returns. Concurrent insertions of other keys do not
         * invalidate the returned reference, but erasing this key, destroying the map, or concurrently modifying the
         * same value while the reference is in use is not allowed unless Value provides its own synchronization.
         */
        template < typename Func >
        Value& get_or_create(Key const& key, Func func)
        {
            std::size_t shard_index = m_hash(key) & (m_shards.size() - 1);
            shard& target_shard = m_shards[shard_index];
            std::lock_guard< std::mutex > lock(target_shard.get_mutex());
            if (auto it = target_shard.m_map.find(key); it != target_shard.m_map.end())
            {
                return it->second;
            }
            else
            {
                auto [it2, inserted] = target_shard.m_map.emplace(key, func());
                return it2->second;
            }
        }

        /**
         * Returns the value for a key, default-constructing and initializing it if missing.
         *
         * @tparam Func Callable type invoked as func(Value&) when the key is absent.
         * @param key Key to look up or insert.
         * @param func Callable invoked while the target shard is locked to initialize a new value.
         * @return A reference to the stored value.
         * @note If func throws, the newly inserted value is erased before the exception is rethrown.
         * @note The callable must not call back into this map for a key in the same shard, otherwise it may deadlock.
         * @note The shard lock is released before this function returns. Concurrent insertions of other keys do not
         * invalidate the returned reference, but erasing this key, destroying the map, or concurrently modifying the
         * same value while the reference is in use is not allowed unless Value provides its own synchronization.
         */
        template < typename Func >
        Value& get_or_init(Key const& key, Func func)
        {
            std::size_t shard_index = m_hash(key) & (m_shards.size() - 1);
            shard& target_shard = m_shards[shard_index];
            std::lock_guard< std::mutex > lock(target_shard.get_mutex());
            if (auto it = target_shard.m_map.find(key); it != target_shard.m_map.end())
            {
                return it->second;
            }
            else
            {
                try
                {
                    auto& val = target_shard.m_map[key];
                    func(val);
                    return val;
                }
                catch (...)
                {
                    target_shard.m_map.erase(key);
                    throw;
                }
            }
        }

        /**
         * Returns the value for a key, default-constructing and initializing it with key/value access if missing.
         *
         * @tparam Func Callable type invoked as func(Key const&, Value&) when the key is absent.
         * @param key Key to look up or insert.
         * @param func Callable invoked while the target shard is locked to initialize a new value.
         * @return A reference to the stored value.
         * @note If func throws, the newly inserted value is erased before the exception is rethrown.
         * @note The callable must not call back into this map for a key in the same shard, otherwise it may deadlock.
         * @note The shard lock is released before this function returns. Concurrent insertions of other keys do not
         * invalidate the returned reference, but erasing this key, destroying the map, or concurrently modifying the
         * same value while the reference is in use is not allowed unless Value provides its own synchronization.
         */
        template < typename Func >
        Value& get_or_init_iter(Key const& key, Func func)
        {
            std::size_t shard_index = m_hash(key) & (m_shards.size() - 1);
            shard& target_shard = m_shards[shard_index];
            std::lock_guard< std::mutex > lock(target_shard.get_mutex());
            if (auto it = target_shard.m_map.find(key); it != target_shard.m_map.end())
            {
                return it->second;
            }
            else
            {
                try
                {
                    auto val = target_shard.m_map.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple());
                    func(val.first->first, val.first->second);
                    return val.first->second;
                }
                catch (...)
                {
                    target_shard.m_map.erase(key);
                    throw;
                }
            }
        }

        /**
         * Inserts a value only if the key is absent.
         *
         * @param key Key to insert.
         * @param value Value to store if the key is absent.
         * @return true if a new value was inserted, otherwise false.
         * @note This operation locks only the shard selected by the key.
         */
        bool try_put(Key const& key, Value value)
        {
            std::size_t shard_index = m_hash(key) & (m_shards.size() - 1);
            shard& target_shard = m_shards[shard_index];
            std::lock_guard< std::mutex > lock(target_shard.get_mutex());
            auto [it, inserted] = target_shard.m_map.emplace(key, std::move(value));
            return inserted;
        }

        /**
         * Returns a copy of the value for a key.
         *
         * @param key Key to look up.
         * @return A copy of the stored value.
         * @throws std::out_of_range if the key is absent.
         * @note This operation locks only the shard selected by the key. The returned copy is independent of subsequent
         * map operations.
         */
        Value get(Key const& key)
        {
            std::size_t shard_index = m_hash(key) & (m_shards.size() - 1);
            shard& target_shard = m_shards[shard_index];
            std::lock_guard< std::mutex > lock(target_shard.get_mutex());
            return target_shard.m_map.at(key);
        }

        /**
         * Erases a key from the map.
         *
         * @param key Key to erase if present.
         * @note Any reference, pointer, or iterator to the erased element is invalidated. Callers must ensure no other
         * thread is still using a reference returned by get_or_create(), get_or_init(), or get_or_init_iter() for this
         * key.
         */
        void erase(Key const& key)
        {
            std::size_t shard_index = m_hash(key) & (m_shards.size() - 1);
            shard& target_shard = m_shards[shard_index];
            std::lock_guard< std::mutex > lock(target_shard.get_mutex());
            target_shard.m_map.erase(key);
        }
    };
} // namespace rpnx

#endif // RPNXDATASTRUCTURES_SHARDED_MAP_HPP
