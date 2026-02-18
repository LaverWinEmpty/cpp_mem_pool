namespace global {

constexpr int bit_ctz(uint64_t in) noexcept {
    if(in == 0) return -1;

#if TARGET_COMP & (COMP_CLANG | COMP_GCC)
    return int(__builtin_ctzll(in));

#elif (TARGET_COMP & COMP_MSVC) && (TARGET_BITS & (BITS_32 | BITS_64))
    unsigned long index = 0;

    if(!IS_CONSTANT_EVALUATED) {
#    if TARGET_BITS == BITS_64
        if(_BitScanForward64(&index, in)) return int(index);
#    else
        if(_BitScanForward(&index, unsigned long(in >> 0))) return int(index);
        if(_BitScanForward(&index, unsigned long(in >> 32))) return int(index + 32);
#    endif
        return int(index);
    }
#endif
    // fallback for constexpr
    constexpr uint64_t LSB = 1;

    int res = 0;
    while((in & LSB) == 0) {
        in >>= 1;
        ++res;
    }
    return res;
}

constexpr int bit_clz(uint64_t in) noexcept {
    if(in == 0) return -1;

#if TARGET_COMP & (COMP_CLANG | COMP_GCC)
    return int(__builtin_clzll(in));

#elif (TARGET_COMP & COMP_MSVC) && (TARGET_BITS & (BITS_32 | BITS_64))
    unsigned long index = 0;

    if(!IS_CONSTANT_EVALUATED) {
#    if TARGET_BITS == BITS_64
        if(_BitScanReverse64(&index, in)) return 63 - int(index);
#    else
        if(_BitScanReverse(&index, unsigned long(in >> 32))) return 31 - (int)index;
        if(_BitScanReverse(&index, unsigned long(in >> 0))) return 63 - (int)index;
#    endif
        return int(index);
    }
#endif
    // fallback for constexpr
    constexpr uint64_t MSB = 1ull << 63;

    int res = 0;
    while((in & MSB) == 0) {
        in <<= 1;
        ++res;
    }
    return res;
}

constexpr uint64_t bit_pow2(uint64_t in) noexcept {
    if(in <= 1) return 1;

    // 0x10 -> 0x0F -> get count 60 -> (1 << (64 - 60)) = 0x10
    int shift = (64 - bit_clz(in - 1));

    return shift >= 64 ? 0 : uint64_t(1) << shift;
}

constexpr int bit_log2(uint64_t in) noexcept {
    // 0 is that for induce an error
    return bit_ctz(bit_aligned(in) ? in : 0);
}

constexpr uint64_t bit_align(uint64_t in, uint64_t unit) noexcept {
    if(unit <= 1) return in;
    if(bit_aligned(unit)) {
        return (in + unit - 1) & ~(unit - 1);
    }
    return uint64_t(-1);
}

constexpr bool bit_aligned(uint64_t in, uint64_t unit) noexcept {
    if(unit == 1) return true;

    if(unit == 0) {
        return in && !(in & (in - 1)); // check pow of 2
    }

    if(bit_aligned(unit)) {
        return (in & (unit - 1)) == 0; // check bit_aligned
    }

    return false;
}

} // namespace global
