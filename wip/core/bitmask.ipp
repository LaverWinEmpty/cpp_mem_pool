#ifndef CORE_BITMASK_HPP
#    include "bitmask.hpp"
#endif

#include "iostream"

namespace core {

template<typename T> constexpr T& Bitmask::word(T* flags, size_t index) {
    return flags[size_t(index >> Operator<T>::SHIFT)];
}

template<typename T> constexpr T Bitmask::bit(size_t index) {
    return T(1) << size_t(index & Operator<T>::MASK);
}

template<typename T> void Bitmask::set(T* flags, size_t index) {
    word(flags, index) |= bit<T>(index);
}

template<typename T> void Bitmask::unset(T* flags, size_t index) {
    word(flags, index) &= ~bit<T>(index);
}

template<typename T> void Bitmask::flip(T* flags, size_t index) {
    word(flags, index) ^= bit<T>(index);
}

template<typename T> bool Bitmask::test(const T* flags, size_t index) {
    return word(flags, index) & bit<T>(index);
}

template<typename T> void Bitmask::set(T& flags, size_t index) {
    flags |= bit<T>(index);
}

template<typename T> void Bitmask::unset(T& flags, size_t index) {
    flags &= ~bit<T>(index);
}

template<typename T> void Bitmask::flip(T& flags, size_t index) {
    flags ^= bit<T>(index);
}

template<typename T> bool Bitmask::test(const T& flags, size_t index) {
    return flags & bit<T>(index);
}

template<typename T> size_t Bitmask::ffz(const T* flags, size_t bits) {
    return first<T, 0>(flags, bits);
}

template<typename T> size_t Bitmask::ffs(const T* flags, size_t bits) {
    return first<T, 1>(flags, bits);
}

template<typename T> size_t Bitmask::count(const T* flags, size_t bits) {
    if(bits == 0) return 0;
    
    size_t cnt  = 0;
    size_t loop = words<T>(bits) - 1;
    for(size_t i = 0; i < loop; ++i) {
        cnt += global::bit_popcnt(std::make_unsigned_t<T>(flags[i]));
    }
    
    size_t   remainder = bits - (loop * Operator<T>::BITS);
    uint64_t area      = ~0ull >> (Operator<T>::BITS - remainder);
    
    return cnt + global::bit_popcnt(std::make_unsigned_t<T>(flags[loop]) & area);
}

template<typename T> size_t Bitmask::ffz(const T& flags) {
    return first<T, 0>(&flags, Operator<T>::BITS);
}

template<typename T> size_t Bitmask::ffs(const T& flags) {
    return first<T, 1>(&flags, Operator<T>::BITS);
}

template<typename T> size_t Bitmask::count(const T& flags) {
    return global::bit_popcnt(std::make_unsigned_t<T>(flags));
}

template<typename T> constexpr size_t Bitmask::words(size_t n) {
    return (n + Operator<T>::MASK) / Operator<T>::BITS; // same as Aligner::count
}

template<typename T> constexpr size_t Bitmask::bytes(size_t n) {
    return words<T>(n) * sizeof(T); // to byte
}

template<typename T, bool SET> size_t Bitmask::first(const T* flags, size_t bits) {
    std::make_unsigned_t<T> target;

    size_t loop = words<T>(bits);
    for (size_t i = 0; i < loop; ++i) {
        if constexpr (SET) {
            target = flags[i]; // find 1
        }
        else target = ~flags[i]; // find 0, flip

        if (target != 0) {
            int    cnt = global::bit_bsf(target);
            size_t out = (i << Operator<T>::SHIFT) + size_t(cnt);

            if(out < bits) {
                return out;
            }
            else break; // for return error
        }
    }

    return size_t(-1); // error: not found
}

}
