#ifndef CORE_BITMASK_HPP
#define CORE_BITMASK_HPP

#include "../global/bit.hpp"
#include "cstring"

namespace core {


//! @brief static mix-in
class Bitmask {
protected:
    template<typename T> struct Operator {
        static constexpr size_t BITS  = sizeof(T) * 8;
        static constexpr size_t MASK  = BITS - 1;
        static constexpr size_t SHIFT = global::bit_bsr(BITS);
    };

public:
    template<typename T> static void set(T* flags, size_t index);
    template<typename T> static void unset(T* flags, size_t index);
    template<typename T> static void flip(T* flags, size_t index);
    template<typename T> static bool check(const T* flags, size_t index);

public:
    template<typename T> static void set(T& flags, size_t index);
    template<typename T> static void unset(T& flags, size_t index);
    template<typename T> static void flip(T& flags, size_t index);
    template<typename T> static bool check(const T& flags, size_t index);

public:
    template<typename T> static size_t ffz(const T* flags, size_t bits);
    template<typename T> static size_t ffs(const T* flags, size_t bits);

protected:
    template<typename T> static constexpr size_t words(size_t bits);
    template<typename T> static constexpr size_t bytes(size_t bits);

private:
    template<typename T> static constexpr T& word(T* flags, size_t index);
    template<typename T> static constexpr T  bit(size_t index);
    template<typename T, bool> static size_t first(const T* flags, size_t size);

protected:
    Bitmask() = default;
};

}
#include "bitmask.ipp"
#endif
