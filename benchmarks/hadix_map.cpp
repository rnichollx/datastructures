// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0
#include "hadix_map.hpp"

namespace rpnx_benchmarks
{
    BENCHMARK_TEMPLATE(hadix_sampled, rpnx::hadix_map< std::uint64_t, std::uint64_t >)->Name("hadix_map/sampled/distributed")->Args({65536, 6553})->Args({1000000, 100000})->Args({100000000, 100000})->Iterations(1)->UseManualTime();
    BENCHMARK_TEMPLATE(hadix_sampled, std::unordered_map< std::uint64_t, std::uint64_t >)->Name("std_unordered_map/sampled/distributed")->Args({65536, 6553})->Args({1000000, 100000})->Args({100000000, 100000})->Iterations(1)->UseManualTime();
    BENCHMARK_TEMPLATE(hadix_sampled, std::map< std::uint64_t, std::uint64_t >)->Name("std_map/sampled/distributed")->Args({65536, 6553})->Args({1000000, 100000})->Args({100000000, 100000})->Iterations(1)->UseManualTime();

    BENCHMARK_TEMPLATE(hadix_insert, rpnx::hadix_map< std::uint64_t, std::uint64_t >)->Name("hadix_map/insert/distributed")->Arg(1024)->Arg(65536)->Arg(1000000)->Arg(1048576);
    BENCHMARK_TEMPLATE(hadix_insert, std::unordered_map< std::uint64_t, std::uint64_t >)->Name("std_unordered_map/insert/distributed")->Arg(1024)->Arg(65536)->Arg(1000000)->Arg(1048576);
    BENCHMARK_TEMPLATE(hadix_insert, std::map< std::uint64_t, std::uint64_t >)->Name("std_map/insert/distributed")->Arg(1024)->Arg(65536)->Arg(1000000)->Arg(1048576);
    BENCHMARK_TEMPLATE(hadix_lookup, rpnx::hadix_map< std::uint64_t, std::uint64_t >, true)->Name("hadix_map/find_hit/distributed")->Arg(1024)->Arg(65536)->Arg(1000000)->Arg(1048576);
    BENCHMARK_TEMPLATE(hadix_lookup, std::unordered_map< std::uint64_t, std::uint64_t >, true)->Name("std_unordered_map/find_hit/distributed")->Arg(1024)->Arg(65536)->Arg(1000000)->Arg(1048576);
    BENCHMARK_TEMPLATE(hadix_lookup, std::map< std::uint64_t, std::uint64_t >, true)->Name("std_map/find_hit/distributed")->Arg(1024)->Arg(65536)->Arg(1000000)->Arg(1048576);
    BENCHMARK_TEMPLATE(hadix_lookup, rpnx::hadix_map< std::uint64_t, std::uint64_t >, false)->Name("hadix_map/find_miss/distributed")->Arg(1024)->Arg(65536)->Arg(1000000)->Arg(1048576);
    BENCHMARK_TEMPLATE(hadix_lookup, std::unordered_map< std::uint64_t, std::uint64_t >, false)->Name("std_unordered_map/find_miss/distributed")->Arg(1024)->Arg(65536)->Arg(1000000)->Arg(1048576);
    BENCHMARK_TEMPLATE(hadix_lookup, std::map< std::uint64_t, std::uint64_t >, false)->Name("std_map/find_miss/distributed")->Arg(1024)->Arg(65536)->Arg(1000000)->Arg(1048576);
    BENCHMARK_TEMPLATE(hadix_erase, rpnx::hadix_map< std::uint64_t, std::uint64_t >)->Name("hadix_map/erase/distributed")->Arg(1024)->Arg(65536)->Arg(1000000)->Arg(1048576);
    BENCHMARK_TEMPLATE(hadix_erase, std::unordered_map< std::uint64_t, std::uint64_t >)->Name("std_unordered_map/erase/distributed")->Arg(1024)->Arg(65536)->Arg(1000000)->Arg(1048576);
    BENCHMARK_TEMPLATE(hadix_erase, std::map< std::uint64_t, std::uint64_t >)->Name("std_map/erase/distributed")->Arg(1024)->Arg(65536)->Arg(1000000)->Arg(1048576);

