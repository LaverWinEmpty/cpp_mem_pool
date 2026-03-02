#include "defs.h"

template<typename T, size_t N> struct TArray {
public:
    T* operator[](size_t index) { return array[index]; }
private:
    T array[N];
};

template<typename T> struct TArray<T, 0> {
public:
    T* operator[](size_t) { return nullptr; }
};
