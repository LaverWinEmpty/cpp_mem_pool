#include "allocator.hpp"

template<typename T, size_t ALIGNMENT = sizeof(void*)>
class Pool: public Allocator<Allocator<0, T>::aligner(ALIGNMENT)> {
    using Base = Allocator<Allocator<0, T>::aligner(ALIGNMENT)>;

public:
    template<typename... Args> T* acquire(Args&&... in) {
        return static_cast<Base*>(this)->acquire<T>(std::forward<Args>(in)...);
    }
};