    BENCHMARK_TEMPLATE(hadix_insert, rpnx::hadix_map< std::uint64_t, std::uint64_t, hadix_constant_hash >)->Name("hadix_map/insert/collision")->Arg(1024)->Arg(8192);
    BENCHMARK_TEMPLATE(hadix_insert, std::unordered_map< std::uint64_t, std::uint64_t, hadix_constant_hash >)->Name("std_unordered_map/insert/collision")->Arg(1024)->Arg(8192);
    BENCHMARK_TEMPLATE(hadix_insert, std::map< std::uint64_t, std::uint64_t >)->Name("std_map/insert/collision")->Arg(1024)->Arg(8192);
    BENCHMARK_TEMPLATE(hadix_lookup, rpnx::hadix_map< std::uint64_t, std::uint64_t, hadix_constant_hash >, true)->Name("hadix_map/find_hit/collision")->Arg(1024)->Arg(8192);
    BENCHMARK_TEMPLATE(hadix_lookup, std::unordered_map< std::uint64_t, std::uint64_t, hadix_constant_hash >, true)->Name("std_unordered_map/find_hit/collision")->Arg(1024)->Arg(8192);
    BENCHMARK_TEMPLATE(hadix_lookup, std::map< std::uint64_t, std::uint64_t >, true)->Name("std_map/find_hit/collision")->Arg(1024)->Arg(8192);
    BENCHMARK_TEMPLATE(hadix_lookup, rpnx::hadix_map< std::uint64_t, std::uint64_t, hadix_constant_hash >, false)->Name("hadix_map/find_miss/collision")->Arg(1024)->Arg(8192);
    BENCHMARK_TEMPLATE(hadix_lookup, std::unordered_map< std::uint64_t, std::uint64_t, hadix_constant_hash >, false)->Name("std_unordered_map/find_miss/collision")->Arg(1024)->Arg(8192);
    BENCHMARK_TEMPLATE(hadix_lookup, std::map< std::uint64_t, std::uint64_t >, false)->Name("std_map/find_miss/collision")->Arg(1024)->Arg(8192);
    BENCHMARK_TEMPLATE(hadix_erase, rpnx::hadix_map< std::uint64_t, std::uint64_t, hadix_constant_hash >)->Name("hadix_map/erase/collision")->Arg(1024)->Arg(8192);
    BENCHMARK_TEMPLATE(hadix_erase, std::unordered_map< std::uint64_t, std::uint64_t, hadix_constant_hash >)->Name("std_unordered_map/erase/collision")->Arg(1024)->Arg(8192);
    BENCHMARK_TEMPLATE(hadix_erase, std::map< std::uint64_t, std::uint64_t >)->Name("std_map/erase/collision")->Arg(1024)->Arg(8192);

    BENCHMARK_TEMPLATE(hadix_latency, rpnx::hadix_map< std::uint64_t, std::uint64_t >, hadix_operation::insert)->Name("hadix_map/latency_insert/distributed")->Arg(65536)->Arg(1000000)->Arg(1048576)->Iterations(1)->UseManualTime();
    BENCHMARK_TEMPLATE(hadix_latency, std::unordered_map< std::uint64_t, std::uint64_t >, hadix_operation::insert)->Name("std_unordered_map/latency_insert/distributed")->Arg(65536)->Arg(1000000)->Arg(1048576)->Iterations(1)->UseManualTime();
    BENCHMARK_TEMPLATE(hadix_latency, std::map< std::uint64_t, std::uint64_t >, hadix_operation::insert)->Name("std_map/latency_insert/distributed")->Arg(65536)->Arg(1000000)->Arg(1048576)->Iterations(1)->UseManualTime();

    BENCHMARK_TEMPLATE(hadix_latency, rpnx::hadix_map< std::uint64_t, std::uint64_t >, hadix_operation::find_hit)->Name("hadix_map/latency_find_hit/distributed")->Arg(65536)->Arg(1000000)->Arg(1048576)->Iterations(1)->UseManualTime();
    BENCHMARK_TEMPLATE(hadix_latency, std::unordered_map< std::uint64_t, std::uint64_t >, hadix_operation::find_hit)->Name("std_unordered_map/latency_find_hit/distributed")->Arg(65536)->Arg(1000000)->Arg(1048576)->Iterations(1)->UseManualTime();
    BENCHMARK_TEMPLATE(hadix_latency, std::map< std::uint64_t, std::uint64_t >, hadix_operation::find_hit)->Name("std_map/latency_find_hit/distributed")->Arg(65536)->Arg(1000000)->Arg(1048576)->Iterations(1)->UseManualTime();

    BENCHMARK_TEMPLATE(hadix_latency, rpnx::hadix_map< std::uint64_t, std::uint64_t >, hadix_operation::find_miss)->Name("hadix_map/latency_find_miss/distributed")->Arg(65536)->Arg(1000000)->Arg(1048576)->Iterations(1)->UseManualTime();
    BENCHMARK_TEMPLATE(hadix_latency, std::unordered_map< std::uint64_t, std::uint64_t >, hadix_operation::find_miss)->Name("std_unordered_map/latency_find_miss/distributed")->Arg(65536)->Arg(1000000)->Arg(1048576)->Iterations(1)->UseManualTime();
    BENCHMARK_TEMPLATE(hadix_latency, std::map< std::uint64_t, std::uint64_t >, hadix_operation::find_miss)->Name("std_map/latency_find_miss/distributed")->Arg(65536)->Arg(1000000)->Arg(1048576)->Iterations(1)->UseManualTime();

