// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0
#include "hadix_map.hpp"
#include <rpnx/inline_hadix_map.hpp>

namespace rpnx_benchmarks
{
    BENCHMARK_TEMPLATE(hadix_sampled, rpnx::inline_hadix_map< std::uint64_t, std::uint64_t >)->Name("inline_hadix_map/sampled/distributed")->Args({65536, 6553})->Args({1000000, 100000})->Args({25000000, 100000})->Iterations(1)->UseManualTime();
    BENCHMARK_TEMPLATE(hadix_insert, rpnx::inline_hadix_map< std::uint64_t, std::uint64_t >)->Name("inline_hadix_map/insert/distributed")->Arg(1024)->Arg(65536)->Arg(1000000)->Arg(1048576);
    BENCHMARK_TEMPLATE(hadix_lookup, rpnx::inline_hadix_map< std::uint64_t, std::uint64_t >, true)->Name("inline_hadix_map/find_hit/distributed")->Arg(1024)->Arg(65536)->Arg(1000000)->Arg(1048576);
    BENCHMARK_TEMPLATE(hadix_lookup, rpnx::inline_hadix_map< std::uint64_t, std::uint64_t >, false)->Name("inline_hadix_map/find_miss/distributed")->Arg(1024)->Arg(65536)->Arg(1000000)->Arg(1048576);
    BENCHMARK_TEMPLATE(hadix_erase, rpnx::inline_hadix_map< std::uint64_t, std::uint64_t >)->Name("inline_hadix_map/erase/distributed")->Arg(1024)->Arg(65536)->Arg(1000000)->Arg(1048576);
    BENCHMARK_TEMPLATE(hadix_insert, rpnx::inline_hadix_map< std::uint64_t, std::uint64_t, hadix_constant_hash >)->Name("inline_hadix_map/insert/collision")->Arg(1024)->Arg(8192);
    BENCHMARK_TEMPLATE(hadix_lookup, rpnx::inline_hadix_map< std::uint64_t, std::uint64_t, hadix_constant_hash >, true)->Name("inline_hadix_map/find_hit/collision")->Arg(1024)->Arg(8192);
    BENCHMARK_TEMPLATE(hadix_lookup, rpnx::inline_hadix_map< std::uint64_t, std::uint64_t, hadix_constant_hash >, false)->Name("inline_hadix_map/find_miss/collision")->Arg(1024)->Arg(8192);
    BENCHMARK_TEMPLATE(hadix_erase, rpnx::inline_hadix_map< std::uint64_t, std::uint64_t, hadix_constant_hash >)->Name("inline_hadix_map/erase/collision")->Arg(1024)->Arg(8192);
    BENCHMARK_TEMPLATE(hadix_latency, rpnx::inline_hadix_map< std::uint64_t, std::uint64_t >, hadix_operation::insert)->Name("inline_hadix_map/latency_insert/distributed")->Arg(65536)->Arg(1000000)->Arg(1048576)->Iterations(1)->UseManualTime();
    BENCHMARK_TEMPLATE(hadix_latency, rpnx::inline_hadix_map< std::uint64_t, std::uint64_t >, hadix_operation::find_hit)->Name("inline_hadix_map/latency_find_hit/distributed")->Arg(65536)->Arg(1000000)->Arg(1048576)->Iterations(1)->UseManualTime();
    BENCHMARK_TEMPLATE(hadix_latency, rpnx::inline_hadix_map< std::uint64_t, std::uint64_t >, hadix_operation::find_miss)->Name("inline_hadix_map/latency_find_miss/distributed")->Arg(65536)->Arg(1000000)->Arg(1048576)->Iterations(1)->UseManualTime();
    BENCHMARK_TEMPLATE(hadix_latency, rpnx::inline_hadix_map< std::uint64_t, std::uint64_t >, hadix_operation::erase)->Name("inline_hadix_map/latency_erase/distributed")->Arg(65536)->Arg(1000000)->Arg(1048576)->Iterations(1)->UseManualTime();
    BENCHMARK_TEMPLATE(hadix_latency, rpnx::inline_hadix_map< std::uint64_t, std::uint64_t, hadix_constant_hash >, hadix_operation::insert)->Name("inline_hadix_map/latency_insert/collision")->Arg(8192)->Iterations(1)->UseManualTime();
    BENCHMARK_TEMPLATE(hadix_latency, rpnx::inline_hadix_map< std::uint64_t, std::uint64_t, hadix_constant_hash >, hadix_operation::find_hit)->Name("inline_hadix_map/latency_find_hit/collision")->Arg(8192)->Iterations(1)->UseManualTime();
    BENCHMARK_TEMPLATE(hadix_latency, rpnx::inline_hadix_map< std::uint64_t, std::uint64_t, hadix_constant_hash >, hadix_operation::find_miss)->Name("inline_hadix_map/latency_find_miss/collision")->Arg(8192)->Iterations(1)->UseManualTime();
    BENCHMARK_TEMPLATE(hadix_latency, rpnx::inline_hadix_map< std::uint64_t, std::uint64_t, hadix_constant_hash >, hadix_operation::erase)->Name("inline_hadix_map/latency_erase/collision")->Arg(8192)->Iterations(1)->UseManualTime();
} // namespace rpnx_benchmarks
