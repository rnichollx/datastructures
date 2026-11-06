// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0
#ifndef RPNX_INSERT_ONLY_CONCURRENT_HASH_TRIE_HPP
#define RPNX_INSERT_ONLY_CONCURRENT_HASH_TRIE_HPP

#include <rpnx/concurrent_hash_trie.hpp>

namespace rpnx
{
    /** @brief Adaptive concurrent trie with permanent Node256 layouts and no erasure.
     *
     * Smaller nodes use shared traversal locks. Node256 traversal uses atomic child
     * links without shared locking. Prefix splitting retains the old Node256 under
     * a new ancestor, keeping in-flight traversals valid. Destruction requires
     * quiescent access. Returned references require external synchronization for
     * mutable access. Hash, comparison, and allocator requirements match
     * concurrent_hash_trie. Factory callbacks must not reenter the same trie.
     */
    template < typename K, typename V, typename Hash = std::hash< K >, typename KeyCompareThreeWay = std::compare_three_way, typename Allocator = std::allocator< std::pair< K const, V > > >
    using insert_only_concurrent_hash_trie = basic_concurrent_hash_trie< K, V, Hash, KeyCompareThreeWay, Allocator, true >;
}
#endif
