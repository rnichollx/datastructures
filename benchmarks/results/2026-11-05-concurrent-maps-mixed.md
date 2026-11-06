# Mixed concurrent map workloads — November 5, 2026

Each run starts with an empty map and performs 1,048,576 accesses. A fixed
64-bit mixing function generates pseudorandom indices from an access counter.
Threads receive disjoint ranges of that counter, preserving the aggregate
trace across thread counts and containers. Scheduling determines interleaving.

On a hit, the operation reads the stored value. On a miss, it calculates a
64-bit value from the key and inserts it if absent. Values are immutable after
insertion. Every returned value is checked against the calculation.
The trie uses `find` followed by `try_emplace` on a miss. The sharded map uses
`get_or_create`, calculating the value while holding its shard lock. Racing
misses in the trie can duplicate calculation, while only one inserts the key.

The uniform distribution covers the entire index domain. The hot distribution
directs 90% of draws into a subset containing approximately 1% of the domain;
the other 10% cover the entire domain. Both maps use the same mixing hash for
container routing and the default container configuration.

Median aggregate throughput from three Release runs is shown in millions of
accesses per second. The CPU-count run uses all 16 reported logical CPUs.

| Distribution | Index domain | Final entries | Successful inserts / accesses | Trie, 1 thread | Sharded, 1 thread | Trie, 16 threads | Sharded, 16 threads |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Uniform | 65,536 | 65,536 | 6.25% | 14.146 | 55.503 | 2.687 | 36.592 |
| Uniform | 1,048,576 | 662,486 | 63.18% | 4.091 | 16.622 | 1.780 | 11.921 |
| Hot 90% | 65,536 | 52,239 | 4.98% | 26.297 | 53.476 | 2.861 | 34.894 |
| Hot 90% | 1,048,576 | 108,831 | 10.38% | 17.844 | 42.269 | 2.580 | 27.846 |

All repetitions produced the same final entry counts in both maps. All value
checks passed. Successful insertion counts describe the full trace; insertion
is more frequent near its beginning as the initially empty map fills.

The sharded map was faster in all measured mixed workloads. At 16 threads it
achieved 6.70–13.62 times the trie's throughput. These measurements include
index generation, value calculation, validation, lookup, and insertion.

Environment: Apple Clang 21.0.0, Darwin arm64, Google Benchmark 1.9.4, Release
configuration, wall-clock timing. Initial system load average was 3.0. Thread
affinity was unavailable. Setup and map destruction were excluded from timing.

From `dev-workspace`:

```sh
cbuild build -c Release -t datastructures --cmake-target rpnx_datastructures_benchmarks
buildspaces/system-clang/datastructures/Release/rpnx_datastructures_benchmarks \
  '--benchmark_filter=/mixed_' '--benchmark_repetitions=3'
```
