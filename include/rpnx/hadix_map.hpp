// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>

#ifndef RPNXHADIX_HADIX_HPP
#define RPNXHADIX_HADIX_HPP

#error "not implemented"
#include "segmented_dynar.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace rpnx
{
    /**
     * @brief Reserved prototype for a hadix map.
     *
     * This type is not yet fully implemented, including this file is an error.
     *
     * @tparam K Key type.
     * @tparam V Mapped value type.
     * @tparam Hash Hash function for keys.
     * @tparam KeyEqual Equality predicate for keys.
     * @tparam Alloc Allocator for key-value storage.
     */
    template < typename K, typename V, typename Hash = std::hash< K >, typename KeyEqual = std::equal_to< K >, typename Alloc = std::allocator< std::pair< const K, V > > >
    class hadix_table
    {
        enum class color : std::uint8_t
        {
            red,
            black
        };

        struct node
        {
            std::pair<const K, V> item;
            color m_color;
            node * m_left;
            node * m_right;
        };

        using hash_type = std::uint64_t;

        static constexpr std::size_t hash_per_bucket = std::max< std::size_t >(4, std::max< std::size_t >(std::hardware_destructive_interference_size, std::hardware_constructive_interference_size) / sizeof(hash_type));

        struct alignas(std::hardware_destructive_interference_size) bucket
        {
            std::array< hash_type, hash_per_bucket > bucket_hashes;
            std::array< node*, hash_per_bucket > bucket_nodes;
            node * m_overflow;
        };

        segmented_dynar< bucket, Alloc > m_buckets;
        std::size_t m_size;

        static hash_type reverse_bits(hash_type i)
        {
            i = ((i & 0x5555555555555555) << 1) | ((i & 0xAAAAAAAAAAAAAAAA) >> 1);
            i = ((i & 0x3333333333333333) << 2) | ((i & 0xCCCCCCCCCCCCCCCC) >> 2);
            i = ((i & 0x0F0F0F0F0F0F0F0F) << 4) | ((i & 0xF0F0F0F0F0F0F0F0) >> 4);
            i = ((i & 0x00FF00FF00FF00FF) << 8) | ((i & 0xFF00FF00FF00FF00) >> 8);
            i = ((i & 0x0000FFFF0000FFFF) << 16) | ((i & 0xFFFF0000FFFF0000) >> 16);
            i = (i << 32) | (i >> 32);
            return i;
        }

        std::size_t bucket_index(hash_type h)
        {
            auto index = reverse_bits(h);

            // 2. Determine the bit-mask based on current table size
            // std::bit_width(size - 1) gives the number of bits to cover the range
            auto size = m_buckets.size();
            int bits = std::bit_width(size - 1);

            // 3. Create the mask (handle 64-bit edge case)
            hash_type mask = (bits >= (sizeof(hash_type) * 8)) ? ~hash_type(0) : (hash_type(1) << bits) - 1;

            auto result = index & mask;

            // 4. The "Fold" Logic:
            // If the index is out of bounds, it must be in the range [size, 2^bits - 1].
            // Stripping the highest bit guaranteed to fit it in the range [0, size - 1].
            if (result >= size)
            {
                result &= (mask >> 1);
            }

            return result;
        }
    };
} // namespace rpnx

#endif // RPNXHADIX_HADIX_HPP
