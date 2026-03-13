#include "../core/bitmask.hpp"

namespace util {

template<size_t BIT, typename T = int, bool = false> class Bitset;

template<size_t BIT, typename T> class Bitset<BIT, T, false> : public Bitset<core::Bitmask::words<T>(BIT), T, true> {
    using Impl = Bitset<core::Bitmask::words<T>(BIT), T, true>;
    
public:
    static constexpr size_t MAX = BIT;
    
public:
    inline void   on    (size_t idx)       { if(idx >= BIT) std::abort(); else Impl::on(idx); }
    inline void   off   (size_t idx)       { if(idx >= BIT) std::abort(); else Impl::off(idx); }
    inline void   toggle(size_t idx)       { if(idx >= BIT) std::abort(); else Impl::toggle(idx); }
    inline bool   check (size_t idx) const { if(idx >= BIT) std::abort(); else return Impl::check(idx); }
};

template<size_t SIZE, typename Word> class Bitset<SIZE, Word, true> : protected core::Bitmask {
public:
    /**
     * @param  [in] idx index
     */
    void on(size_t idx);

public:
    /**
     *@param  [in] idx index
     */
    void off(size_t idx);

public:
    /**
     * @param  [in] idx index
     */
    void toggle(size_t idx);

public:
    /**
     * @param  [in] idx index
     * @return get flag state
     */
    bool check(size_t idx) const;
    
public:
    /**
     * @return first index, -1 is haven't slot
     */
    size_t next();

protected:
    Word flags[SIZE] = { 0 };
};

}

namespace util {

template<size_t SIZE, typename Word> void Bitset<SIZE, Word, true>::on(size_t index) {
    Bitmask::set(flags, index);
}

template<size_t SIZE, typename Word> void Bitset<SIZE, Word, true>::off(size_t index) {
    Bitmask::unset(flags, index);
}

template<size_t SIZE, typename Word> void Bitset<SIZE, Word, true>::toggle(size_t index) {
    Bitmask::flip(flags, index);
}

template<size_t SIZE, typename Word> bool Bitset<SIZE, Word, true>::check(size_t index) const {
    return Bitmask::test(flags, index);
}

template<size_t SIZE, typename Word> size_t Bitset<SIZE, Word, true>::next() {
    return Bitmask::ffz(flags, SIZE << Operator<Word>::SHIFT);
}

}

