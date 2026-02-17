// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
#include <benchmark/benchmark.h>
#include <functional>
#include <new>
#include "rpnx/functional.hpp"

template <typename T>
struct isolate
{
    alignas(std::hardware_destructive_interference_size) T val;

    T & operator *()
    {
        return val;
    }
};


// --- Helper Functors ---

struct SmallFunctor {
    int x;
    int operator()(int a) const { return a + x; }
};

struct LargeFunctor {
    int data[16]; // 64 bytes, definitely > 16 bytes SBO
    int operator()(int a) const { return a + data[0]; }
};

struct StatelessFunctor {
    int operator()(int a) const { return a; }
};

int plain_function(int a) { return a; }

using FunctionPointer = int (*)(int);
using FreeFunction = int(int);

// --- Construction Benchmarks ---

template <typename FuncType, typename Functor>
static void BM_Construct(benchmark::State& state) {
    if constexpr (std::is_same_v<Functor, FreeFunction>) {
        for (auto _ : state) {
            alignas(std::hardware_destructive_interference_size) FuncType func(plain_function);
            benchmark::DoNotOptimize(func);
        }
    } else {
        isolate<Functor> f;
        if constexpr (std::is_same_v<Functor, FunctionPointer>) {
            *f = &plain_function;
        } else if constexpr (std::is_same_v<Functor, StatelessFunctor>) {
            *f = StatelessFunctor{};
        } else {
            *f = Functor{42};
        }
        benchmark::DoNotOptimize(*f);

        for (auto _ : state) {
            alignas(std::hardware_destructive_interference_size) FuncType func(*f);
            benchmark::DoNotOptimize(func);
        }
    }
}

BENCHMARK_TEMPLATE(BM_Construct, std::function<int(int)>, SmallFunctor);
BENCHMARK_TEMPLATE(BM_Construct, rpnx::function<int(int)>, SmallFunctor);
BENCHMARK_TEMPLATE(BM_Construct, std::function<int(int)>, LargeFunctor);
BENCHMARK_TEMPLATE(BM_Construct, rpnx::function<int(int)>, LargeFunctor);
BENCHMARK_TEMPLATE(BM_Construct, std::function<int(int)>, StatelessFunctor);
BENCHMARK_TEMPLATE(BM_Construct, rpnx::function<int(int)>, StatelessFunctor);
BENCHMARK_TEMPLATE(BM_Construct, std::function<int(int)>, FunctionPointer);
BENCHMARK_TEMPLATE(BM_Construct, rpnx::function<int(int)>, FunctionPointer);
BENCHMARK_TEMPLATE(BM_Construct, std::function<int(int)>, FreeFunction);
BENCHMARK_TEMPLATE(BM_Construct, rpnx::function<int(int)>, FreeFunction);

// --- Move Benchmarks ---

template <typename FuncType, typename Functor>
static void BM_Move(benchmark::State& state) {
    auto get_functor = []() {
        if constexpr (std::is_function_v<Functor>) {
            return plain_function;
        } else if constexpr (std::is_same_v<Functor, FunctionPointer>) {
            return &plain_function;
        } else if constexpr (std::is_same_v<Functor, StatelessFunctor>) {
            return StatelessFunctor{};
        } else {
            return Functor{42};
        }
    };

    auto f = get_functor();
    benchmark::DoNotOptimize(f);
    for (auto _ : state) {
        state.PauseTiming();
        isolate<FuncType> func1;
        *func1 = FuncType(f);
        state.ResumeTiming();
        isolate<FuncType> func2;
        *func2 = std::move(*func1);
        benchmark::DoNotOptimize(func2);
    }
}

BENCHMARK_TEMPLATE(BM_Move, std::function<int(int)>, SmallFunctor);
BENCHMARK_TEMPLATE(BM_Move, rpnx::function<int(int)>, SmallFunctor);
BENCHMARK_TEMPLATE(BM_Move, std::function<int(int)>, LargeFunctor);
BENCHMARK_TEMPLATE(BM_Move, rpnx::function<int(int)>, LargeFunctor);
BENCHMARK_TEMPLATE(BM_Move, std::function<int(int)>, StatelessFunctor);
BENCHMARK_TEMPLATE(BM_Move, rpnx::function<int(int)>, StatelessFunctor);
BENCHMARK_TEMPLATE(BM_Move, std::function<int(int)>, FunctionPointer);
BENCHMARK_TEMPLATE(BM_Move, rpnx::function<int(int)>, FunctionPointer);
BENCHMARK_TEMPLATE(BM_Move, std::function<int(int)>, FreeFunction);
BENCHMARK_TEMPLATE(BM_Move, rpnx::function<int(int)>, FreeFunction);

