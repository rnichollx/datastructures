// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>

#ifndef RPNXDATASTRUCTURES_SHARDED_MAP_HPP
#define RPNXDATASTRUCTURES_SHARDED_MAP_HPP

#include <thread>
#include <unordered_map>
#include <vector>
#include <new>
#include <mutex>
#include <iterator>
#include <type_traits>

namespace rpnx
{
    template <typename Key, typename Value, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>,
              typename Alloc = std::allocator<std::pair<const Key, Value>>>
    class conc_sharded_unordered_map
    {
        struct alignas(std::hardware_destructive_interference_size) shard
        {
            std::aligned_storage_t<sizeof(std::mutex), alignof(std::mutex)> m_mutex;
            std::unordered_map<Key, Value, Hash, KeyEqual, Alloc> m_map;

            std::mutex& get_mutex() const
            {
                return *std::launder(reinterpret_cast<std::mutex*>(const_cast<std::aligned_storage_t<sizeof(std::mutex), alignof(std::mutex)>*>(&m_mutex)));
            }

            shard(Alloc alloc)
                : m_map(0, Hash(), KeyEqual(), alloc)
            {
            }
        };

        std::vector<shard, typename std::allocator_traits<Alloc>::template rebind_alloc<shard>> m_shards;

        template <bool IsConst>
        class basic_iterator
        {
            using map_type = std::conditional_t<IsConst, const conc_sharded_unordered_map, conc_sharded_unordered_map>;
            using shard_type = std::conditional_t<IsConst, const shard, shard>;
            using inner_iterator = std::conditional_t<IsConst, typename std::unordered_map<Key, Value, Hash, KeyEqual, Alloc>::const_iterator, typename std::unordered_map<Key, Value, Hash, KeyEqual, Alloc>::iterator>;

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
            using value_type = std::pair<const Key, Value>;
            using difference_type = std::ptrdiff_t;
            using pointer = std::conditional_t<IsConst, const value_type*, value_type*>;
            using reference = std::conditional_t<IsConst, const value_type&, value_type&>;

            basic_iterator() : m_map(nullptr), m_shard_index(0), m_inner() {}
            basic_iterator(map_type* map, std::size_t shard_index, inner_iterator inner)
                : m_map(map), m_shard_index(shard_index), m_inner(inner)
            {
                if (m_map && m_shard_index < m_map->m_shards.size())
                {
                    advance_to_next_valid();
                }
            }

            reference operator*() const { return *m_inner; }
            pointer operator->() const { return &(*m_inner); }

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
                if (m_map != other.m_map) return false;
                if (m_shard_index != other.m_shard_index) return false;
                if (m_shard_index >= (m_map ? m_map->m_shards.size() : 0)) return true;
                return m_inner == other.m_inner;
            }

            bool operator!=(const basic_iterator& other) const { return !(*this == other); }
        };


        template <typename Iterator>
        struct range
        {
            Iterator m_begin;
            Iterator m_end;
            Iterator begin() { return std::move(m_begin); }
            Iterator end() { return std::move(m_end); }
        };

    public:
        using iterator = basic_iterator<false>;
        using const_iterator = basic_iterator<true>;

        /**
         * A thread-unsafe method to get a range that can iterate over the entire conc_sharded_unordered_map.
         * @note This method is not thread-safe and should only be used when no other threads are mutating the map.
         * @return A range that can iterate over the conc_sharded_unordered_map.
         */
        [[nodiscard]] range<iterator> range_exclusive()
        {
            return {iterator(this, 0, m_shards[0].m_map.begin()), iterator(this, m_shards.size(), {})};
        }

        /**
         * A thread-unsafe method to get a range that can iterate over the entire conc_sharded_unordered_map.
         * @note This method is not thread-safe and should only be used when no other threads are mutating the map.
         * @return A const range that can iterate over the conc_sharded_unordered_map.
         */
        [[nodiscard]] range<const_iterator> range_exclusive() const
        {
            return {const_iterator(this, 0, m_shards[0].m_map.begin()), const_iterator(this, m_shards.size(), {})};
        }

