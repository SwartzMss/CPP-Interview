// Build: g++ -std=c++17 -O0 -g -Wall -Wextra -pedantic docs/stl/vector_iterator_invalidation_demo.cpp -o /tmp/vector_demo && /tmp/vector_demo
#include <vector>
#include <iostream>
#include <iomanip>
#include <string>

template <class T>
void print_vec(const std::vector<T>& v, const std::string& title = "") {
    if (!title.empty()) std::cout << "\n== " << title << " ==\n";
    std::cout << "size=" << v.size()
              << " capacity=" << v.capacity()
              << " data=" << static_cast<const void*>(v.data())
              << " values:";
    for (const auto& x : v) std::cout << ' ' << x;
    std::cout << "\n";
}

void demo_reallocation_invalidation() {
    std::cout << "\n[Demo A] Without reserve: reallocation invalidates all iterators/pointers/references\n";
    std::vector<int> v;

    // Fill until we trigger at least one reallocation.
    v.push_back(1);
    auto it0 = v.begin();           // iterator to first element
    int* p0 = v.data();             // pointer to first element
    auto base = v.data();           // remember base address
    print_vec(v, "initial state");

    // Grow until capacity changes at least once
    std::size_t last_cap = v.capacity();
    bool reallocated = false;
    for (int i = 2; i <= 64; ++i) {
        v.push_back(i);
        if (v.capacity() != last_cap) {
            std::cout << "capacity grew: " << last_cap << " -> " << v.capacity()
                      << ", data moved: " << base << " -> " << static_cast<const void*>(v.data()) << "\n";
            last_cap = v.capacity();
            base = v.data();
            reallocated = true;
            break; // one reallocation is enough to demonstrate
        }
    }
    print_vec(v, "after growth");

    // DO NOT dereference it0 or p0 here; they are invalid if reallocated.
    if (reallocated) {
        std::cout << "Note: it0/p0 captured before reallocation are now INVALID.\n";
    } else {
        std::cout << "No reallocation observed (unlikely). it0 might still be valid.\n";
    }
}

void demo_reserve_stability() {
    std::cout << "\n[Demo B] With reserve: push_back without reallocation keeps iterators valid (except end())\n";
    std::vector<int> v;
    v.reserve(100); // ensure no reallocation for <=100 elements
    v.push_back(10);
    auto it0 = v.begin();
    int* p0 = v.data();
    auto base = v.data();
    print_vec(v, "reserved and one element");

    for (int i = 11; i < 20; ++i) v.push_back(i);
    print_vec(v, "after more push_back (no reallocation expected)");

    std::cout << "base stayed: " << base << ", current data: " << static_cast<const void*>(v.data()) << "\n";
    std::cout << "Dereference saved iterator/pointer: *it0=" << *it0 << ", *p0=" << *p0 << " (still valid)\n";
}

void demo_insert_erase_rules() {
    std::cout << "\n[Demo C] insert/erase without reallocation: which iterators are invalidated\n";
    std::vector<int> v;
    v.reserve(100); // avoid reallocation so we focus on local invalidation rules
    for (int i = 0; i < 6; ++i) v.push_back(i); // [0 1 2 3 4 5]
    print_vec(v, "start");

    // Capture iterators at different positions
    auto it_before = v.begin() + 1; // points to 1 (before insertion point)
    auto it_at = v.begin() + 2;     // points to 2 (at insertion point)
    auto it_after = v.begin() + 4;  // points to 4 (after insertion point)

    std::size_t cap_before = v.capacity();
    const void* data_before = v.data();

    // Insert at position 2; this shifts elements at/after pos to the right
    v.insert(v.begin() + 2, 99); // vector becomes [0 1 99 2 3 4 5]
    print_vec(v, "after insert at pos=2");
    std::cout << "capacity same? " << std::boolalpha << (v.capacity() == cap_before)
              << ", data moved? " << (static_cast<const void*>(v.data()) != data_before) << "\n";

    // Per standard: if no reallocation, iterators before pos remain valid; pos and after are invalidated.
    std::cout << "Safe to dereference it_before (points to original 1): *it_before=" << *it_before << "\n";

    std::cout << "Do NOT dereference it_at/it_after: they are invalid (undefined behavior).\n";

    // Demonstrate safe pattern: store index instead of iterator
    std::size_t idx_at = 2;   // original logical position
    std::cout << "Access by index after insert: v[3]=" << v[3] << " (element originally at index 2 shifted right)\n";

    // Erase demo: erasing invalidates from erase position to end, and returns next valid iterator
    auto it_next = v.erase(v.begin() + 3); // erase the element that is now at index 3 (which was 2)
    print_vec(v, "after erase at pos=3");
    if (it_next != v.end()) {
        std::cout << "iterator returned by erase points to: " << *it_next << " (first element after erased one)\n";
    }
}

int main() {
    std::cout << std::boolalpha;
    demo_reallocation_invalidation();
    demo_reserve_stability();
    demo_insert_erase_rules();
    return 0;
}

