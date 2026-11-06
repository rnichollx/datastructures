// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0

#ifndef RPNXDATASTRUCTURES_BENCHMARKS_CONCURRENT_MAPS_HPP
#define RPNXDATASTRUCTURES_BENCHMARKS_CONCURRENT_MAPS_HPP

#include <benchmark/benchmark.h>
#include <rpnx/concurrent_hash_trie.hpp>
#include <rpnx/insert_only_concurrent_hash_trie.hpp>
#include <rpnx/sharded_unordered_map.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace rpnx_benchmarks
{
    constexpr std::size_t concurrent_map_keys = 65536;
    constexpr std::size_t concurrent_map_batch = 131072;

    /** @brief Spread adjacent integer keys across the entire 64-bit hash space. */
    struct concurrent_map_hash
    {
        /** @brief Hash an integer key with a fixed reversible mix. */
        std::uint64_t operator()(std::uint64_t key) const noexcept
        {
            key += 0x9e3779b97f4a7c15ULL;
            key = (key ^ (key >> 30)) * 0xbf58476d1ce4e5b9ULL;
            key = (key ^ (key >> 27)) * 0x94d049bb133111ebULL;
            return key ^ (key >> 31);
        }
    };

    /** @brief Adapt the concurrent trie to the benchmark's value operations. */
    struct trie_map_operations
    {
        using map_type = rpnx::concurrent_hash_trie< std::uint64_t, std::uint64_t, concurrent_map_hash >;

        /** @brief Read an immutable value or calculate and insert it on a miss. */
        static std::uint64_t access(map_type& map, std::uint64_t key, std::size_t& inserted)
        {
            if (map_type::pointer entry = map.find(key))
            {
                return entry->second;
            }
            auto result = map.try_emplace(key, concurrent_map_hash{}(key));
            inserted += result.second;
            return result.first->second;
        }

        /** @brief Insert a value if its key is absent. */
        static void insert(map_type& map, std::uint64_t key)
        {
            map.try_emplace(key, key);
        }

        /** @brief Copy a stored value under the trie's read lock. */
        static std::uint64_t read(map_type& map, std::uint64_t key)
        {
            std::uint64_t value = 0;
            map.with_read(key, [&](map_type::const_reference entry) { value = entry.second; });
            return value;
        }

        /** @brief Replace a stored value under the trie's write lock. */
        static void update(map_type& map, std::uint64_t key)
        {
            map.insert_or_assign(key, key);
        }

        /** @brief Remove a stored key. */
        static void erase(map_type& map, std::uint64_t key)
        {
            map.erase(key);
        }
    };

    /** @brief Adapt the sharded map to the benchmark's value operations. */
    struct sharded_map_operations
    {
        using map_type = rpnx::conc_sharded_unordered_map< std::uint64_t, std::uint64_t, concurrent_map_hash >;

        /** @brief Read an immutable value or calculate and insert it on a miss. */
        static std::uint64_t access(map_type& map, std::uint64_t key, std::size_t& inserted)
        {
            return map.get_or_create(key, [&]
            {
                ++inserted;
                return concurrent_map_hash{}(key);
            });
        }

        /** @brief Insert a value if its key is absent. */
        static void insert(map_type& map, std::uint64_t key)
        {
            map.try_put(key, key);
        }

        /** @brief Copy a stored value under its shard lock. */
        static std::uint64_t read(map_type& map, std::uint64_t key)
        {
            return map.get(key);
        }

        /** @brief Replace a stored value under its shard lock. */
        static void update(map_type& map, std::uint64_t key)
        {
            map.put(key, key);
        }

        /** @brief Remove a stored key. */
        static void erase(map_type& map, std::uint64_t key)
        {
            map.erase(key);
        }
    };

    /** @brief Adapt the insertion-only trie to mixed access workloads. */
    struct insert_only_map_operations
    {
        using map_type = rpnx::insert_only_concurrent_hash_trie< std::uint64_t, std::uint64_t, concurrent_map_hash >;

        /** @brief Read a value or calculate it once when absent. */
        static std::uint64_t access(map_type& map, std::uint64_t key, std::size_t& inserted)
        {
            return map.get_or_create(key, [&]
            {
                ++inserted;
                return concurrent_map_hash{}(key);
            });
        }
    };

    /** @brief Hold the map shared by every thread in one benchmark run. */
    template < typename Operations >
    struct concurrent_map_context
    {
        std::unique_ptr< typename Operations::map_type > map;
    };

    /** @brief Map pseudorandom draws uniformly onto the key domain. */
    struct uniform_map_indices
    {
        /** @brief Select a key from a domain whose size is a power of two. */
        static std::uint64_t select(std::uint64_t draw, std::uint64_t domain)
        {
            return draw & (domain - 1);
        }
    };

    /** @brief Direct 90 percent of accesses to approximately one percent of keys. */
    struct hot_map_indices
    {
        /** @brief Select a key from the hot subset or the full domain. */
        static std::uint64_t select(std::uint64_t draw, std::uint64_t domain)
        {
            return draw % 10 < 9 ? (draw >> 8) % (domain / 100) : (draw >> 8) & (domain - 1);
        }
    };

    /** @brief Execute a fixed mixed read and insert trace against an initially empty map. */
    template < typename Operations, typename Distribution >
    void concurrent_map_mixed(benchmark::State& state, concurrent_map_context< Operations >& context)
    {
        constexpr std::size_t accesses = 1048576;
        std::size_t first = accesses * static_cast< std::size_t >(state.thread_index()) / static_cast< std::size_t >(state.threads());
        std::size_t last = accesses * static_cast< std::size_t >(state.thread_index() + 1) / static_cast< std::size_t >(state.threads());
        std::size_t inserted = 0;
        bool valid = true;
        for (auto unused : state)
        {
            for (std::size_t index = first; index < last; ++index)
            {
                std::uint64_t draw = concurrent_map_hash{}(index);
                std::uint64_t key = Distribution::select(draw, static_cast< std::uint64_t >(state.range(0)));
                std::uint64_t value = Operations::access(*context.map, key, inserted);
                valid &= value == concurrent_map_hash{}(key);
                benchmark::DoNotOptimize(value);
            }
        }
        if (!valid)
        {
            state.SkipWithError("Mixed access returned an incorrect calculated value");
        }
        state.SetItemsProcessed(state.iterations() * (last - first));
        state.counters["inserted"] = static_cast< double >(inserted);
    }

    /** @brief Register mixed traces with fixed domains and per-run empty maps. */
    template < typename Operations, typename Distribution >
    void register_mixed_map_benchmarks(std::string name)
    {
        std::shared_ptr< concurrent_map_context< Operations > > context = std::make_shared< concurrent_map_context< Operations > >();
        benchmark::RegisterBenchmark(name, [context](benchmark::State& state) { concurrent_map_mixed< Operations, Distribution >(state, *context); })
            ->Setup([context](benchmark::State const&) { context->map = std::make_unique< typename Operations::map_type >(); })
            ->Teardown([context](benchmark::State const&) { context->map.reset(); })
            ->Arg(65536)->Arg(1048576)->Iterations(1)
            ->Threads(1)->ThreadPerCpu()->UseRealTime();
    }

    /** @brief Read existing keys from a map populated before measurement. */
    template < typename Operations >
    void concurrent_map_read(benchmark::State& state, concurrent_map_context< Operations >& context)
    {
        std::uint64_t sequence = static_cast< std::uint64_t >(state.thread_index()) * 8191;
        for (auto unused : state)
        {
            std::uint64_t key = (sequence++ * 0x9e3779b97f4a7c15ULL) & (concurrent_map_keys - 1);
            std::uint64_t value = Operations::read(*context.map, key);
            benchmark::DoNotOptimize(value);
        }
        state.SetItemsProcessed(state.iterations());
    }

    /** @brief Replace existing values in a map populated before measurement. */
    template < typename Operations >
    void concurrent_map_update(benchmark::State& state, concurrent_map_context< Operations >& context)
    {
        std::uint64_t sequence = static_cast< std::uint64_t >(state.thread_index()) * 8191;
        for (auto unused : state)
        {
            std::uint64_t key = (sequence++ * 0x9e3779b97f4a7c15ULL) & (concurrent_map_keys - 1);
            Operations::update(*context.map, key);
        }
        state.SetItemsProcessed(state.iterations());
    }

    /** @brief Insert a fixed total number of disjoint keys into a fresh map. */
    template < typename Operations >
    void concurrent_map_insert(benchmark::State& state, concurrent_map_context< Operations >& context)
    {
        std::size_t first = concurrent_map_batch * static_cast< std::size_t >(state.thread_index()) / static_cast< std::size_t >(state.threads());
        std::size_t last = concurrent_map_batch * static_cast< std::size_t >(state.thread_index() + 1) / static_cast< std::size_t >(state.threads());
        for (auto unused : state)
        {
            for (std::size_t key = first; key < last; ++key)
            {
                Operations::insert(*context.map, key);
            }
        }
        state.SetItemsProcessed(state.iterations() * (last - first));
    }

    /** @brief Erase disjoint keys from a map populated before measurement. */
    template < typename Operations >
    void concurrent_map_erase(benchmark::State& state, concurrent_map_context< Operations >& context)
    {
        std::size_t first = concurrent_map_batch * static_cast< std::size_t >(state.thread_index()) / static_cast< std::size_t >(state.threads());
        std::size_t last = concurrent_map_batch * static_cast< std::size_t >(state.thread_index() + 1) / static_cast< std::size_t >(state.threads());
        for (auto unused : state)
        {
            for (std::size_t key = first; key < last; ++key)
            {
                Operations::erase(*context.map, key);
            }
        }
        state.SetItemsProcessed(state.iterations() * (last - first));
    }

    /** @brief Register comparable read, update, insert, and erase workloads. */
    template < typename Operations >
    void register_concurrent_map_benchmarks(std::string name)
    {
        std::shared_ptr< concurrent_map_context< Operations > > read_context = std::make_shared< concurrent_map_context< Operations > >();
        benchmark::RegisterBenchmark(name + "/read", [read_context](benchmark::State& state) { concurrent_map_read< Operations >(state, *read_context); })
            ->Setup([read_context](benchmark::State const&)
                    {
                        read_context->map = std::make_unique< typename Operations::map_type >();
                        for (std::size_t key = 0; key < concurrent_map_keys; ++key)
                        {
                            Operations::insert(*read_context->map, key);
                        }
                    })
            ->Teardown([read_context](benchmark::State const&) { read_context->map.reset(); })
            ->ThreadRange(1, benchmark::CPUInfo::Get().num_cpus)->UseRealTime();

        std::shared_ptr< concurrent_map_context< Operations > > update_context = std::make_shared< concurrent_map_context< Operations > >();
        benchmark::RegisterBenchmark(name + "/update", [update_context](benchmark::State& state) { concurrent_map_update< Operations >(state, *update_context); })
            ->Setup([update_context](benchmark::State const&)
                    {
                        update_context->map = std::make_unique< typename Operations::map_type >();
                        for (std::size_t key = 0; key < concurrent_map_keys; ++key)
                        {
                            Operations::insert(*update_context->map, key);
                        }
                    })
            ->Teardown([update_context](benchmark::State const&) { update_context->map.reset(); })
            ->ThreadRange(1, benchmark::CPUInfo::Get().num_cpus)->UseRealTime();

        std::shared_ptr< concurrent_map_context< Operations > > insert_context = std::make_shared< concurrent_map_context< Operations > >();
        benchmark::RegisterBenchmark(name + "/insert", [insert_context](benchmark::State& state) { concurrent_map_insert< Operations >(state, *insert_context); })
            ->Setup([insert_context](benchmark::State const&) { insert_context->map = std::make_unique< typename Operations::map_type >(); })
            ->Teardown([insert_context](benchmark::State const&) { insert_context->map.reset(); })
            ->Iterations(1)->ThreadRange(1, benchmark::CPUInfo::Get().num_cpus)->UseRealTime();

        std::shared_ptr< concurrent_map_context< Operations > > erase_context = std::make_shared< concurrent_map_context< Operations > >();
        benchmark::RegisterBenchmark(name + "/erase", [erase_context](benchmark::State& state) { concurrent_map_erase< Operations >(state, *erase_context); })
            ->Setup([erase_context](benchmark::State const&)
                    {
                        erase_context->map = std::make_unique< typename Operations::map_type >();
                        for (std::size_t key = 0; key < concurrent_map_batch; ++key)
                        {
                            Operations::insert(*erase_context->map, key);
                        }
                    })
            ->Teardown([erase_context](benchmark::State const&) { erase_context->map.reset(); })
            ->Iterations(1)->ThreadRange(1, benchmark::CPUInfo::Get().num_cpus)->UseRealTime();
    }

} // namespace rpnx_benchmarks

#endif // RPNXDATASTRUCTURES_BENCHMARKS_CONCURRENT_MAPS_HPP
