#ifndef CORE_MASK_HPP
#    include "mask.hpp"
#endif

namespace core {

template<size_t N> Mask<N>& Mask<N>::on(size_t index) {
    flags[index >> 6] |= (1ull << uint64_t(index & (64 - 1))); // [index / 64] |= index % 64
    return *this;
}

template<size_t N> Mask<N>& Mask<N>::off(size_t index) {
    flags[index >> 6] &= ~(1ull << uint64_t(index & (64 - 1))); // [index / 64] &= index % 64
    return *this;
}

template<size_t N> Mask<N>& Mask<N>::toggle(size_t index) {
    flags[index >> 6] ^= (1ull << uint64_t(index & (64 - 1))); // [index / 64] ^= index % 64
    return *this;
}

template<size_t N> bool Mask<N>::check(size_t index) const {
    return (flags[index >> 6] >> uint64_t(index & (64 - 1))) & 1ull; // ([index / 64] >> index % 64) & 1
}

template<size_t N> size_t Mask<N>::next() const {
    for(size_t i = 0; i < N; ++i) {
        // check [i] != 0xFF...FF
        if(flags[i] != uint64_t(-1)) {
            int cnt = global::bit_ctz(~flags[i]); // find zero
            // i * 64 + found index
            return size_t((i << 6) + size_t(cnt));
        }
    }
    return size_t(-1); // not found
}

}