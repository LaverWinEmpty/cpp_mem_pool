#include "allocator.hpp"

//! @brief object pool
template<typename T, size_t ALIGNMENT = sizeof(T)>
class Pool : public Allocator<global::bit_align(sizeof(T), ALIGNMENT)> {
public:
    static constexpr size_t BLOCK = global::bit_align(sizeof(T), ALIGNMENT); // same as parent
    using Base = Allocator<BLOCK>;

public:
    template<typename... Args> T* acquire(Args&&... in) {
        return Base::template acquire<T>(std::forward<Args>(in)...);
    }
};
