// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0

#include "concurrent_maps.hpp"

namespace rpnx_benchmarks
{
    [[maybe_unused]] bool const concurrent_map_benchmarks_registered = []
    {
        register_concurrent_map_benchmarks< trie_map_operations >("concurrent_hash_trie");
        register_concurrent_map_benchmarks< sharded_map_operations >("conc_sharded_unordered_map");
        register_mixed_map_benchmarks< trie_map_operations, uniform_map_indices >("concurrent_hash_trie/mixed_uniform");
        register_mixed_map_benchmarks< sharded_map_operations, uniform_map_indices >("conc_sharded_unordered_map/mixed_uniform");
        register_mixed_map_benchmarks< trie_map_operations, hot_map_indices >("concurrent_hash_trie/mixed_hot90");
        register_mixed_map_benchmarks< sharded_map_operations, hot_map_indices >("conc_sharded_unordered_map/mixed_hot90");
        register_mixed_map_benchmarks< insert_only_map_operations, uniform_map_indices >("insert_only_concurrent_hash_trie/mixed_uniform");
        register_mixed_map_benchmarks< insert_only_map_operations, hot_map_indices >("insert_only_concurrent_hash_trie/mixed_hot90");
        return true;
    }();
} // namespace rpnx_benchmarks