// --- Invocation Benchmarks ---

template <typename FuncType, typename Functor>
static void BM_Invoke(benchmark::State& state) {

    isolate<FuncType> func;

        if constexpr (std::is_same_v<Functor, FreeFunction>) {
            *func =  plain_function;
        } else if constexpr (std::is_same_v<Functor, FunctionPointer>) {
            *func = &plain_function;
        } else if constexpr (std::is_same_v<Functor, StatelessFunctor>) {
            *func = StatelessFunctor{};
        } else {
            *func = Functor{42};
        }


    benchmark::DoNotOptimize(func);
    for (auto _ : state) {
        int i = (*func)(9);
        benchmark::DoNotOptimize(i);
    }
}


BENCHMARK_TEMPLATE(BM_Invoke, std::function<int(int)>, SmallFunctor);
BENCHMARK_TEMPLATE(BM_Invoke, rpnx::function<int(int)>, SmallFunctor);
BENCHMARK_TEMPLATE(BM_Invoke, std::function<int(int)>, LargeFunctor);
BENCHMARK_TEMPLATE(BM_Invoke, rpnx::function<int(int)>, LargeFunctor);
BENCHMARK_TEMPLATE(BM_Invoke, std::function<int(int)>, StatelessFunctor);
BENCHMARK_TEMPLATE(BM_Invoke, rpnx::function<int(int)>, StatelessFunctor);
BENCHMARK_TEMPLATE(BM_Invoke, std::function<int(int)>, FunctionPointer);
BENCHMARK_TEMPLATE(BM_Invoke, rpnx::function<int(int)>, FunctionPointer);
BENCHMARK_TEMPLATE(BM_Invoke, std::function<int(int)>, FreeFunction);
BENCHMARK_TEMPLATE(BM_Invoke, rpnx::function<int(int)>, FreeFunction);

// --- Copy Benchmarks ---

template <typename FuncType, typename Functor>
static void BM_Copy(benchmark::State& state) {
    auto get_functor = []() {
        if constexpr (std::is_function_v<Functor>) {
            return (int(&)(int))plain_function;
        } else if constexpr (std::is_same_v<Functor, FunctionPointer>) {
            return &plain_function;
        } else if constexpr (std::is_same_v<Functor, StatelessFunctor>) {
            return StatelessFunctor{};
        } else {
            return Functor{42};
        }
    };

    isolate<FuncType> func1;
    *func1 = FuncType(get_functor());
    for (auto _ : state) {
        isolate<FuncType> func2;
        *func2 = *func1;
        benchmark::DoNotOptimize(func2);
    }
}

BENCHMARK_TEMPLATE(BM_Copy, std::function<int(int)>, SmallFunctor);
BENCHMARK_TEMPLATE(BM_Copy, rpnx::function<int(int)>, SmallFunctor);
BENCHMARK_TEMPLATE(BM_Copy, std::function<int(int)>, LargeFunctor);
BENCHMARK_TEMPLATE(BM_Copy, rpnx::function<int(int)>, LargeFunctor);
BENCHMARK_TEMPLATE(BM_Copy, std::function<int(int)>, StatelessFunctor);
BENCHMARK_TEMPLATE(BM_Copy, rpnx::function<int(int)>, StatelessFunctor);
BENCHMARK_TEMPLATE(BM_Copy, std::function<int(int)>, FunctionPointer);
BENCHMARK_TEMPLATE(BM_Copy, rpnx::function<int(int)>, FunctionPointer);
BENCHMARK_TEMPLATE(BM_Copy, std::function<int(int)>, FreeFunction);
BENCHMARK_TEMPLATE(BM_Copy, rpnx::function<int(int)>, FreeFunction);

BENCHMARK_MAIN();


