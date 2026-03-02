#ifndef CORE_FLAGS_HPP
#    include "flags.hpp"
#endif

namespace core {

template<bool BASE> void Flags<0, BASE>::on(uint64_t index) {
    flags[index >> 6] |= (1ull << uint64_t(index & (64 - 1)));
}

template<bool BASE> void Flags<0, BASE>::off(uint64_t index) {
    flags[index >> 6] &= ~(1ull << uint64_t(index & (64 - 1)));
}

template<bool BASE> void Flags<0, BASE>::toggle(uint64_t index) {
    flags[index >> 6] ^= (1ull << uint64_t(index & (64 - 1)));
}

template<bool BASE> bool Flags<0, BASE>::check(uint64_t index) const {
    return (flags[index >> 6] >> uint64_t(index & (64 - 1))) & 1ull;
}

template<bool BASE> size_t Flags<0, BASE>::find(size_t bits) const {
    size_t bytes = Aligner::count(bits, 64);

    for(size_t i = 0; i < bytes; ++i) {
        // check [i] != 0xFF...FF
        if(flags[i] != uint64_t(-1)) {
            int cnt = global::bit_ctz(~flags[i]); // find zero
            // i * 64 + found index
            return size_t((i << 6) + size_t(cnt));
        }
    }
}

template<bool BASE> void Flags<0, BASE>::zero(size_t bits) {
    size_t bytes = Aligner::count(bits, 64);

    for (size_t i = 0; i < bytes; ++i) {
        flags[i] = 0;
    }
}

size_t Flags<1, true>::next() const {
    this-> template find(1);
}

void Flags<1, true>::reset() {
    this-> template zero(1);
}

template<size_t N> size_t Flags<N, true>::next() const {
    this-> template find(N);
}

template<size_t N> void Flags<N, true>::reset() {
    this-> template zero(N);
}

} // namespace core
