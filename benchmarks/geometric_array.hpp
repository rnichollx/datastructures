// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0
#ifndef RPNX_BENCHMARKS_GEOMETRIC_ARRAY_HPP
#define RPNX_BENCHMARKS_GEOMETRIC_ARRAY_HPP
#include <array>
#include <benchmark/benchmark.h>
#include <chrono>
#include <cstdint>
#include <numeric>
#include <random>
#include <rpnx/geometric_array.hpp>
#include <vector>

namespace rpnx_benchmarks
{
    /** @brief Large inline payload that must be relocated when a vector grows. */
    struct large_value
    {
        std::array< std::uint64_t, 32 > words;
        /** @brief Initializes all payload words to a reproducible value. */
        explicit large_value(std::uint64_t value)
        {
            words.fill(value);
        }
    };

    /** @brief Measures complete append workloads, including allocation and destruction. */
    template < typename Container >
    void geometric_append(benchmark::State& state)
    {
        std::size_t count = static_cast< std::size_t >(state.range(0));
        for (auto iteration : state)
        {
            Container values;
            for (std::size_t index = 0; index < count; ++index)
            {
                values.emplace_back(index);
            }
            benchmark::DoNotOptimize(values);
            benchmark::ClobberMemory();
        }
        state.SetItemsProcessed(state.iterations() * static_cast< std::int64_t >(count));
    }

    /**
     * @brief Measures individual append wall-clock latency, including observed tail latency.
     *
     * Setup and destruction are outside the manual timing interval. Containers
     * grow naturally to the registered element count. Registrations select full
     * growth boundaries or counts with spare slots for each container's layout.
     * Samples include clock overhead and scheduling interruptions;
     * the observed maximum is not a hard upper bound on execution time.
     */
    template < typename Container >
    void geometric_append_latency(benchmark::State& state)
    {
        std::size_t count = static_cast< std::size_t >(state.range(0));
        std::vector< double > samples;
        samples.reserve(1000);
        for (auto iteration : state)
        {
            Container values;
            std::size_t initial_size = count;
            for (std::size_t index = 0; index < initial_size; ++index)
            {
                values.emplace_back(index);
            }
            benchmark::DoNotOptimize(values);
            benchmark::ClobberMemory();
            std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
            values.emplace_back(initial_size);
            benchmark::DoNotOptimize(values);
            benchmark::ClobberMemory();
            std::chrono::steady_clock::time_point finish = std::chrono::steady_clock::now();
            double seconds = std::chrono::duration< double >(finish - start).count();
            state.SetIterationTime(seconds);
            samples.push_back(seconds * 1e9);
            state.counters["elements_before_append"] = static_cast< double >(initial_size);
        }
        std::sort(samples.begin(), samples.end());
        if (!samples.empty())
        {
            state.counters["median_ns"] = samples[samples.size() / 2];
            state.counters["p99_ns"] = samples[(samples.size() * 99 - 1) / 100];
            state.counters["max_ns"] = samples.back();
            state.counters["samples"] = static_cast< double >(samples.size());
        }
    }

    /** @brief Measures traversal of an existing sequence, excluding setup. */
    template < typename Container >
    void geometric_traversal(benchmark::State& state)
    {
        std::size_t count = static_cast< std::size_t >(state.range(0));
        Container values(count);
        std::iota(values.begin(), values.end(), std::uint64_t{0});
        for (auto iteration : state)
        {
            benchmark::DoNotOptimize(values);
            std::uint64_t sum = 0;
            for (std::uint64_t value : values)
            {
                sum += value;
            }
            benchmark::DoNotOptimize(sum);
        }
        state.SetItemsProcessed(state.iterations() * static_cast< std::int64_t >(count));
    }

    /** @brief Measures indexed reads in the same deterministic shuffled order. */
    template < typename Container >
    void geometric_random_access(benchmark::State& state)
    {
        std::size_t count = static_cast< std::size_t >(state.range(0));
        Container values(count);
        std::iota(values.begin(), values.end(), std::uint64_t{0});
        std::vector< std::size_t > indices(count);
        std::iota(indices.begin(), indices.end(), std::size_t{0});
        std::mt19937 random(7291);
        std::shuffle(indices.begin(), indices.end(), random);
        for (auto iteration : state)
        {
            benchmark::DoNotOptimize(values);
            benchmark::DoNotOptimize(indices.data());
            std::uint64_t sum = 0;
            for (std::size_t index : indices)
            {
                sum += values[index];
            }
            benchmark::DoNotOptimize(sum);
        }
        state.SetItemsProcessed(state.iterations() * static_cast< std::int64_t >(count));
    }
} // namespace rpnx_benchmarks
#endif
