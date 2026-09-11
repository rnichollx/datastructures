// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0

#include "rpnx/uint64_base.hpp"

#include "gtest/gtest.h"

namespace
{
    struct test_identifier : rpnx::uint64_base< test_identifier >
    {
        using rpnx::uint64_base< test_identifier >::uint64_base;
    };
} // namespace

TEST(uint64_base, header_provides_serialization_types)
{
    test_identifier value(42);

    EXPECT_EQ(std::get< 0 >(value.tie()), 42);
    ASSERT_EQ(test_identifier::strings().size(), 1);
    EXPECT_EQ(test_identifier::strings().front(), "value");
}
