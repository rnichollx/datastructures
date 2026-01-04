// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>

#ifndef RPNXDATASTRUCTURES_SHARDED_MAP_HPP
#define RPNXDATASTRUCTURES_SHARDED_MAP_HPP

#include <thread>
#include <unordered_map>
#include <vector>
#include <new>

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

            std::mutex& get_mutex()
            {
                return *std::launder(reinterpret_cast<std::mutex*>(&m_mutex));
            }

            shard(Alloc alloc)
                : m_map(0, Hash(), KeyEqual(), alloc)
            {
            }
        };

        std::vector<shard, typename std::allocator_traits<Alloc>::template rebind_alloc<shard>> m_shards;

    public:
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

        std::size_t size() const
        {
            std::size_t total_size = 0;
            for (auto& shard : m_shards)
            {
                std::lock_guard<std::mutex> lock(shard.get_mutex());
                total_size += shard.m_map.size();
            }
            return total_size;
        }
    };
}

#endif //RPNXDATASTRUCTURES_SHARDED_MAP_HPP
