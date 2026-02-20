namespace global {

CXX_INLINE void pal_pause() noexcept {
#if CHECK_TARGET(COMP_MSVC | ARCH_X86)
    _mm_pause();

#elif CHECK_TARGET(COMP_MSVC | ARCH_ARM)
    __yield();

#elif CHECK_TARGET(ARCH_X86) && (TARGET_COMP & (COMP_CLANG | COMP_GCC))
    __asm__ __volatile__("pause" ::: "memory");

#elif CHECK_TARGET(ARCH_ARM) && (TARGET_COMP & (COMP_CLANG | COMP_GCC))
    __asm__ __volatile__("yield" ::: "memory");

#else
    __asm__ __volatile__("" ::: "memory");
#endif
}

template<> void* pal_valloc<void>(size_t in) noexcept {
    static constexpr size_t ALIGNMENT = 64 * 1024;                    // 64KiB alignment
    static constexpr size_t SAFETY    = size_t(-1) - (ALIGNMENT * 2); // protect overflow

    //! @TODO:
    const size_t BYTE = bit_align(in, 0); // 4KiB alignment, to byte
    if(BYTE > SAFETY) {
        return nullptr; // invalid
    }
    void* ptr = nullptr;

#if CHECK_TARGET(OS_WINDOWS)
    // TODO
    using Cast = void* __stdcall (*)(void*, void*, size_t, unsigned long, unsigned long, void*, unsigned long)
    auto f = (Cast)GetProcAddress(GetModuleHandleA("kernel32.dll"), "VirtualAlloc2");

    // check callable VirtualAlloc2
    if(f) {
        // MEM_ADDRESS_REQUREMENTS binary layout
        struct {
            void*  l;
            void*  h;
            size_t n;
        } addr = { NULL, NULL, ALIGNMENT };

        // MEM_EXTENDED_PARAMETER binary layout
        struct {
            uint64_t type; // uint64_t : 8 == uint64_t (windows is LE)
            union {
                uint64_t n;
                void*    ptr;
            }
        } param = { 1, 0 };
        param.ptr = &addr;

        return _VirtualAlloc2(
            GetCurrentProcess(),
            NULL,
            size,
            MEM_RESERVE | MEM_COMMIT, 
            PAGE_READWRITE,
            &param,
            1
        );

        // MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE
        return f(GetProcAddress(), nullptr, size, 0x1000 | 0x2000, 0x4, &param, 1);
    }

    // use VirtualAlloc: address are automatically aligned to 64KiB
    // MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE
    ptr = VirtualAlloc(nullptr, BYTE, 0x1000 | 0x2000, 0x04);




#elif CHECK_TARGET(OS_POSIX)
    const size_t ALLOC = bit_align(BYTE + ALIGNMENT, ALIGNMENT); // with address alignment size

    // use mmap: address are manually alinged to 64KiB
    ptr = mmap(NULL, ALLOC, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(ptr == MAP_FAILED) {
        return nullptr;
    }

    uintptr_t allocated = uintptr_t(ptr);                  // casting
    uintptr_t aligned   = bit_align(allocated, ALIGNMENT); // 64KB align (ptr to return)
    uintptr_t moved     = aligned - allocated;             // moved
    uintptr_t remained  = ALLOC - BYTE - moved;            // remained

    if(moved) {
        munmap(ptr, moved); // trim front if aligned
    }
    munmap(reinterpret_cast<char*>(aligned + BYTE), remained); // trim back
    ptr = reinterpret_cast<void*>(aligned);                    // position rollback

#else
    // use default malloc
    void* src = nullptr; // real address

    src = malloc(BYTE + ALIGNMENT + sizeof(void*)); // call malloc
    if(src) {
        // aligned pointer
        ptr = reinterpret_cast<void*>(bit_align(reinterpret_cast<uintptr_t>(src), ALIGNMENT)); // first align

        // lack
        if(ptrdiff_t(reinterpret_cast<char*>(ptr) - reinterpret_cast<char*>(src)) < sizeof(void*)) {
            ptr = reinterpret_cast<char*>(ptr) + ALIGNMENT; // second align: make space for metadata pointer
        }

        // embed source address
        *(reinterpret_cast<void**>(ptr) - 1) = src;
    }

#endif
    return ptr;
}

template<typename T> T* pal_valloc(size_t in) noexcept {
    return static_cast<T*>(pal_valloc<void>(in));
}

void pal_vfree(void* ptr, size_t in) noexcept {
    if(!ptr) return;

#if CHECK_TARGET(OS_WINDOWS)
    // MEM_RELEASE
    VirtualFree(ptr, 0, 0x00008000);
#elif CHECK_TARGET(OS_POSIX)
    munmap(ptr, bit_align(ptr, 4096)); // alignment to 4096
#else
    free(*(reinterpret_cast<void**>(ptr) - 1));
#endif
}

} // namespace global
