// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>

// Mostly AI-generated tests.

#include "gtest/gtest.h"

#include "rpnx/functional.hpp"
#include "test_utils.hpp"
#include <functional>

TEST(function, rpnx_function_default_ctor)
{
    rpnx::function< int() > f;
    EXPECT_FALSE(f);
    EXPECT_EQ(f, nullptr);
    EXPECT_THROW(f(), std::bad_function_call);
}

TEST(function, rpnx_function_copy)
{
    int call_count = 0;
    rpnx::function< int() > f1 = [&call_count]()
    {
        return ++call_count;
    };

    EXPECT_TRUE(f1);
    EXPECT_EQ(f1(), 1);

    rpnx::function< int() > f2 = f1;
    EXPECT_TRUE(f2);
    EXPECT_EQ(f2(), 2);
    EXPECT_EQ(f1(), 3);
}

TEST(function, rpnx_function_move)
{
    int call_count = 0;
    rpnx::function< int() > f1 = [&call_count]()
    {
        return ++call_count;
    };

    EXPECT_TRUE(f1);

    rpnx::function< int() > f2 = std::move(f1);
    EXPECT_TRUE(f2);
    EXPECT_FALSE(f1);
    EXPECT_EQ(f2(), 1);
    EXPECT_THROW(f1(), std::bad_function_call);
}

TEST(function, rpnx_function_non_sbo)
{
    struct LargeFunctor
    {
        int data[10];
        int operator()()
        {
            return data[0];
        }
    };

    LargeFunctor lf;
    lf.data[0] = 42;

    rpnx::function< int() > f1 = lf;
    EXPECT_TRUE(f1);
    EXPECT_EQ(f1(), 42);

    rpnx::function< int() > f2 = f1;
    EXPECT_TRUE(f2);
    EXPECT_EQ(f2(), 42);
}

TEST(function, rpnx_function_non_sbo_move)
{
    struct LargeFunctor
    {
        int data[10];
        int operator()()
        {
            return data[0];
        }
    };

    LargeFunctor lf;
    lf.data[0] = 42;

    rpnx::function< int() > f1 = lf;
    EXPECT_TRUE(f1);

    rpnx::function< int() > f2 = std::move(f1);
    EXPECT_TRUE(f2);
    EXPECT_FALSE(f1);
    EXPECT_EQ(f2(), 42);
}

TEST(function, rpnx_function_assignment)
{
    rpnx::function< int() > f1 = []()
    {
        return 1;
    };
    rpnx::function< int() > f2 = []()
    {
        return 2;
    };

    f1 = f2;
    EXPECT_EQ(f1(), 2);
    EXPECT_EQ(f2(), 2);

    f1 = []()
    {
        return 3;
    };
    EXPECT_EQ(f1(), 3);

    f1 = std::move(f2);
    EXPECT_EQ(f1(), 2);
    // Not a valid assumption, if the type is SBO and also nothrow move assignable, it's not required to be reset.
    // EXPECT_FALSE(f2);
}

struct LifetimeTracker
{
    int* destroyed_count;
    LifetimeTracker(int* d) : destroyed_count(d)
    {
    }
    LifetimeTracker(const LifetimeTracker&) = default;
    LifetimeTracker(LifetimeTracker&&) = default;
    ~LifetimeTracker()
    {
        if (destroyed_count)
            (*destroyed_count)++;
    }
    int operator()()
    {
        return 0;
    }
};

TEST(function, rpnx_function_lifetime)
{
    int destroyed = 0;
    {
        rpnx::function< int() > f;
        f = LifetimeTracker(&destroyed);
    }
    // 1 destruction for the temporary, 1 for the one in f
    EXPECT_EQ(destroyed, 2);
}

TEST(function, rpnx_function_reassignment_lifetime)
{
    int destroyed1 = 0;
    int destroyed2 = 0;
    {
        rpnx::function< int() > f = LifetimeTracker(&destroyed1);
        int initial_destroyed1 = destroyed1;
        f = LifetimeTracker(&destroyed2);
        EXPECT_GT(destroyed1, initial_destroyed1); // Should have destroyed the first one
    }
    EXPECT_GE(destroyed2, 2);
}

TEST(function, rpnx_function_large_lifetime)
{
    struct LargeLifetimeTracker
    {
        int data[10];
        int* destroyed_count;
        LargeLifetimeTracker(int* d) : destroyed_count(d)
        {
            data[0] = 0;
        }
        ~LargeLifetimeTracker()
        {
            if (destroyed_count)
                (*destroyed_count)++;
        }
        int operator()()
        {
            return data[0];
        }
    };

    int destroyed = 0;
    {
        rpnx::function< int() > f = LargeLifetimeTracker(&destroyed);
    }
    EXPECT_EQ(destroyed, 2);
}

TEST(function, rpnx_function_argument_passing)
{
    rpnx::function< int(int, int) > f = [](int a, int b)
    {
        return a + b;
    };
    EXPECT_EQ(f(10, 20), 30);
}

TEST(function, rpnx_function_complex_args)
{
    rpnx::function< std::string(std::string) > f = [](std::string s)
    {
        return s + "!";
    };
    EXPECT_EQ(f("hello"), "hello!");
}