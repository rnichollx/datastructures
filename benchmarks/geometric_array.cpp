// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0
#include "geometric_array.hpp"

namespace rpnx_benchmarks
{

    BENCHMARK_TEMPLATE(geometric_append, std::vector< std::uint64_t >)->Arg(1024)->Arg(65536)->Arg(1048576);
    BENCHMARK_TEMPLATE(geometric_append, rpnx::geometric_array< std::uint64_t >)->Arg(1024)->Arg(65536)->Arg(1048576);
    BENCHMARK_TEMPLATE(geometric_append, std::vector< rpnx_benchmarks::large_value >)->Arg(1024)->Arg(65536);
    BENCHMARK_TEMPLATE(geometric_append, rpnx::geometric_array< rpnx_benchmarks::large_value >)->Arg(1024)->Arg(65536);
    BENCHMARK_TEMPLATE(geometric_traversal, std::vector< std::uint64_t >)->Arg(1024)->Arg(65536)->Arg(1048576);
    BENCHMARK_TEMPLATE(geometric_traversal, rpnx::geometric_array< std::uint64_t >)->Arg(1024)->Arg(65536)->Arg(1048576);
    BENCHMARK_TEMPLATE(geometric_random_access, std::vector< std::uint64_t >)->Arg(1024)->Arg(65536)->Arg(1048576);
    BENCHMARK_TEMPLATE(geometric_random_access, rpnx::geometric_array< std::uint64_t >)->Arg(1024)->Arg(65536)->Arg(1048576);

    BENCHMARK_TEMPLATE(geometric_append_latency, std::vector< std::uint64_t >)->Arg(65536)->Arg(60000)->Arg(1048576)->Arg(1000000)->Iterations(1000)->UseManualTime();
    BENCHMARK_TEMPLATE(geometric_append_latency, rpnx::geometric_array< std::uint64_t >)->Arg(65535)->Arg(60000)->Arg(1048575)->Arg(1000000)->Iterations(1000)->UseManualTime();
    BENCHMARK_TEMPLATE(geometric_append_latency, std::vector< large_value >)->Arg(65536)->Arg(60000)->Iterations(1000)->UseManualTime();
    BENCHMARK_TEMPLATE(geometric_append_latency, rpnx::geometric_array< large_value >)->Arg(65535)->Arg(60000)->Iterations(1000)->UseManualTime();
} // namespace rpnx_benchmarks
