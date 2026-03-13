#ifndef CORE_BITMASK_HPP
#    include "bitmask.hpp"
#endif

namespace core {

template<typename T> constexpr T& Bitmask::word(T* flags, size_t index) {
    return flags[size_t(index >> Operator<T>::SHIFT)];
}

template<typename T> constexpr T Bitmask::bit(size_t index) {
    return 1ull << size_t(index & Operator<T>::MASK);
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

template<typename T> size_t Bitmask::ffz(const T& flags) {
    return first<T, 0>(&flags, Operator<T>::BITS);
}

template<typename T> size_t Bitmask::ffs(const T& flags) {
    return first<T, 1>(&flags, Operator<T>::BITS);
}

template<typename T> constexpr size_t Bitmask::words(size_t n) {
    return (n + Operator<T>::MASK) / Operator<T>::BITS; // same as Aligner::count
}

template<typename T> constexpr size_t Bitmask::bytes(size_t n) {
    return words<T>(n) * sizeof(T); // to byte
}

template<typename T, bool SET> size_t Bitmask::first(const T* flags, size_t bits) {
    T target;

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