    BENCHMARK_TEMPLATE(hadix_latency, rpnx::hadix_map< std::uint64_t, std::uint64_t >, hadix_operation::erase)->Name("hadix_map/latency_erase/distributed")->Arg(65536)->Arg(1000000)->Arg(1048576)->Iterations(1)->UseManualTime();
    BENCHMARK_TEMPLATE(hadix_latency, std::unordered_map< std::uint64_t, std::uint64_t >, hadix_operation::erase)->Name("std_unordered_map/latency_erase/distributed")->Arg(65536)->Arg(1000000)->Arg(1048576)->Iterations(1)->UseManualTime();
    BENCHMARK_TEMPLATE(hadix_latency, std::map< std::uint64_t, std::uint64_t >, hadix_operation::erase)->Name("std_map/latency_erase/distributed")->Arg(65536)->Arg(1000000)->Arg(1048576)->Iterations(1)->UseManualTime();

    BENCHMARK_TEMPLATE(hadix_latency, rpnx::hadix_map< std::uint64_t, std::uint64_t, hadix_constant_hash >, hadix_operation::insert)->Name("hadix_map/latency_insert/collision")->Arg(8192)->Iterations(1)->UseManualTime();
    BENCHMARK_TEMPLATE(hadix_latency, std::unordered_map< std::uint64_t, std::uint64_t, hadix_constant_hash >, hadix_operation::insert)->Name("std_unordered_map/latency_insert/collision")->Arg(8192)->Iterations(1)->UseManualTime();
    BENCHMARK_TEMPLATE(hadix_latency, std::map< std::uint64_t, std::uint64_t >, hadix_operation::insert)->Name("std_map/latency_insert/collision")->Arg(8192)->Iterations(1)->UseManualTime();

    BENCHMARK_TEMPLATE(hadix_latency, rpnx::hadix_map< std::uint64_t, std::uint64_t, hadix_constant_hash >, hadix_operation::find_hit)->Name("hadix_map/latency_find_hit/collision")->Arg(8192)->Iterations(1)->UseManualTime();
    BENCHMARK_TEMPLATE(hadix_latency, std::unordered_map< std::uint64_t, std::uint64_t, hadix_constant_hash >, hadix_operation::find_hit)->Name("std_unordered_map/latency_find_hit/collision")->Arg(8192)->Iterations(1)->UseManualTime();
    BENCHMARK_TEMPLATE(hadix_latency, std::map< std::uint64_t, std::uint64_t >, hadix_operation::find_hit)->Name("std_map/latency_find_hit/collision")->Arg(8192)->Iterations(1)->UseManualTime();

    BENCHMARK_TEMPLATE(hadix_latency, rpnx::hadix_map< std::uint64_t, std::uint64_t, hadix_constant_hash >, hadix_operation::find_miss)->Name("hadix_map/latency_find_miss/collision")->Arg(8192)->Iterations(1)->UseManualTime();
    BENCHMARK_TEMPLATE(hadix_latency, std::unordered_map< std::uint64_t, std::uint64_t, hadix_constant_hash >, hadix_operation::find_miss)->Name("std_unordered_map/latency_find_miss/collision")->Arg(8192)->Iterations(1)->UseManualTime();
    BENCHMARK_TEMPLATE(hadix_latency, std::map< std::uint64_t, std::uint64_t >, hadix_operation::find_miss)->Name("std_map/latency_find_miss/collision")->Arg(8192)->Iterations(1)->UseManualTime();

    BENCHMARK_TEMPLATE(hadix_latency, rpnx::hadix_map< std::uint64_t, std::uint64_t, hadix_constant_hash >, hadix_operation::erase)->Name("hadix_map/latency_erase/collision")->Arg(8192)->Iterations(1)->UseManualTime();
    BENCHMARK_TEMPLATE(hadix_latency, std::unordered_map< std::uint64_t, std::uint64_t, hadix_constant_hash >, hadix_operation::erase)->Name("std_unordered_map/latency_erase/collision")->Arg(8192)->Iterations(1)->UseManualTime();
    BENCHMARK_TEMPLATE(hadix_latency, std::map< std::uint64_t, std::uint64_t >, hadix_operation::erase)->Name("std_map/latency_erase/collision")->Arg(8192)->Iterations(1)->UseManualTime();
} // namespace rpnx_benchmarks
