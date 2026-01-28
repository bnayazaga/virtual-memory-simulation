# Virtual Memory Simulation

A simulation of a paging algorithm for managing virtual memory with hierarchical page tables and cyclic distance-based page replacement.

**Course**: Hebrew University Operating Systems  
**Implementation**: VirtualMemory.cpp (support files provided by course staff)

This project implements:
- Multi-level page table address translation
- Demand paging with page fault handling
- Page eviction using cyclic distance algorithm
- Frame allocation with three-tier search strategy

**Applied techniques**: DFS traversal, bit manipulation, tree search, low-level memory management

**Key concepts**: Virtual-to-physical address translation, hierarchical page tables, page fault handling

**Note**: C-style implementation in C++ (per OS course requirements)

---

## How to Use

### Compilation
```bash
g++ -std=c++11 -Wall -Wextra VirtualMemory.cpp PhysicalMemory.cpp -o vm_simulation
```

### API

#### Initialize the system
```c
#include "VirtualMemory.h"

VMinitialize();  // Must be called before any read/write operations
```

#### Write to virtual memory
```c
word_t value = 42;
int result = VMwrite(0x12345, value);  // Returns 1 on success, 0 on failure
```

#### Read from virtual memory
```c
word_t value;
int result = VMread(0x12345, &value);  // Returns 1 on success, 0 on failure
// value now contains 42
```

There are test files in the tests folder, that were provided by the course staff