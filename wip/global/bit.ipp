namespace global {

constexpr int bit_ctz(uint64_t in) noexcept {
    if(in == 0) return -1;

    // built-in
#if CXX_HAS_BUILTIN(__builtin_ctzll)
    return int(__builtin_ctzll(in)); // compile-time
#endif

    // runtime on MSVC
#if TARGET_COMP == COMP_MSVC
    if(!CXX_IS_CONSTANT_EVALUATED()) {
        unsigned long index = 0;
        if constexpr (TARGET_BITS == BITS_64) {
            if(_BitScanForward64(&index, in)) return int(index);
        }
        else {
            if(_BitScanForward(&index, unsigned long(in >> 0))) return int(index);
            if(_BitScanForward(&index, unsigned long(in >> 32))) return int(index + 32);
        }
        return int(index);
    }
#endif

    // fallback for constexpr
    //
    // e.g. input 12 (1100)
    // (in - 1) -> 1011 (fill low bits)
    // (~in)    -> 0011 (flip for find zeros)
    // AND        ------
    //             0011 -> return 2
    return bit_popcnt((in - 1) & ~in);
}

constexpr int bit_clz(uint64_t in) noexcept {
    if(in == 0) return -1;


    // built-in
#if CXX_HAS_BUILTIN(__builtin_clzll)
    return int(__builtin_clzll(in)); // compile-time
#endif

    // runtime on MSVC
#if TARGET_COMP == COMP_MSVC
    if(!CXX_IS_CONSTANT_EVALUATED()) {
        unsigned long index = 0;
        if constexpr (TARGET_BITS == BITS_64) {
            if(_BitScanReverse64(&index, in)) return 63 - int(index);
        }
        else {
            if(_BitScanReverse(&index, unsigned long(in >> 32))) return 31 - (int)index;
            if(_BitScanReverse(&index, unsigned long(in >> 0))) return 63 - (int)index;
        }
        return int(index);
    }
#endif

    // fallback for constexpr
    in |= (in >> 1);  // get MSB  e.g. 10000100 -> 11000110
    in |= (in >> 2);  // move 2   e.g. 11000110 -> 11111111
    in |= (in >> 4);  // move 4   8 bits end
    in |= (in >> 8);  // move 8   16 bits end
    in |= (in >> 16); // move 16  32 bits end
    in |= (in >> 32); // move 32  64 bits end

    // e.g. 64 - (0xFF -> 8)
    return sizeof(uint64_t) * 8 - bit_popcnt(in);
}

constexpr uint64_t bit_pow2(uint64_t in) noexcept {
    if(in <= 1) return 1;

    // 0x10 -> 0x0F -> get count 60 -> (1 << (64 - 60)) = 0x10
    int shift = (64 - bit_clz(in - 1));

    return shift >= 64 ? uint64_t(-1) : uint64_t(1) << shift;
}

constexpr int bit_log2(uint64_t in) noexcept {
    // 0 is that for induce an error
    return bit_ctz(bit_aligned(in) ? in : 0);
}

constexpr uint64_t bit_align(uint64_t in, uint64_t unit) noexcept {
    if(unit == 0) return uint64_t(-1);
    if(in <= unit) return unit;
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

constexpr int bit_popcnt(uint64_t in) noexcept {
    // run-time
    if (!CXX_IS_CONSTANT_EVALUATED()) {
    #if CXX_HAS_BUILTIN(__builtin_popcountll)
        return __builtin_popcountll(in); // non-compile-time
    #endif

    // MSVC
    #if TARGET_COMP == COMP_MSVC
        if constexpr (TARGET_BITS == BITS_64) {
            return (int)__popcnt64(in);
        }
        else return __popcnt(uint32_t(in)) + __popcnt(uint32_t(in >> 32));
    #endif
    }

    // fallback for constexpr
    uint64_t hi = (in >> 1); // (high to low)

    hi &= 0x5555555555555555ull;      // get high bit      (0b0101)
    in -= hi;                         // 2 bits count      (0b01)
    hi = in & 0xCCCCCCCCCCCCCCCCull; // get high 2 bits   (0b1100)
    in = in & 0x3333333333333333ull; // get low 2 bits    (0b0011)
    in = in + (hi >> 2);             // 4 bits count      (1100 + 0011)
    in = in + (in >> 4);             // 8 bits count      (11110000 + 00001111)
    in &= 0x0F0F0F0F0F0F0F0Full;      // clean high 4 bits (11111111 & 00001111)
    in *= 0x0101010101010101ull;      // parallel shfit    (00001111 << 8)

    return in >> 56; // return merged at high 8 bits
}


} // namespace global
