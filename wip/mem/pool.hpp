#include "allocator.hpp"

template<typename T, size_t ALIGNMENT = sizeof(void*)>
class Pool: public Allocator<global::bit_align(sizeof(T), global::bit_align(sizeof(T), global::bit_pow2(ALIGNMENT)))> {

};
