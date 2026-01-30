// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>

#include <gtest/gtest.h>
#include "rpnx/variant.hpp"


TEST(variant, variant_meta)
{
    rpnx::variant<int, std::string> v = 5;
    ASSERT_TRUE(v.index() == 0);
    ASSERT_TRUE(v.get_as< int >() == 5);
    v = std::string("hello");
    ASSERT_TRUE(v.index() == 1);
    ASSERT_TRUE(v.get_as< std::string >() == "hello");

    rpnx::variant<int, std::string> v2;

    ASSERT_THROW((v2.get_as<std::string>()), std::bad_variant_access);
    ASSERT_TRUE(v2 < v);

    std::pair<int, rpnx::variant<int, std::string>> p = {5, std::string("hello")};

    std::map<rpnx::variant<int, std::string>, int> mp;
    mp[v2] = 9;

    v2 = 5;
    mp[v2] = 6;
    ASSERT_TRUE(mp[0] == 9);
    ASSERT_TRUE(mp[5] == 6);
    ASSERT_TRUE(mp[5] != 7);
}
