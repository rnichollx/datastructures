# RPNX::DataStructures


## Summary

Implementation of various data structures for C++.

Compared to the C++ Standard Library, RPNX::DataStructures aims to
optimize for performance. 

Note that many of the data structure operations have only basic exception safety guarantees, instead of the strong exception safety guarantees provided by most C++ Standard Library data structure operations.

## Data Structures

### Segmented Dynar (Segmented Dynamic Array)

`#include <rpnx/segmented_dynar.hpp>`

`rpnx::segmented_dynar<T, Alloc>` is a dynamic array implementation that uses a segmented storage approach to minimize memory reallocations and copying when the array grows. It is designed to provide efficient random access and dynamic resizing.

It has a similar interface to `std::vector<T, Alloc>`, but with different performance characteristics for certain operations.

Most notably, `push_back` operations have O(log n) worst-case time complexity instead of O(n) due to the segmented storage approach. Elements are not copied when the array grows, only pointers to new segments are added.

## License

This project is licensed under the Apache License 2.0. See the LICENSE file for details.

