# RPNX::DataStructures


## Summary

Implementation of various data structures for C++.

Compared to the C++ Standard Library, RPNX::DataStructures aims to
optimize for performance.

Note that many of the data structure operations have only basic exception safety guarantees, instead of the strong exception safety guarantees provided by most C++ Standard Library data structure operations.

## Data Structures

### Shared Pointer

`#include <rpnx/shared_ptr.hpp>`

`rpnx::shared_ptr<T>` stores its owner count and `T` in one allocation.
`rpnx::make_shared<T>(args...)` creates a populated handle. Default and null
construction create empty handles. Copies share ownership, moves transfer
ownership, and `reset` releases ownership. The last owner destroys `T`.
`get`, `operator*`, and `operator->` access the value; dereferencing requires a
populated handle. The pointer has no weak ownership.

Different handles can be copied and released concurrently. Callers synchronize
access to an individual handle and coordinate mutable access to `T`.
`use_count()` acquires releases by prior owners, allowing a sole owner to access
`T` after those owners finish their accesses. `use_count_estimate()` performs a
relaxed load for approximate counts without ordering accesses to `T`.

### Copy-on-write Pointer

`#include <rpnx/cow.hpp>`

`rpnx::cow<T>` shares ownership when copied. `get`, `read`, `operator*`, and
`operator->` provide immutable access; `edit` copies a shared value before
returning a mutable reference. `set` replaces the stored value. A default
instance owns a default-constructed `T`; an instance constructed with `nullptr`
is empty. Value access requires `has_value()` to be true. Empty values compare
equal and sort before populated values. Different handles may access their
shared value concurrently when `T` supports concurrent const access and copying.
References and pointers returned by access operations must finish use before
their handle releases ownership of that value. Finish mutation through an
`edit()` reference before sharing or transferring ownership of its value.

### Segmented Dynar (Segmented Dynamic Array)

`#include <rpnx/segmented_dynar.hpp>`

`rpnx::segmented_dynar<T, Alloc>` is a dynamic array implementation that uses a segmented storage approach to minimize memory reallocations and copying when the array grows. It is designed to provide efficient random access and dynamic resizing.

It has a similar interface to `std::vector<T, Alloc>`, but with different performance characteristics for certain operations.

Most notably, `push_back` operations have O(log n) worst-case time complexity instead of O(n) due to the segmented storage approach. Elements are not copied when the array grows, only pointers to new segments are added.

### Thin Shared Mutex

`#include <rpnx/thin_shared_mutex.hpp>`

`rpnx::thin_shared_mutex` supports exclusive and shared ownership through
`lock`, `try_lock`, `unlock`, `lock_shared`, `try_lock_shared`, and
`unlock_shared`. It works with `std::unique_lock`, `std::shared_lock`,
`std::scoped_lock`, and `std::condition_variable_any`.

The mutex occupies eight bytes where an inline, lock-free 64-bit atomic is
available, providing a 62-bit reader count. Otherwise it occupies four bytes
and provides a 30-bit reader count. A writer reserves ownership before waiting
for existing readers to drain. New readers wait while that reservation is held.
Scheduling does not guarantee FIFO order or freedom from starvation.

Windows uses native address waits. Linux uses private futexes for four-byte
states and `futex_waitv` on both halves of eight-byte states (Linux 5.16 or later).
macOS 14.4 and later use `os_sync_wait_on_address` and
`os_sync_wake_by_address_all`. Each mutex contains only its atomic state integer.

The mutex is nonrecursive and process-local. The owning thread releases the
mode it acquired. Destruction requires an unlocked mutex, no waiting threads,
and no future access. A releasing thread can finish an address-only wake after
ownership has passed to another thread. Acquisition uses acquire ordering and
release uses release ordering.

### Insert-only Concurrent Hash Trie

`#include <rpnx/insert_only_concurrent_hash_trie.hpp>`

`rpnx::insert_only_concurrent_hash_trie<K, V, Hash, KeyCompareThreeWay, Allocator>`
supports `try_emplace`, `get_or_create`, `find`, `contains`, `at`, and
`size_exclusive`. `try_emplace` returns an entry pointer and an insertion flag.
`get_or_create(key, factory)` calls the factory only if the key is absent and
returns a mapped reference. The factory executes under the terminal lock after
checking for an existing key. A throwing factory leaves the key absent.

