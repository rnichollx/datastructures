// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>

#include "gtest/gtest.h"

#include <functional>
#include "rpnx/functional.hpp"

TEST(function, rpnx_function_copy)
{
    int call_count = 0;
    rpnx::function<int()> f1 = [&call_count]() { return ++call_count; };
    
    EXPECT_TRUE(f1);
    EXPECT_EQ(f1(), 1);

    rpnx::function<int()> f2 = f1;
    EXPECT_TRUE(f2);
    EXPECT_EQ(f2(), 2);
    EXPECT_EQ(f1(), 3);
}

TEST(function, rpnx_function_non_sbo)
{
    struct LargeFunctor {
        int data[10];
        int operator()() { return data[0]; }
    };

    LargeFunctor lf;
    lf.data[0] = 42;

    rpnx::function<int()> f1 = lf;
    EXPECT_TRUE(f1);
    EXPECT_EQ(f1(), 42);

    rpnx::function<int()> f2 = f1;
    EXPECT_TRUE(f2);
    EXPECT_EQ(f2(), 42);
}