        /**
         * Estimates the number of elements in the map.
         * @return The total number of elements.
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
                std::lock_guard<std::mutex> lock(shard.get_mutex());
                total_size += shard.m_map.size();
            }
            return total_size;
        }

        /**
         * Returns the number of elements in the map without locking.
         * @note This method is not thread-safe and should only be used when no other threads are mutating the map.
         * @return The total number of elements.
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

        explicit conc_sharded_unordered_map(std::size_t shard_count = std::thread::hardware_concurrency() * 2,
                                            Alloc const& alloc = Alloc())
            : m_shards((typename std::allocator_traits<Alloc>::template rebind_alloc<shard>)(alloc))
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
                m_shards.emplace_back(alloc);
                new(&m_shards[i].m_mutex) std::mutex();
            }
        }

        ~conc_sharded_unordered_map()
        {
            for (auto& shard : m_shards)
            {
                shard.get_mutex().~mutex();
            }
        }

        void put(Key const& key, Value value)
        {
            std::size_t shard_index = Hash{}(key) & (m_shards.size() - 1);
            shard& target_shard = m_shards[shard_index];
            std::lock_guard<std::mutex> lock(target_shard.get_mutex());
            target_shard.m_map[key] = std::move(value);
        }

        template <typename Func>
        void put_exec(Key const& key, Func func)
        {
            std::size_t shard_index = Hash{}(key) & (m_shards.size() - 1);
            shard& target_shard = m_shards[shard_index];
            std::lock_guard<std::mutex> lock(target_shard.get_mutex());
            target_shard.m_map[key] = func();
        }

        template <typename Func>
        bool try_put_exec(Key const& key, Func func)
        {
            std::size_t shard_index = Hash{}(key) & (m_shards.size() - 1);
            shard& target_shard = m_shards[shard_index];
            std::lock_guard<std::mutex> lock(target_shard.get_mutex());
            if (target_shard.m_map.find(key) != target_shard.m_map.end())
            {
                return false;
            }
            target_shard.m_map[key] = func();
            return true;
        }

        template <typename Func>
        Value& get_or_create(Key const& key, Func func)
        {
            std::size_t shard_index = Hash{}(key) & (m_shards.size() - 1);
            shard& target_shard = m_shards[shard_index];
            std::lock_guard<std::mutex> lock(target_shard.get_mutex());
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

        template <typename Func>
        Value& get_or_init(Key const& key, Func func)
        {
            std::size_t shard_index = Hash{}(key) & (m_shards.size() - 1);
            shard& target_shard = m_shards[shard_index];
            std::lock_guard<std::mutex> lock(target_shard.get_mutex());
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

        template <typename Func>
        Value& get_or_init_iter(Key const& key, Func func)
        {
            std::size_t shard_index = Hash{}(key) & (m_shards.size() - 1);
            shard& target_shard = m_shards[shard_index];
            std::lock_guard<std::mutex> lock(target_shard.get_mutex());
            if (auto it = target_shard.m_map.find(key); it != target_shard.m_map.end())
            {
                return it->second;
            }
            else
            {
                try
                {
                    auto& val = target_shard.m_map.insert(key);
                    func(val.first, val.second);
                    return val;
                }
                catch (...)
                {
                    target_shard.m_map.erase(key);
                    throw;
                }
            }
        }

        bool try_put(Key const& key, Value value)
        {
            std::size_t shard_index = Hash{}(key) & (m_shards.size() - 1);
            shard& target_shard = m_shards[shard_index];
            std::lock_guard<std::mutex> lock(target_shard.get_mutex());
            auto [it, inserted] = target_shard.m_map.emplace(key, std::move(value));
            return inserted;
        }

        Value get(Key const& key)
        {
            std::size_t shard_index = Hash{}(key) & (m_shards.size() - 1);
            shard& target_shard = m_shards[shard_index];
            std::lock_guard<std::mutex> lock(target_shard.get_mutex());
            return target_shard.m_map.at(key);
        }

        void erase(Key const& key)
        {
            std::size_t shard_index = Hash{}(key) & (m_shards.size() - 1);
            shard& target_shard = m_shards[shard_index];
            std::lock_guard<std::mutex> lock(target_shard.get_mutex());
            target_shard.m_map.erase(key);
        }

    };
}

#endif //RPNXDATASTRUCTURES_SHARDED_MAP_HPP
