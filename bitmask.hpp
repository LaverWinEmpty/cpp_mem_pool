#include "../global/bit.hpp"
#include "cstring"

using size_t = uintptr_t;

//! @brief static mix-in
class Bitmask {
protected:
    template<typename T> struct Operator {
        static constexpr size_t BITS  = sizeof(T) * 8;
        static constexpr size_t MASK  = BITS - 1;
        static constexpr size_t SHIFT = global::bit_log2(BITS);
    };

public:
    template<typename T> static void set(T* flags, size_t index);
    template<typename T> static void unset(T* flags, size_t index);
    template<typename T> static void flip(T* flags, size_t index);
    template<typename T> static bool check(const T* flags, size_t index);

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

template<typename T, bool SET> size_t Bitmask::first(const T* flags, size_t bits) {
    T target;

    size_t loop = words<T>(bits);
    for(size_t i = 0; i < loop; ++i) {
        if constexpr(SET) {
            target = flags[i];
        }
        else target = ~flags[i];
        if(target != 0) {
            int    cnt = global::bit_ctz(target);
            size_t out = (i << Operator<T>::SHIFT) + size_t(cnt);
            return (out < bits) ? out : T(-1);
        }
    }
    return size_t(-1);
}

// template<size_t N, bool = false> class Flags;
//
// template<size_t N> class Flags<N, true>: protected Bitmask {
// public:
//    inline void on(size_t index) { Bitmask::on(flags, index, N << SHIFT); }
//    inline void off(size_t index) { Bitmask::off(flags, index, N << SHIFT); }
//    inline void toggle(size_t index) { Bitmask::toggle(flags, index, N << SHIFT); }
//    inline bool check(size_t index) { return Bitmask::check(flags, index, N << SHIFT); }
//    inline size_t next() { return Bitmask::ffz(flags, N << SHIFT); }
//    inline void reset() { Bitmask::reset(flags, sizeof(Word) * N); }
//
// public:
//    Word read(size_t index) const { return flags[index]; }
//
// private:
//    Word flags[N] = { 0 };
//};
//
// template<size_t N> class Flags<N, false>: public Flags<Bitmask::words(N), true> {
// public:
//    static constexpr size_t SIZE = N;
//};
//
// template<bool BASE> class Flags<0, BASE>: protected Bitmask {
// public:
//    static constexpr size_t SIZE = 0;
//
//    inline void on(size_t index) { }
//    inline void off(size_t index) { }
//    inline void toggle(size_t index) { }
//    inline bool check(size_t index) { return false; }
//    inline size_t next() { return Word(-1); }
//    inline void reset() { }
//    inline Word word() { return 0; }
//};
