#ifdef _MSC_VER
#    include <intrin.h>
#endif
#ifdef _WIN32
#    include <windows.h>
#else
#    include <sys/mman.h>
#    include <unistd.h>
#endif
#include <iostream>
#include <vector>
#include <thread>

#include <cassert>

#ifdef NDEBUG
#    define DEBUG 0
#else
#    define DEBUG 1
#endif

// SPIN LOCK
#include <lwe/async/lock.hpp>

static thread_local const std::thread::id THIS_THREAD = std::this_thread::get_id();

#define FOR_POOL(X) \
    X(8)            \
    X(16)           \
    X(32)           \
    X(64)           \
    X(128)          \
    X(256)          \
    X(512)          \
    X(1024)

class Fatal {
public:
    template<typename T> static void initialize() {
        if(!instance) {
            std::atexit([]() { delete Fatal::instance; });
        }
        else delete instance;
        instance = new T();
    }
public:
    static void call(const char* msg) noexcept {
        if(instance) {
            instance->proc(msg);
        }
        else std::terminate(); // defualt: shutdown
    }
protected:
    virtual void proc(const char* msg) = 0;
    virtual ~Fatal()                   = default;
    Fatal()                            = default;
private:
    static Fatal* instance;
};
Fatal* Fatal::instance;

#define CRASH(msg)                               \
    do {                                         \
        if(DEBUG) throw std::runtime_error(msg); \
        else Fatal::call(msg);                   \
    }                                            \
    while(false)

template<size_t N> class Mask {
public:
    void on(size_t index) {
        bits[index >> 6] |= (1ull << uint64_t(index & (64 - 1))); // [index / 64] |= index % 64
    }
    void off(size_t index) {
        bits[index >> 6] &= ~(1ull << uint64_t(index & (64 - 1))); // [index / 64] &= index % 64
    }
    void toggle(size_t index) {
        bits[index >> 6] ^= (1ull << uint64_t(index & (64 - 1))); // [index / 64] ^= index % 64
    }
    bool test(size_t index) {
        return (bits[index >> 6] >> uint64_t(index & (64 - 1))) & 1ull; // ([index / 64] >> index % 64) & 1
    }
public:
    size_t next() const {
        for(int i = 0; i < N; ++i) {
            if(bits[i] != uint64_t(-1)) {
#ifdef _MSC_VER
                // Win32 API
                unsigned long index;
                if(_BitScanForward64(&index, ~bits[i])) {
                    return size_t((i << 6) + index);
                }
#else
                // GCC / clang
                uint64_t inv = ~bits[i];
                return size_t((i << 6) + (inv ? __builtin_ctzll(inv) : 0));
#endif
            }
        }
        return size_t(-1);
    }

public:
    uint64_t bits[N] = { 0 };
};
using Bit256 = Mask<4>;

/*
    [memory layout]
    block
    +-------+-------+-----------+--------+
    | chunk | chunk | remainder | Footer |
    +-------+-------+-----------+--------+
    ^       ^                            ^
    0x0000  0x0000 + size                0xFFFF

    (ptr & 0xFFFF) / size => index
    (ptr & ~0xFFFF) -> block address

    ! ALIGNMENT SUPPORT: data starts at 0x0000

    [proof]
    - B: BlockSize(64KB), P: Pointer(8B), S: ObjectSize(aligned 8n)
    - N: Max objects (Calculated by: 8(B-P) / (8S+1))
    - R: Remainder bits (8(B-P) % (8S+1))

    * Identity: 8(B-P) = N(8S+1) + R  =>  B = NS + P + (N+R)/8
    * Since (N+R) is a multiple of 64, (N+R)/8 exactly matches the 8-byte aligned bitmask size.
    * This guarantees the total size never exceeds B(64KiB).

    [test]
    - block: 64 KiB -> 65536 byte -> 52488 bits,
      524288 - 256 (metadata) = 524224

    Calculation:
    ! bits   => chunk size bits + bit flag (1 bit per slot)
    ! amount => 524224 / bits, round down
    ! mask   => ceil(amount / 64) * 8 bytes (bitmap size)

    param | bits | amount              | bit mask  | total
    ------+------+---------------------+-----------+------------
        8 |   65 |  8062 (64496 byte)  | 1008 byte | 65504 byte
       16 |  129 |  4062 (64992 byte)  |  512 byte | 65504 byte
       24 |  193 |  2715 (65160 byte)  |  344 byte | 65504 byte
       32 |  257 |  2039 (65248 byte)  |  256 byte | 65504 byte
       40 |  321 |  1632 (65280 byte)  |  208 byte | 65488 byte
       48 |  385 |  1361 (65328 byte)  |  176 byte | 65504 byte
       56 |  449 |  1167 (65352 byte)  |  152 byte | 65504 byte
       64 |  513 |  1021 (65344 byte)  |  128 byte | 65472 byte
       72 |  577 |   908 (65376 byte)  |  120 byte | 65496 byte
       80 |  641 |   817 (65360 byte)  |  104 byte | 65464 byte
       88 |  705 |   743 (65384 byte)  |   96 byte | 65480 byte
       96 |  769 |   681 (65376 byte)  |   88 byte | 65464 byte
      104 |  833 |   629 (65416 byte)  |   80 byte | 65496 byte
      112 |  897 |   584 (65408 byte)  |   80 byte | 65488 byte
      120 |  961 |   545 (65400 byte)  |   72 byte | 65472 byte
      128 | 1025 |   511 (65408 byte)  |   64 byte | 65472 byte
*/

