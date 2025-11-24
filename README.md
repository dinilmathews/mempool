# mempool
Implementation of a standard malloc/free for embedded platform


## Overview

A lightweight memory pool allocator designed for embedded systems where dynamic memory allocation is needed. The implementation uses a first-fit allocation strategy with automatic coalescing of adjacent free blocks to minimize fragmentation. This implementation is based on one of the heap management strategies in FreeRTOS.


## Running Tests

```bash
# Build the test
make

# Clean
make clean

# Build and run tests
make run
```