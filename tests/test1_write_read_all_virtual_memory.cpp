#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <math.h>

#include <cstdio>
#include <cassert>
#include <iostream>


//
// int main(int argc, char **argv) {
//     VMinitialize();
//     for (uint64_t i = 0; i < VIRTUAL_MEMORY_SIZE; ++i) {
//         VMwrite(i, i);
//     }
//
//     for (uint64_t i = 0; i < VIRTUAL_MEMORY_SIZE; ++i) {
//         word_t value;
//         VMread(i, &value);
//         assert(uint64_t(value) == i);
//     }
//
//     printf("success\n");

#include "VirtualMemory.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <cstdlib>
#include <ctime>

// Test 1: Sequential access to ALL pages multiple times
void test_all_pages_sequential() {
    std::cout << "\n=== Test 1: Sequential All Pages (Multiple Passes) ===" << std::endl;
    VMinitialize();

    const int NUM_PASSES = 3;

    for (int pass = 0; pass < NUM_PASSES; pass++) {
        std::cout << "Pass " << pass << ": Writing to ALL " << NUM_PAGES << " pages..." << std::endl;

        // Write unique value to EVERY page in virtual memory
        for (uint64_t page = 0; page < NUM_PAGES; page++) {
            uint64_t addr = page * PAGE_SIZE;
            word_t value = (pass * 10000) + page;

            if (VMwrite(addr, value) == 0) {
                std::cout << "WRITE FAILED at page " << page << std::endl;
                return;
            }
        }

        std::cout << "Pass " << pass << ": Reading back ALL pages..." << std::endl;

        // Read back and verify EVERY page
        for (uint64_t page = 0; page < NUM_PAGES; page++) {
            uint64_t addr = page * PAGE_SIZE;
            word_t expected = (pass * 10000) + page;
            word_t actual;

            if (VMread(addr, &actual) == 0) {
                std::cout << "READ FAILED at page " << page << std::endl;
                return;
            }

            if (actual != expected) {
                std::cout << "MISMATCH at page " << page
                         << ": expected " << expected
                         << ", got " << actual << std::endl;
                return;
            }
        }

        std::cout << "Pass " << pass << ": OK (all " << NUM_PAGES << " pages correct)" << std::endl;
    }

    std::cout << "✓ All passes succeeded!" << std::endl;
}

// Test 2: Random access pattern to all pages
void test_all_pages_random() {
    std::cout << "\n=== Test 2: Random Access to All Pages ===" << std::endl;
    VMinitialize();

    srand(42); // Fixed seed for reproducibility

    std::vector<uint64_t> pages;
    for (uint64_t i = 0; i < NUM_PAGES; i++) {
        pages.push_back(i);
    }

    for (int pass = 0; pass < 2; pass++) {
        // Shuffle pages
        for (size_t i = pages.size() - 1; i > 0; i--) {
            size_t j = rand() % (i + 1);
            std::swap(pages[i], pages[j]);
        }

        std::cout << "Pass " << pass << ": Writing in random order..." << std::endl;

        // Write in random order
        for (uint64_t page : pages) {
            uint64_t addr = page * PAGE_SIZE;
            word_t value = (pass * 10000) + page;
            VMwrite(addr, value);
        }

        // Shuffle again for reading
        for (size_t i = pages.size() - 1; i > 0; i--) {
            size_t j = rand() % (i + 1);
            std::swap(pages[i], pages[j]);
        }

        std::cout << "Pass " << pass << ": Reading in different random order..." << std::endl;

        // Read in different random order
        for (uint64_t page : pages) {
            uint64_t addr = page * PAGE_SIZE;
            word_t expected = (pass * 10000) + page;
            word_t actual;
            VMread(addr, &actual);

            if (actual != expected) {
                std::cout << "MISMATCH at page " << page
                         << ": expected " << expected
                         << ", got " << actual << std::endl;
                return;
            }
        }

        std::cout << "Pass " << pass << ": OK" << std::endl;
    }

    std::cout << "✓ Random access test passed!" << std::endl;
}