class Pool {
    static constexpr size_t BLOCK = 64 * 1024; // byte == 64KiB

    union Block;
    struct Meta {
        size_t used  = 0;
        Pool*  outer = 0;
        Block* next  = 0;
        Block* prev  = 0;
    };

    //! @tparam chunk size byte
    //! @note constructor not   call, set zero memory
    template<size_t BYTE> struct Preset {
        //! @brief the block total bits / data + flag bits
        static constexpr size_t MAX = (BLOCK - sizeof(Meta)) * 8 / (BYTE * 8 + 1);

        //! @brief object count to byte, divied to sizeof(uint_64), and round up
        using State = Mask<(MAX + 63) / 64>;

        uint8_t data[BLOCK - sizeof(State) - sizeof(Meta)];
        State   state;
        Meta    meta;

        // cast to Block
        Block* cast() { return reinterpret_cast<Block*>(this); }
    };

    union Block {
#define BLOCK_MACRO_UNION(X) Preset<X> _##X;
        FOR_POOL(BLOCK_MACRO_UNION);
#undef BLOCK_MACRO_UNION

        //! @brief memset wrapper for placement new
        Block(Pool* in) {
            // active member check (for prevent UB)
            switch(in->CHUNK) {
#define BLOCK_MACRO_CONSTRUCT(X)          \
    case X: new(this) Preset<X>(); break;
                FOR_POOL(BLOCK_MACRO_CONSTRUCT);
#undef BLOCK_MACRO_CONSTRUCT
            }
            meta()->outer = in;
        }
        ~Block() = default;

        // layout
        // uint8_t[BLCOK - sizeof(Meta)]
        // Meta
        Meta* meta() {
            // cast Preset to Meta, protects type pun UB.
            return std::launder(reinterpret_cast<Meta*>(reinterpret_cast<uint8_t*>(this) + BLOCK - sizeof(Meta)));
        }

        // get bit mask
        template<size_t N> typename Preset<N>::State* state() {
            if constexpr(false) { } // if-else chain
#define BLOCK_MACRO_STATE(X)    \
    else if constexpr(N == X) { \
        return &_##X.state;     \
    }
            FOR_POOL(BLOCK_MACRO_STATE);
#undef BLOCK_MACRO_STATE
            return nullptr;
        }
    };

