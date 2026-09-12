// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0
#ifndef RPNX_BENCHMARKS_HADIX_MAP_HPP
#define RPNX_BENCHMARKS_HADIX_MAP_HPP

#include <algorithm>
#include <benchmark/benchmark.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <random>
#include <rpnx/hadix_map.hpp>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rpnx_benchmarks
{
    /** @brief Returns distinct, well-distributed keys through a reversible integer permutation. */
    inline std::uint64_t hadix_benchmark_key(std::uint64_t index) noexcept
    {
        std::uint64_t value = index + UINT64_C(0x9e3779b97f4a7c15);
        value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
        return value ^ (value >> 31);
    }

    /** @brief Creates full hash collisions while preserving the required result width. */
    struct hadix_constant_hash
    {
        /** @brief Returns the same hash for every key. */
        std::size_t operator()(std::uint64_t) const noexcept
        {
            return 0;
        }
    };

    /** @brief Owns reproducible insertion, successful lookup and unsuccessful lookup sequences. */
    struct hadix_workload
    {
        std::vector< std::uint64_t > insertion;
        std::vector< std::uint64_t > hits;
        std::vector< std::uint64_t > misses;

        /** @brief Generates a population and a bounded set of distinct, shuffled hits and misses. */
        hadix_workload(std::size_t count, std::size_t query_count)
        {
            query_count = std::min(count, query_count);
            insertion.reserve(count);
            misses.reserve(query_count);
            for (std::size_t i = 0; i < count; ++i)
            {
                insertion.push_back(hadix_benchmark_key(i));
            }
            for (std::size_t i = 0; i < query_count; ++i)
            {
                misses.push_back(hadix_benchmark_key(count + i));
            }
            hits.assign(insertion.begin(), insertion.begin() + query_count);
            std::mt19937_64 random(809173);
            std::shuffle(insertion.begin(), insertion.end(), random);
            std::shuffle(hits.begin(), hits.end(), random);
            std::shuffle(misses.begin(), misses.end(), random);
        }
    };

    /**
     * @brief Measures insertion from empty with natural growth and no reservation.
     * Key generation and container destruction are excluded; allocation and all
     * insertion-triggered splits or rehashes are included in the timing.
     */
    template < typename Map >
    void hadix_insert(benchmark::State& state)
    {
        std::size_t count = static_cast< std::size_t >(state.range(0));
        hadix_workload workload(count, count);
        for (auto iteration : state)
        {
            state.PauseTiming();
            {
                Map values;
                state.ResumeTiming();
                for (std::uint64_t key : workload.insertion)
                {
                    values.try_emplace(key, key);
                }
                benchmark::DoNotOptimize(values);
                benchmark::ClobberMemory();
                state.PauseTiming();
            }
            state.ResumeTiming();
        }
        state.SetItemsProcessed(state.iterations() * static_cast< std::int64_t >(count));
    }

    /** @brief Measures shuffled successful or unsuccessful lookups, excluding map construction. */
    template < typename Map, bool Hit >
    void hadix_lookup(benchmark::State& state)
    {
        std::size_t count = static_cast< std::size_t >(state.range(0));
        hadix_workload workload(count, count);
        Map values;
        for (std::uint64_t key : workload.insertion)
        {
            values.try_emplace(key, key);
        }
        std::vector< std::uint64_t > const& queries = Hit ? workload.hits : workload.misses;
        Map const& lookup = values;
        for (auto iteration : state)
        {
            benchmark::DoNotOptimize(values);
            std::uint64_t sum = 0;
            for (std::uint64_t key : queries)
            {
                typename Map::const_iterator found = lookup.find(key);
                if (found != lookup.end())
                {
                    sum += found->second;
                }
            }
            benchmark::DoNotOptimize(sum);
        }
        state.SetItemsProcessed(state.iterations() * static_cast< std::int64_t >(count));
    }

    /**
     * @brief Measures erasing every key in shuffled order from a naturally grown map.
     * Population and final destruction are excluded. Deallocation and all
     * deletion-triggered bucket joins are included in the timing.
     */
    template < typename Map >
    void hadix_erase(benchmark::State& state)
    {
        std::size_t count = static_cast< std::size_t >(state.range(0));
        hadix_workload workload(count, count);
        for (auto iteration : state)
        {
            state.PauseTiming();
            {
                Map values;
                for (std::uint64_t key : workload.insertion)
                {
                    values.try_emplace(key, key);
                }
                state.ResumeTiming();
                for (std::uint64_t key : workload.hits)
                {
                    values.erase(key);
                }
                benchmark::DoNotOptimize(values);
                benchmark::ClobberMemory();
                state.PauseTiming();
            }
            state.ResumeTiming();
        }
        state.SetItemsProcessed(state.iterations() * static_cast< std::int64_t >(count));
    }

    /** @brief Selects the operation whose individual latency is sampled. */
    enum class hadix_operation { insert, find_hit, find_miss, erase };

    /**
     * @brief Measures each of four operations for five seconds on one prepopulated map.
     * Each batch begins with the requested number of keys. Batches advance through
     * the key population and wrap at its end, shuffling queries outside timing.
     * Restoration, population, key generation and destruction are excluded from the
     * five-second budget. The final batch completes even if it exceeds the budget.
     * Counters report mean nanoseconds, total operations and measured seconds for
     * each phase; construction_seconds reports the initial population cost separately.
     */
    template < typename Map >
    void hadix_sampled(benchmark::State& state)
    {
        std::size_t count = static_cast< std::size_t >(state.range(0));
        hadix_workload workload(count, static_cast< std::size_t >(state.range(1)));
        Map values;
        std::chrono::steady_clock::time_point population_start = std::chrono::steady_clock::now();
        for (std::uint64_t key : workload.insertion)
        {
            values.try_emplace(key, key);
        }
        state.counters["construction_seconds"] = std::chrono::duration< double >(std::chrono::steady_clock::now() - population_start).count();
        state.counters["initial_keys"] = static_cast< double >(count);
        state.counters["batch_keys"] = static_cast< double >(workload.hits.size());
        std::size_t total_operations = 0;
        for (auto iteration : state)
        {
            auto measure = []< hadix_operation Operation >(Map& current, std::vector< std::uint64_t >& queries, std::size_t population)
            {
                Map const& lookup = current;
                double seconds = 0;
                std::size_t operations = 0;
                std::size_t cursor = 0;
                std::mt19937_64 random(809174);
                while (seconds < 5.0)
                {
                    std::size_t key_offset = Operation == hadix_operation::insert || Operation == hadix_operation::find_miss ? population : 0;
                    for (std::size_t i = 0; i < queries.size(); ++i)
                    {
                        queries[i] = hadix_benchmark_key(key_offset + (cursor + i) % population);
                    }
                    std::shuffle(queries.begin(), queries.end(), random);
                    std::uint64_t sum = 0;
                    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
                    for (std::uint64_t key : queries)
                    {
                        if constexpr (Operation == hadix_operation::insert)
                        {
                            current.try_emplace(key, key);
                        }
                        else if constexpr (Operation == hadix_operation::erase)
                        {
                            sum += current.erase(key);
                        }
                        else
                        {
                            typename Map::const_iterator found = lookup.find(key);
                            if (found != lookup.end())
                            {
                                sum += found->second;
                            }
                        }
                    }
                    benchmark::DoNotOptimize(sum);
                    benchmark::DoNotOptimize(current);
                    benchmark::ClobberMemory();
                    seconds += std::chrono::duration< double >(std::chrono::steady_clock::now() - start).count();
                    operations += queries.size();
                    if constexpr (Operation == hadix_operation::insert)
                    {
                        for (std::uint64_t key : queries)
                        {
                            current.erase(key);
                        }
                    }
                    else if constexpr (Operation == hadix_operation::erase)
                    {
                        for (std::uint64_t key : queries)
                        {
                            current.try_emplace(key, key);
                        }
                    }
                    cursor = (cursor + queries.size()) % population;
                }
                return std::pair{seconds, operations};
            };
            auto [hit_seconds, hit_samples] = measure.template operator()< hadix_operation::find_hit >(values, workload.hits, count);
            auto [miss_seconds, miss_samples] = measure.template operator()< hadix_operation::find_miss >(values, workload.misses, count);
            auto [insert_seconds, insert_samples] = measure.template operator()< hadix_operation::insert >(values, workload.misses, count);
            auto [erase_seconds, erase_samples] = measure.template operator()< hadix_operation::erase >(values, workload.hits, count);
            if (values.size() != count)
            {
                state.SkipWithError("Map population changed after restoring measured batches");
                return;
            }
            state.counters["find_hit_ns"] += hit_seconds * 1e9 / hit_samples;
            state.counters["find_miss_ns"] += miss_seconds * 1e9 / miss_samples;
            state.counters["insert_ns"] += insert_seconds * 1e9 / insert_samples;
            state.counters["erase_ns"] += erase_seconds * 1e9 / erase_samples;
            state.counters["find_hit_seconds"] += hit_seconds;
            state.counters["find_miss_seconds"] += miss_seconds;
            state.counters["insert_seconds"] += insert_seconds;
            state.counters["erase_seconds"] += erase_seconds;
            state.counters["find_hit_samples"] += static_cast< double >(hit_samples);
            state.counters["find_miss_samples"] += static_cast< double >(miss_samples);
            state.counters["insert_samples"] += static_cast< double >(insert_samples);
            state.counters["erase_samples"] += static_cast< double >(erase_samples);
            total_operations += hit_samples + miss_samples + insert_samples + erase_samples;
            state.SetIterationTime(hit_seconds + miss_seconds + insert_seconds + erase_seconds);
        }
        for (char const* counter : {"find_hit_ns", "find_miss_ns", "insert_ns", "erase_ns"})
        {
            state.counters[counter] = state.counters[counter] / static_cast< double >(state.iterations());
        }
        state.SetItemsProcessed(static_cast< std::int64_t >(total_operations));
    }

    /**
     * @brief Samples every operation in a pass, including natural split/join and rehash boundaries.
     * Setup, sample storage and destruction are outside the manual timing intervals.
     * Samples include clock overhead and scheduling effects. An observed maximum
     * describes this run and does not establish a worst-case execution-time bound.
     */
    template < typename Map, hadix_operation Operation >
    void hadix_latency(benchmark::State& state)
    {
        std::size_t count = static_cast< std::size_t >(state.range(0));
        hadix_workload workload(count, count);
        std::vector< double > samples;
        samples.reserve(count);
        std::vector< std::uint64_t > const& queries = Operation == hadix_operation::insert ? workload.insertion : Operation == hadix_operation::find_miss ? workload.misses : workload.hits;
        for (auto iteration : state)
        {
            Map values;
            if constexpr (Operation != hadix_operation::insert)
            {
                for (std::uint64_t key : workload.insertion)
                {
                    values.try_emplace(key, key);
                }
            }
            double total_seconds = 0;
            for (std::uint64_t key : queries)
            {
                std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
                if constexpr (Operation == hadix_operation::insert)
                {
                    auto inserted = values.try_emplace(key, key);
                    benchmark::DoNotOptimize(inserted);
                }
                else if constexpr (Operation == hadix_operation::erase)
                {
                    typename Map::size_type erased = values.erase(key);
                    benchmark::DoNotOptimize(erased);
                }
                else
                {
                    typename Map::const_iterator found = std::as_const(values).find(key);
                    benchmark::DoNotOptimize(found);
                }
                std::chrono::steady_clock::time_point finish = std::chrono::steady_clock::now();
                double seconds = std::chrono::duration< double >(finish - start).count();
                total_seconds += seconds;
                samples.push_back(seconds * 1e9);
            }
            state.SetIterationTime(total_seconds);
        }
        std::sort(samples.begin(), samples.end());
        state.counters["median_ns"] = samples[samples.size() / 2];
        state.counters["p99_ns"] = samples[(samples.size() * 99 - 1) / 100];
        state.counters["max_ns"] = samples.back();
        state.counters["samples"] = static_cast< double >(samples.size());
        state.SetItemsProcessed(state.iterations() * static_cast< std::int64_t >(count));
    }
} // namespace rpnx_benchmarks
#endif
