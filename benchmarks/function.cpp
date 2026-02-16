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

// --- Construction Benchmarks ---

template <typename FuncType, typename Functor>
static void BM_Construct(benchmark::State& state) {
    isolate<Functor> f{42};
    benchmark::DoNotOptimize(*f);

    for (auto _ : state) {
        alignas(std::hardware_destructive_interference_size) FuncType func(*f);
        benchmark::DoNotOptimize(func);
    }
}

BENCHMARK_TEMPLATE(BM_Construct, std::function<int(int)>, SmallFunctor);
BENCHMARK_TEMPLATE(BM_Construct, rpnx::function<int(int)>, SmallFunctor);
BENCHMARK_TEMPLATE(BM_Construct, std::function<int(int)>, LargeFunctor);
BENCHMARK_TEMPLATE(BM_Construct, rpnx::function<int(int)>, LargeFunctor);

// --- Move Benchmarks ---

template <typename FuncType, typename Functor>
static void BM_Move(benchmark::State& state) {
    Functor f{42};
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

// --- Invocation Benchmarks ---

template <typename FuncType, typename Functor>
static void BM_Invoke(benchmark::State& state) {
    isolate<Functor> f;
    *f = {42};
    isolate<FuncType> func;
    *func = FuncType(*f);
    benchmark::DoNotOptimize(f);
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

// --- Copy Benchmarks ---

template <typename FuncType, typename Functor>
static void BM_Copy(benchmark::State& state) {
    Functor f{42};
    isolate<FuncType> func1;
    *func1 = FuncType(f);
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

BENCHMARK_MAIN();