    static Block* from(void* in) { return reinterpret_cast<Block*>(uintptr_t(in) & ~0xFFFF); }

public:
    size_t indexing(void* in) { return (uintptr_t(in) & 0xFFFF) / CHUNK; }

protected:
    Block* generate() {
        void* ptr;
#ifdef _WIN32
        ptr = VirtualAlloc(nullptr, BLOCK, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
        ptr = mmap(NULL, BLOCK * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 // file
        );

        if(ptr == reinterpret_cast<void*>(-1)) {
            return nullptr;
        }

        uintptr_t address = uintptr_t(ptr);                       // casting
        uintptr_t aligned = (address + BLOCK - 1) & ~(BLOCK - 1); // 64KB align

        if(aligned > address) {
            munmap(ptr, aligned - address); // trim front
        }
        ptr = reinterpret_cast<void*>(aligned + BLOCK); // remainder
        munmap(ptr, BLOCK - (aligned - address));       // trim back

        ptr = reinterpret_cast<void*>(aligned); // -= BLOCK
#endif
        if(ptr) {
            new(ptr) Block(this); // init
        }
        return static_cast<Block*>(ptr);
    }

private:
    void destroy(Block* ptr) {
        Meta* meta = ptr->meta();
        // check valid
        if(indexing(ptr) != 0) CRASH("INVALID PARAMETER");
        // check pool
        if(meta->outer != this) CRASH("POOL MISMATCH");
        // check thread
        if(meta->outer->OUTER != std::this_thread::get_id()) CRASH("THREAD MISMATCH");

        ptr->~Block(); // trivial destructor: placement new pair

#ifdef _WIN32
        VirtualFree(ptr, 0, MEM_RELEASE);
#else
        munmap(ptr, BLOCK);
#endif
    }

private:
    static constexpr size_t alginer(size_t chunk) {
        // TODO: 비트연산으로 변경
        if(chunk <= 8) return 8;
        if(chunk <= 16) return 16;
        if(chunk <= 32) return 32;
        if(chunk <= 64) return 64;
        if(chunk <= 128) return 128;
        if(chunk <= 256) return 256;
        if(chunk <= 512) return 512;
        if(chunk <= 1024) return 1024;
        return chunk;
    }

    static constexpr size_t limiter(size_t chunk) {
        // TODO: 비트연산으로 변경
        if(chunk <= 8) return Preset<8>::MAX;
        if(chunk <= 16) return Preset<16>::MAX;
        if(chunk <= 32) return Preset<32>::MAX;
        if(chunk <= 64) return Preset<64>::MAX;
        if(chunk <= 128) return Preset<128>::MAX;
        if(chunk <= 256) return Preset<256>::MAX;
        if(chunk <= 512) return Preset<512>::MAX;
        if(chunk <= 1024) return Preset<1024>::MAX;
        return 0;
    }

public:
    Pool(size_t chunk): CHUNK(alginer(chunk)), MAX(limiter(CHUNK)), OUTER(THIS_THREAD) { }

public:
    template<typename T = void, typename... Args> T* allocate(Args&&... in) {
        if(void* ptr = allocate<void>()) {
            if constexpr(sizeof...(Args) != 0) {
                return new(ptr) T(std::forward<Args>(in)...);
            }
            else return new(ptr) T();
        }
        return nullptr;
    }

    template<> void* allocate<void>() {
        if(MAX == 0) {
            return malloc(CHUNK);
        }
        // check block
        if(!current) {
            current = used.next(); // recycle (first)
            if(!current) {
                current = full.next(); // recycle (second)
                // last: alloc
                if(!current) {
                    current = generate();
                    if(!current) {
                        return nullptr; // failed
                    }
                }
            }
        }

        // check state
        size_t index = size_t(-1);
        switch(CHUNK) {
#define BLOCK_MACRO_ON(X)                 \
    case X: {                             \
        auto state = current->state<X>(); \
        index      = state->next();       \
        state->on(index);                 \
    } break; // get state and check
            FOR_POOL(BLOCK_MACRO_ON);
#undef BLOCK_MACRO_ON
        }

        // return
        void* out = reinterpret_cast<uint8_t*>(current) + index * CHUNK;

        // get meta, and MAX to index
        Meta* meta = current->meta();
        if(++meta->used > MAX - 1) {
            empty.push(current); // check overflow, and set empty state
            current = nullptr;
        }
        return out;
    }

public:
    template<typename T> void release(T* in) {
        in->~T();
        release<void>(in);
    }

    template<> void release<void>(void* in) {
        if(MAX == 0) {
            free(in);
            return;
        }

        // get block info
        Block*    block = from(in);
        Meta*     meta  = block->meta();
        ptrdiff_t index = (uintptr_t(in) & 0xFFFF) / CHUNK;

        // check pool
        if(meta->outer != this) CRASH("POOL MISMATCH");

        // check thread
        if(meta->outer->OUTER != THIS_THREAD) CRASH("THREAD MISMATCH");

        // uncheck state
        switch(CHUNK) {
#define BLOCK_MACRO_OFF(X)             \
    case X: {                          \
        block->state<X>()->off(index); \
    } break; // get state and off
            FOR_POOL(BLOCK_MACRO_OFF);
#undef BLOCK_MACRO_OFF
        }

        // current -> ignore state changes
        if(block != current) {
            // MAX(overflow) -> MAX - 1
            if(meta->used == MAX) {
                empty.pop(block); // empty to
                used.push(block); // used
            }
            // 1 -> 0
            if(meta->used == 1) {
                used.pop(block);  // used to
                full.push(block); // full
            }
        }
        --meta->used; // decount
    }

public:
    size_t reserve(size_t cnt) {
        if(CHUNK > 1024) return 0;

        size_t n = 0;
        while(n++ < cnt) {
            Block* block = generate();
            if(!current) {
                return n - 1; // failed
            }
            full.push(block); // insert
        }
        return n; // create count
    }

public:
    size_t shrink(size_t in = size_t(-1)) {
        size_t cnt = 0;
        Block* del = full.next(); // pop

        for(size_t i = 0; i < in && del; ++i) {
            Block* temp = full.next(); // pop
            destroy(del);              // delete
            del = temp;                // set next
            ++cnt;
        }
        return cnt;
    }

public:
    bool destructible() { return current->meta()->used == 0 && used.head == nullptr && empty.head == nullptr; }

public:
    ~Pool() {
        List* list[3] = { &full, &empty, &used };
        for(int i = 0; i < 3; ++i) {
            List& curr = *list[i];

            Block* del = curr.next(); // pop
            while(del != nullptr) {
                Block* temp = curr.next(); // pop
                destroy(del);              // delete
                del = temp;                // set next
            }
        }
        if(current) {
            destroy(current);
        }
    }

public:
    //! @brief Local Thread Storedge
    template<size_t CHUNK>
    static Pool* lts() {
        static thread_local Pool pool(CHUNK);
        return &pool;
    }

private:
    // for Allocator
    template<typename T> friend class Allocator;
    //! @biref for global pool
    class Singleton;
    //! @brief get global pool
    template<size_t> static Singleton* singleton();

private:
    class List {
    public:
        void push(Block* in) {
            Meta* meta = in->meta();
            meta->prev = nullptr;
            meta->next = head; // push front
            if(head) {
                head->meta()->prev = in; // link
            }
            head = in; // new head
        }

        void pop(Block* in) {
            Meta*  meta = in->meta();
            Block* prev = meta->prev;
            Block* next = meta->next;
            if(prev) prev->meta()->next = next;
            if(next) next->meta()->prev = prev;
            if(in == head) head = next;
        }

        Block* next() {
            Block* out = head;
            if(out) {
                Meta* meta = out->meta();
                head       = meta->next;
                meta->next = nullptr;
                meta->prev = nullptr;
            }
            return out;
        }

        Block* head = nullptr;
    };
    List used;
    List empty;
    List full;

public:
    Block* current;

public:
    const size_t          CHUNK;
    const size_t          MAX;
    const std::thread::id OUTER;
};

inline static constexpr uint64_t align(uint64_t in) noexcept {
    if(in <= 1) {
        return 1;
    }
    in -= 1;
    for(uint64_t i = 1; i < sizeof(uint64_t); i <<= 1) {
        in |= in >> i;
    }
    return in + 1;
}

class Pool::Singleton {
public:
    Singleton(size_t CHUNK): pool(CHUNK) { }
    Pool             pool;
    lwe::async::Lock spin; // normal lock
    std::mutex       mtx;  // os call lock
};

template<size_t CHUNK> Pool::Singleton* Pool::singleton() {
    static Singleton instance{ CHUNK };
    return &instance;
}

template<typename T> class Allocator {
public:
    template<typename... Args> static T* allocate(Args&&... in) {
        // return instance.allocate<T>(std::forward<T>(in));

        static Pool&             pool = instance->pool;
        static LWE::async::Lock& spin = instance->spin;
        static std::mutex&       mtx  = instance->mtx;

        while(true) {
            LOCKGUARD(spin) {
                // check current block
                if((pool.current != nullptr) ||
                   (pool.current = pool.used.next()) ||
                   (pool.current = pool.full.next())) {
                    // not null -> returned immediately
                    return pool.allocate<T>(std::forward<Args>(in)...);
                }
            }

            //! OS CALL, use mutex
            LOCKGUARD(mtx) {
                // check and system call
                if(!pool.current) {
                    pool.current = pool.generate();
                    // OS CALL FAILED
                    if(pool.current == nullptr) {
                        return nullptr;
                    }
                }
            }
        }
    }

public:
    template<typename T = void> static void release(T* in) {
        static Pool&             pool = instance->pool;
        static LWE::async::Lock& spin = instance->lock;

        LOCKGUARD(spin) {
            pool.release(in);
        }
    }

private:
    static Pool::Singleton* instance;
};

//! @brief shared instance using the same chunk size
template<typename T> Pool::Singleton* Allocator<T>::instance = Pool::singleton<align(sizeof(T))>();

#undef FOR_POOL

// TODO


//! @brief memory align utility, with Allocator Mix-in base class
class Aligner {
public:
    //! @brief align to pointer size (4 or 8 Byte)
    static constexpr size_t ptr(size_t in) {
        return global::bit_align(in, sizeof(void*));
    }
public:
    //! @brief align to page size (16 KiB)
    static constexpr size_t page(size_t in) {
        return global::bit_align(in, global::PAL_PAGE);
    }
public:
    //! @brief align to huge page baseline size (2 MiB)
    static constexpr size_t pmd(size_t in) {
        return global::bit_align(in, global::PAL_HUGEPAGE);
    }
public:
    //! @brief get next power of 2 (round up)
    static constexpr size_t ceil(size_t in) {
        return global::bit_pow2(in);
    }
public:
    //! @brief get previous power of 2 (round down)
    static constexpr size_t floor(size_t in) {
        if(global::bit_aligned(in)) {
            return in;
        }
        return global::bit_pow2(in >> 1);
    }
public:
    //! @brief get value to multifly for align (align function without bit operator)
    static constexpr size_t counter(size_t in, size_t align) {
        return (in + align - 1) / align;
    }
protected:
    //! @brief ghost struct for calculate offset
    struct Header {
        void*  next;  //!< next chunk ptr
        void*  prev;  //!< prev chunk ptr
        void*  outer; //!< ID: parent allocator ptr
        size_t used;  //!< counter
    };
protected:
    //! @brief get chunk size, guaranteed at least 15 blocks
    static constexpr size_t chunk(size_t block) {
        const size_t ALIGNED = global::bit_pow2(block);
        size_t size = ALIGNED * 15;
        
        // min is 64KiB: 15 blocks with metadata based on 4 KiB
        if(size <= global::PAL_BOUNDARY) {
            return global::PAL_BOUNDARY;
        }
        return size + ALIGNED; // * 16
    }
protected:
    //! @brief get block count per chunk
    static constexpr size_t amount(size_t block) {
        // (claculated chunk reminder bits) / (block bits + mask 1 bits)
        return ((chunk(block) - sizeof(Header)) * 8) / (block * 8 + 1);
    }
protected:
    //! @brief aligned pointer cache by free-list style
    struct List {
        bool  remove(void*); //!< param: Header*
        bool  push(void*);   //!< param: Header*
        void* pop();         //!< return: Header*
        