// Test 3: Write to all pages, then read first pages (tests eviction/restore)
void test_eviction_correctness() {
    std::cout << "\n=== Test 3: Eviction and Restoration ===" << std::endl;
    VMinitialize();

    std::cout << "Writing to first " << NUM_FRAMES << " pages..." << std::endl;
    for (uint64_t page = 0; page < NUM_FRAMES; page++) {
        VMwrite(page * PAGE_SIZE, page + 1000);
    }

    std::cout << "Writing to remaining pages (forcing evictions)..." << std::endl;
    for (uint64_t page = NUM_FRAMES; page < NUM_PAGES; page++) {
        VMwrite(page * PAGE_SIZE, page + 2000);
    }

    std::cout << "Reading back first pages (should be restored from disk)..." << std::endl;
    for (uint64_t page = 0; page < NUM_FRAMES; page++) {
        word_t value;
        VMread(page * PAGE_SIZE, &value);
        word_t expected = page + 1000;

        if (value != expected) {
            std::cout << "EVICTION/RESTORE ERROR at page " << page
                     << ": expected " << expected
                     << ", got " << value << std::endl;
            return;
        }
    }

    std::cout << "✓ Eviction/restoration works correctly!" << std::endl;
}

// Test 4: Boundary addresses
void test_boundary_addresses() {
    std::cout << "\n=== Test 4: Boundary Addresses ===" << std::endl;
    VMinitialize();

    // Test first address
    VMwrite(0, 999);
    word_t val;
    VMread(0, &val);
    assert(val == 999);
    std::cout << "✓ Address 0 works" << std::endl;

    // Test last valid address
    uint64_t last_addr = VIRTUAL_MEMORY_SIZE - 1;
    VMwrite(last_addr, 888);
    VMread(last_addr, &val);
    assert(val == 888);
    std::cout << "✓ Last address works" << std::endl;

    // Test invalid address (should fail)
    int result = VMwrite(VIRTUAL_MEMORY_SIZE, 777);
    assert(result == 0);  // Should fail
    std::cout << "✓ Out-of-bounds address correctly rejected" << std::endl;
}

// Test 5: Multiple writes to same address
void test_overwrite() {
    std::cout << "\n=== Test 5: Multiple Overwrites ===" << std::endl;
    VMinitialize();

    uint64_t addr = 100 * PAGE_SIZE;

    for (int i = 0; i < 10; i++) {
        VMwrite(addr, i * 100);
        word_t val;
        VMread(addr, &val);
        assert(val == i * 100);
    }

    std::cout << "✓ Overwrite test passed" << std::endl;
}

// Test 6: Sparse access pattern (like tests 12/13 might do)
void test_sparse_pattern() {
    std::cout << "\n=== Test 6: Sparse Access Pattern ===" << std::endl;
    VMinitialize();

    // Access pages at regular intervals across entire address space
    std::vector<uint64_t> test_pages;
    for (uint64_t page = 0; page < NUM_PAGES; page += NUM_PAGES / 100) {
        test_pages.push_back(page);
    }

    for (int pass = 0; pass < 3; pass++) {
        std::cout << "Sparse pass " << pass << "..." << std::endl;

        // Write
        for (uint64_t page : test_pages) {
            VMwrite(page * PAGE_SIZE, (pass * 5000) + page);
        }

        // Read back
        for (uint64_t page : test_pages) {
            word_t val;
            VMread(page * PAGE_SIZE, &val);
            word_t expected = (pass * 5000) + page;
            if (val != expected) {
                std::cout << "ERROR at page " << page << std::endl;
                return;
            }
        }
    }

    std::cout << "✓ Sparse pattern test passed" << std::endl;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "  Virtual Memory Comprehensive Test Suite" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "Configuration:" << std::endl;
    std::cout << "  NUM_FRAMES: " << NUM_FRAMES << std::endl;
    std::cout << "  NUM_PAGES: " << NUM_PAGES << std::endl;
    std::cout << "  PAGE_SIZE: " << PAGE_SIZE << std::endl;
    std::cout << "  VIRTUAL_MEMORY_SIZE: " << VIRTUAL_MEMORY_SIZE << std::endl;
    std::cout << "==================================================" << std::endl;

    try {
        test_boundary_addresses();
        test_overwrite();
        test_eviction_correctness();
        test_sparse_pattern();
        test_all_pages_random();
        test_all_pages_sequential();  // This is the BIG one

        std::cout << "\n==================================================" << std::endl;
        std::cout << "  ✓✓✓ ALL TESTS PASSED ✓✓✓" << std::endl;
        std::cout << "==================================================" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "\n❌ TEST FAILED: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}


