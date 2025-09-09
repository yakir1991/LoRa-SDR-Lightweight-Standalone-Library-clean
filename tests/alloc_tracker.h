#pragma once
#include <cstddef>
namespace alloc_tracker {
struct Guard {
    Guard() {}
    ~Guard() {}
    std::size_t count() const { return 0; }
};
}