        void* head;
    };
protected:
    //! @brief aligned pointer cache by pointer array
    struct Array {
        bool  remove(void*);
        bool  push(void*);
        void* pop();
        
        void** ptr = nullptr;
        size_t top = 0;
        size_t cap = 0;
    };
protected:
    Aligner() = default;
};

//! @note: guaranteed at least 15 blocks, memory overhead max 6.25%
template<size_t N> class Slab : protected Aligner {
public:
    static constexpr size_t BLOCK = ptr(N);
    static constexpr size_t CHUNK = chunk(BLOCK);
    static constexpr size_t UNIT  = amount(BLOCK);

protected:
    using State = core::Mask<counter(amount(BLOCK), 64)>;
    
    static constexpr size_t OFFEST =  counter(sizeof(Header) + sizeof(State), N) * N;
    
    struct Chunk;
    union Meta {
        Header _; //! unused
        struct {
            Chunk* next;
            Chunk* prev;
            Slab*  outer;
            size_t used;
        };
    };
    
    struct Chunk {
        Meta meta;
        State state;
        uint8_t block[CHUNK - sizeof(Header) - sizeof(State)];
    };
    
    using Cache = List;
    
public:
    template<typename T> T* acquire();
    
public:
    template<typename T> void release(T*);
    
public:
    size_t reserve(size_t);
    
public:
    size_t shirnk(size_t = 0);
    
public:
    size_t usable() const;
    
private:
    size_t counter;
    
private:
    Cache freeable;
    Cache partial;
    Cache
};

//! @note: guaranteed at least 64KiB, memory overhead max 20%
template<size_t N> class Bin : protected Aligner {
    static constexpr size_t BLOCK = page(N);
    static constexpr size_t CHUNK = BLOCK; // block as chunk
    static constexpr size_t UNIT  = 1;     // block per chunk is 1
    
    struct Chunk {
        uint8_t block[CHUNK];
    };
    
    using Cache = Array;
    
public:
    template<typename T> T* acquire();
    
public:
    template<typename T> void release(T*);
    
public:
    size_t reserve(size_t);
    
public:
    size_t shirnk(size_t = 0);
    
public:
    size_t usable() const;
    
private:
    size_t counter;
    
private:
    Cache freeable;
    Cache
};
