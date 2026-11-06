# Concurrent map benchmark — November 5, 2026

The Release benchmark compared `rpnx::concurrent_hash_trie` with
`rpnx::conc_sharded_unordered_map` on the same 64-bit integer keys and mapped
values. Both containers used the same fixed 64-bit mixing hash. The sharded map
used its default shard count.

Read and update runs started with 65,536 entries. Reads copied the value while
holding each container's read protection. Updates assigned an existing value
under each container's write protection. Insert and erase runs processed 131,072
distinct keys in total across all threads. Each insert or erase run received a
fresh map; erase runs started with all target keys populated. Setup and teardown
were excluded from timing.

The table reports median aggregate throughput from three runs, in millions of
operations per second. The last column divides sharded throughput by trie
throughput. Thread counts include the 16 logical CPUs reported by Google
Benchmark on this machine.

| Operation | Threads | Trie | Sharded map | Sharded / trie |
| --- | ---: | ---: | ---: | ---: |
| Read | 1 | 18.05 | 62.32 | 3.45× |
| Read | 2 | 18.24 | 86.12 | 4.72× |
| Read | 4 | 12.83 | 116.44 | 9.08× |
| Read | 8 | 7.20 | 67.16 | 9.33× |
| Read | 16 | 3.75 | 47.72 | 12.71× |
| Update | 1 | 13.71 | 62.96 | 4.59× |
| Update | 2 | 17.17 | 84.63 | 4.93× |
| Update | 4 | 9.10 | 103.90 | 11.42× |
| Update | 8 | 5.53 | 20.75 | 3.75× |
| Update | 16 | 3.57 | 15.78 | 4.41× |
| Insert | 1 | 11.02 | 21.14 | 1.92× |
| Insert | 2 | 7.54 | 14.03 | 1.86× |
| Insert | 4 | 8.00 | 20.30 | 2.54× |
| Insert | 8 | 6.39 | 14.45 | 2.26× |
| Insert | 16 | 3.23 | 13.10 | 4.06× |
| Erase | 1 | 6.27 | 35.37 | 5.64× |
| Erase | 2 | 9.18 | 26.96 | 2.94× |
| Erase | 4 | 7.80 | 23.29 | 2.99× |
| Erase | 8 | 3.06 | 17.40 | 5.68× |
| Erase | 16 | 1.50 | 14.47 | 9.64× |

The sharded map was faster in each measured case. The trie retains shared locks
along the route from its root to a leaf; the sharded map locks one shard per
operation. This difference is a plausible contributor to the trie’s lower
read and update throughput under contention, but these measurements do not
isolate lock costs from routing, allocation, and container layout.

The run used Apple Clang 21.0.0 on Darwin arm64 with 16 reported logical CPUs
and Google Benchmark 1.9.4. Google Benchmark could not determine a reliable CPU
frequency or set thread affinity. The measured values use wall time and do not
depend on the reported CPU frequency. The system load average was about 3.6 at
the start of the run. The trie read and update measurements at 16 threads had
13.7% and 17.1% variation across the three repetitions, respectively.

Reproduce from `dev-workspace` after a Release build of the benchmark target:

```sh
cbuild build -c Release -t datastructures --cmake-target rpnx_datastructures_benchmarks
buildspaces/system-clang/datastructures/Release/rpnx_datastructures_benchmarks \
  '--benchmark_filter=^(concurrent_hash_trie|conc_sharded_unordered_map)/' \
  '--benchmark_min_time=0.2s' '--benchmark_repetitions=3'
```
