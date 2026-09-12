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
#include <string>
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
    enum class hadix_operation { insert, find_hit, find_miss, erase, mixed };

    /**
     * @brief Measures full construction and deletion passes plus timed lookup and churn.
     * Construction inserts N distinct keys once, and final deletion removes all N keys once.
     * Present lookup, absent lookup and mixed erase/insert phases each run for at least
     * five measured seconds, completing the final batch. Mixed pairs replace an existing
     * key with a fresh key, so population alternates between N-1 and N.
     * Key generation, shuffling and live-key bookkeeping occur outside batch timing.
     * Means include per-operation clock and maximum tracking overhead. Mixed statistics
     * are per individual operation, not per pair. The mixed maximum also covers construction
     * and final deletion; mixed_phase_max_ns retains the mixed-only peak. Its mean covers
     * only the five-second mixed phase. Observed wall-clock maxima include
     * scheduling effects and do not establish an execution-time bound.
     */
    template < typename Map >
    void hadix_sampled(benchmark::State& state)
    {
        std::size_t count = static_cast< std::size_t >(state.range(0));
        hadix_workload workload(count, static_cast< std::size_t >(state.range(1)));
        /** @brief Accumulates elapsed time, sample count, successful results and peak latency. */
        struct operation_measurement
        {
            double seconds = 0;
            std::size_t operations = 0;
            std::size_t successes = 0;
            std::chrono::steady_clock::duration maximum{};
        };
        auto measure_batch = []< hadix_operation Operation >(Map& current, std::vector< std::uint64_t > const& queries, operation_measurement& result)
        {
            std::size_t successes = 0;
            std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
            for (std::size_t i = 0; i < queries.size(); ++i)
            {
                std::uint64_t key = queries[i];
                std::chrono::steady_clock::time_point operation_start = std::chrono::steady_clock::now();
                if constexpr (Operation == hadix_operation::insert)
                {
                    std::pair< typename Map::iterator, bool > inserted = current.try_emplace(key, key);
                    benchmark::DoNotOptimize(inserted);
                    successes += inserted.second;
                }
                else if constexpr (Operation == hadix_operation::erase)
                {
                    successes += current.erase(key);
                }
                else if constexpr (Operation == hadix_operation::mixed)
                {
                    if (i % 2 == 0)
                    {
                        successes += current.erase(key);
                    }
                    else
                    {
                        std::pair< typename Map::iterator, bool > inserted = current.try_emplace(key, key);
                        benchmark::DoNotOptimize(inserted);
                        successes += inserted.second;
                    }
                }
                else
                {
                    typename Map::const_iterator found = std::as_const(current).find(key);
                    benchmark::DoNotOptimize(found);
                    successes += found != current.end();
                }
                benchmark::DoNotOptimize(successes);
                std::chrono::steady_clock::duration elapsed = std::chrono::steady_clock::now() - operation_start;
                result.maximum = std::max(result.maximum, elapsed);
            }
            benchmark::DoNotOptimize(current);
            benchmark::ClobberMemory();
            result.seconds += std::chrono::duration< double >(std::chrono::steady_clock::now() - start).count();
            result.operations += queries.size();
            result.successes += successes;
        };
        state.counters["initial_keys"] = static_cast< double >(count);
        state.counters["batch_keys"] = static_cast< double >(workload.hits.size());
        std::size_t total_operations = 0;
        for (auto iteration : state)
        {
            Map values;
            operation_measurement insertion;
            measure_batch.template operator()< hadix_operation::insert >(values, workload.insertion, insertion);
            auto measure_lookup = [&]< hadix_operation Operation >(std::vector< std::uint64_t >& queries)
            {
                operation_measurement result;
                std::size_t cursor = 0;
                std::mt19937_64 random(809174);
                while (result.seconds < 5.0)
                {
                    std::size_t offset = Operation == hadix_operation::find_miss ? count : 0;
                    for (std::size_t i = 0; i < queries.size(); ++i)
                    {
                        queries[i] = hadix_benchmark_key(offset + (cursor + i) % count);
                    }
                    std::shuffle(queries.begin(), queries.end(), random);
                    measure_batch.template operator()< Operation >(values, queries, result);
                    cursor = (cursor + queries.size()) % count;
                }
                return result;
            };
            operation_measurement hit = measure_lookup.template operator()< hadix_operation::find_hit >(workload.hits);
            operation_measurement miss = measure_lookup.template operator()< hadix_operation::find_miss >(workload.misses);
            operation_measurement mixed;
            std::vector< std::uint64_t > live_keys = workload.insertion;
            std::vector< std::uint64_t > mixed_queries(workload.hits.size() * 2);
            std::vector< std::size_t > positions(workload.hits.size());
            std::size_t cursor = 0;
            std::uint64_t next_key = count;
            std::mt19937_64 random(809175);
            while (mixed.seconds < 5.0)
            {
                for (std::size_t i = 0; i < positions.size(); ++i)
                {
                    positions[i] = (cursor + i) % count;
                }
                std::shuffle(positions.begin(), positions.end(), random);
                for (std::size_t i = 0; i < positions.size(); ++i)
                {
                    mixed_queries[2 * i] = live_keys[positions[i]];
                    mixed_queries[2 * i + 1] = hadix_benchmark_key(next_key++);
                    live_keys[positions[i]] = mixed_queries[2 * i + 1];
                }
                measure_batch.template operator()< hadix_operation::mixed >(values, mixed_queries, mixed);
                if (values.size() != count || mixed.successes != mixed.operations)
                {
                    state.SkipWithError("Mixed operations failed to preserve the target population");
                    return;
                }
                cursor = (cursor + positions.size()) % count;
            }
            std::shuffle(live_keys.begin(), live_keys.end(), random);
            operation_measurement deletion;
            measure_batch.template operator()< hadix_operation::erase >(values, live_keys, deletion);
            if (!values.empty() || insertion.successes != count || deletion.successes != count || hit.successes != hit.operations || miss.successes != 0)
            {
                state.SkipWithError("Construction, lookup or deletion returned an unexpected result");
                return;
            }
            double total_seconds = 0;
            for (std::pair< char const*, operation_measurement > phase : {std::pair{"insert", insertion}, {"find_hit", hit}, {"find_miss", miss}, {"mixed", mixed}, {"erase", deletion}})
            {
                std::string name = phase.first;
                operation_measurement& result = phase.second;
                state.counters[name + "_ns"] += result.seconds * 1e9 / result.operations;
                state.counters[name + "_max_ns"] = std::max(static_cast< double >(state.counters[name + "_max_ns"]), std::chrono::duration< double, std::nano >(result.maximum).count());
                state.counters[name + "_seconds"] += result.seconds;
                state.counters[name + "_samples"] += static_cast< double >(result.operations);
                total_operations += result.operations;
                total_seconds += result.seconds;
            }
            state.counters["mixed_phase_max_ns"] = std::max(static_cast< double >(state.counters["mixed_phase_max_ns"]), std::chrono::duration< double, std::nano >(mixed.maximum).count());
            state.counters["mixed_max_ns"] = std::max({static_cast< double >(state.counters["mixed_max_ns"]), static_cast< double >(state.counters["insert_max_ns"]), static_cast< double >(state.counters["erase_max_ns"])});
            state.SetIterationTime(total_seconds);
        }
        for (char const* counter : {"find_hit_ns", "find_miss_ns", "insert_ns", "erase_ns", "mixed_ns"})
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
