# Adaptive insert-only trie comparison

The insert-only variant shares the adaptive radix implementation with
`concurrent_hash_trie`. Node4, Node16, Node48, and collision leaves retain their
locks. Node256 layouts are permanent and publish an atomic pointer for traversal
without shared locking. Prefix splits retain those layouts below a new ancestor.
Erasure and clearing are disabled.

The mixed workload starts empty and performs 1,048,576 accesses, calculating and
inserting a value on a miss. The uniform and hot distributions and the value
calculation match the earlier mixed benchmark. All three containers use identical
aggregate traces. The insert-only trie and sharded map use `get_or_create`;
the general trie uses `find` followed by `try_emplace` on a miss.

Median aggregate throughput from three Release repetitions at 16 threads,
in millions of accesses per second:

| Distribution | Index domain | Final entries | General trie | Sharded map | Insert-only trie |
| --- | ---: | ---: | ---: | ---: | ---: |
| Uniform | 65,536 | 65,536 | 2.678 | 34.959 | 134.914 |
| Uniform | 1,048,576 | 662,486 | 1.762 | 12.081 | 50.265 |
| Hot 90% | 65,536 | 52,239 | 2.733 | 33.649 | 129.397 |
| Hot 90% | 1,048,576 | 108,831 | 2.612 | 28.001 | 133.042 |

Every returned value passed validation. Final insertion counts agreed across
the containers. This run used Apple Clang 21.0.0, Darwin arm64, Google Benchmark
1.9.4, and 16 reported logical CPUs. Initial load average was 7.24; thread affinity
was unavailable. Timings include index generation, validation, and value
calculation. Setup and destruction are excluded. These workloads include no
full-hash collisions or erasure.

From `dev-workspace`:

```sh
cbuild build -c Release -t datastructures --cmake-target rpnx_datastructures_benchmarks
buildspaces/system-clang/datastructures/Release/rpnx_datastructures_benchmarks \
  '--benchmark_filter=/mixed_' '--benchmark_repetitions=3'
```
