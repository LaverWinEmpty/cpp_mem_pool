// WIP

#include "cstdint"

//! @brief static mix-in
class Bitmask {
protected:
    using Word = uintptr_t;

protected:
    static constexpr size_t BITS  = sizeof(Word) * 8;
    static constexpr size_t MASK  = BITS - 1;
    static constexpr size_t SHIFT = global::bit_log2(BITS);

public:
    static void   on(Word flags[], size_t size, size_t index);
    static void   off(Word flags[], size_t size, size_t index);
    static void   toggle(Word flags[], size_t size, size_t index);
    static bool   check(Word flags[], size_t size, size_t index);
    static void   reset(Word flags[], size_t size);
    static size_t ffz(const Word flags[], size_t size);
    static size_t ffs(const Word flags[], size_t size);

protected:
    static constexpr size_t cycle(size_t bits);

private:
    static constexpr Word& word(Word flags[], size_t index) { return flags[size_t(index >> SHIFT)]; }
    static constexpr Word  bit(size_t index) { return Word(1) << size_t(index & MASK); }

protected:
    Bitmask() = default;
};

void Bitmask::on(Word flags[], size_t size, size_t index) {
    word(flags, index) |= bit(index);
}

void Bitmask::off(Word flags[], size_t size, size_t index) {
    word(flags, index) &= ~bit(index);
}

void Bitmask::toggle(Word flags[], size_t size, size_t index) {
    word(flags, index) ^= bit(index);
}

bool Bitmask::check(Word flags[], size_t size, size_t index) {
    return word(flags, index) & bit(index);
}

size_t Bitmask::ffz(const Word flags[], size_t size) {
    size_t bytes = cycle(size);

    size_t out = 0;
    for(size_t i = 0; i < bytes; ++i) {
        // campare with 0xFF...FF
        if(flags[i] != Word(-1)) {
            int cnt = global::bit_ctz(Word(~flags[i]));   // find first zero (unset)
            out     = size_t((i << SHIFT) + size_t(cnt)); // i * 64 + found index
            break;
        }
    }

    if(out < size) {
        return out;
    }
    return Word(-1); // error
}

size_t Bitmask::ffs(const Word flags[], size_t size) {
    size_t bytes = cycle(size);

    size_t out = Word(-1);
    for(size_t i = 0; i < bytes; ++i) {
        // campare with
        if(flags[i] != Word(0)) {
            int cnt = global::bit_ctz(Word(flags[i]));    // find first set (1)
            out     = size_t((i << SHIFT) + size_t(cnt)); // i * 64 + found index
            break;
        }
    }

    if(out < size) {
        return out;
    }
    return Word(-1); // error
}

void Bitmask::reset(Word flags[], size_t size) {
    std::memset(flags, 0, cycle(size));
}

constexpr size_t Bitmask::cycle(size_t n) {
    return (n + MASK) / BITS; // same as Aligner::count
}

template<size_t N, bool = false> class Flags;

template<size_t N> class Flags<N, true>: protected Bitmask {
public:
    inline void   on(size_t index) { Bitmask::on(flags, index, N << SHIFT); }
    inline void   off(size_t index) { Bitmask::off(flags, index, N << SHIFT); }
    inline void   toggle(size_t index) { Bitmask::toggle(flags, index, N << SHIFT); }
    inline bool   check(size_t index) { return Bitmask::check(flags, index, N << SHIFT); }
    inline size_t next() { return Bitmask::ffz(flags, N << SHIFT); }
    inline void   reset() { Bitmask::reset(flags, sizeof(Word) * N); }

public:
    Word read(size_t index) const { return flags[index]; }

private:
    Word flags[N] = { 0 };
};

template<size_t N> class Flags<N, false>: public Flags<Bitmask::cycle(N), true> {
public:
    static constexpr size_t SIZE = N;
};

template<bool BASE> class Flags<0, BASE>: protected Bitmask {
public:
    static constexpr size_t SIZE = 0;

    inline void   on(size_t index) { }
    inline void   off(size_t index) { }
    inline void   toggle(size_t index) { }
    inline bool   check(size_t index) { return false; }
    inline size_t next() { return Word(-1); }
    inline void   reset() { }
    inline Word   word() { return 0; }
};