Nodes grow through the existing Node4, Node16, Node48, and Node256 layouts.
Smaller nodes retain shared traversal locks. A published Node256 keeps its
layout permanently, allowing traversal and child insertion through its atomic
slots without acquiring the node's shared lock. If a compressed prefix splits,
the Node256 is retained below the new ancestor so existing traversals can finish.
Full-hash collision groups retain their terminal locks and ordered maps.

Entries have stable addresses until destruction. Callers synchronize mutation
of mapped values. Hash, comparison, and allocator state must support concurrent
use; equivalent keys must hash equally, and comparison must never be unordered.
Factory callbacks must not reenter this map. `size_exclusive` and destruction
require callers to exclude concurrent access. Failed insertion may retain empty
routing storage. Erasure and clearing are unavailable; storage is reclaimed
during destruction. Structural operations have O(8 + log(c + 1)) work for c
keys sharing a full hash, excluding allocation, user operations, and contention.

### Concurrent Hash Trie

`#include <rpnx/concurrent_hash_trie.hpp>`

`rpnx::concurrent_hash_trie<K, V, Hash, KeyCompareThreeWay, Allocator>` is a
concurrent associative container with stable entry addresses. It routes over
eight bytes of a 64-bit hash using compressed Node4, Node16, Node48, and Node256
layouts. `Hash` defaults to `std::hash<K>`; unsigned results narrower than
64 bits are zero-extended. `KeyCompareThreeWay` defaults to
`std::compare_three_way` and defines ordering and equivalence for keys with
identical hashes. Equivalent keys must hash equally, and comparison must never
return unordered. Complete-hash collision groups use balanced ordered maps.

Each owning link contains a `thin_shared_mutex`. Operations retain shared
ancestor locks while accessing descendants. Insertion publishes child links
through atomic slots; promotion, prefix splitting, and reclamation acquire the
affected owning link exclusively. Erasure releases empty groups, collapses
single-child branches, and shrinks sparse layouts with hysteresis.

```cpp
#include <cstdint>
#include <rpnx/concurrent_hash_trie.hpp>
#include <string>

rpnx::concurrent_hash_trie<std::string, std::uint64_t> counts;
counts.try_emplace("requests", 0);
counts.with_write("requests", [](decltype(counts)::reference entry) {
    ++entry.second;
});

std::uint64_t snapshot = 0;
counts.with_read("requests", [&](decltype(counts)::const_reference entry) {
    snapshot = entry.second;
});
```

The per-key API comprises `try_emplace`, `insert_or_assign`, `find`, `contains`,
`at`, `operator[]`, `erase`, `with_read`, and `with_write`. Insertion returns
`std::pair<value_type*, bool>`, `find` returns an entry pointer or `nullptr`,
and `erase` returns zero or one. Stored keys are immutable. Values are
constructed in place, supporting move-only keys and immovable mapped values.

`with_read` invokes its callback with an immutable entry under a shared lock.
`with_write` invokes its callback with a mutable entry under an exclusive
terminal lock. Both return whether the key was found. Callbacks must not
reenter the same trie. Returned pointers and references release their locks
before returning; callers coordinate subsequent use with erasure, assignment,
clearing, and destruction. Routing changes preserve retained entry addresses.

`estimate_size` sums protected group counts during concurrent access. Its result
can combine observations from different times. `size_exclusive` provides an
exact count when callers exclude concurrent mutation. `range_exclusive` provides
mutable or const forward traversal under the same external exclusion requirement.
Structural changes can invalidate all iterators, including on failure. Iteration
order is unspecified. `clear` acquires the root exclusively.

Structural key operations cost `O(8 + log(c + 1))` for `c` keys sharing a hash,
excluding allocation, user operations, and lock contention. Hash, comparison,
and allocator policies must support concurrent use. Hash and comparison are
invoked through const references. Copying and moving the trie
are disabled. Destruction requires quiescent access to the trie and its entries.

Operations provide basic exception safety. Failed insertion adds no entry but
can leave empty routing storage. Shrinking allocation failure can occur after
erasure has committed; retained entries remain usable, and later erasures can
reclaim excess routing storage. Callback exceptions propagate after releasing
locks, preserving any changes already made by the callback.

## License

This project is licensed under the Apache License 2.0. See the LICENSE file for details.

