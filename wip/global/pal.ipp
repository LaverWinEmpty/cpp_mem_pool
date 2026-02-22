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

template<> void* pal_valloc<void>(size_t byte, size_t align) noexcept {
    byte = bit_align(byte,  4096);

    if constexpr(CHECK_TARGET(OS_WINDOWS))
        align = bit_align(align, 65536); // VirtualAlloc need alignment of 64KiB
    else align = bit_align(align, 4096);

    // protect overflow
    if(byte > (~size_t(0) - (align * 2))) {
        return nullptr; // invalid
    }
    void* ptr = nullptr;

#if CHECK_TARGET(OS_WINDOWS)
    static auto valloc2 =
        (void*(__stdcall*)(void*, void*, size_t, uint32_t, uint32_t, void*, uint32_t))
        GetProcAddress(GetModuleHandleA("kernelbase.dll"), "VirtualAlloc2");

    // check callable VirtualAlloc2
    if(valloc2) {
        // MEM_ADDRESS_REQUREMENTS binary layout
        struct {
            void*  l;
            void*  h;
            size_t n;
        } addr = { NULL, NULL, align };

        // MEM_EXTENDED_PARAMETER binary layout
        struct {
            uint64_t type; // uint64_t : 8 == uint64_t (windows is LE)
            union {
                uint64_t n;
                void*    ptr;
            };
        } param = { 1, 0 };
        param.ptr = &addr;

        // MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE
        return valloc2(GetCurrentProcess(), nullptr, byte, 0x1000 | 0x2000, 0x4, &param, 1);
    }

    // fallback
    // param: MEM_RESERVE, PAGE_NOACCESS
    ptr = VirtualAlloc(nullptr, byte + align, 0x2000, 0x1);
    if(ptr) {
        ptr = reinterpret_cast<void*>(bit_align(uint64_t(ptr), align));
        // param:  MEM_COMMIT, PAGE_READWIRTE
        ptr = VirtualAlloc(ptr, byte, 0x1000, 0x4);
    }
    // else return nullptr

#elif CHECK_TARGET(OS_POSIX)
    const size_t ALLOC = bit_align(byte + align, align); // with address alignment size

    // use mmap: address are manually alinged to 64KiB
    ptr = mmap(NULL, ALLOC, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(ptr == MAP_FAILED) {
        return nullptr;
    }

    uintptr_t allocated = uintptr_t(ptr);                  // casting
    uintptr_t aligned   = bit_align(allocated, align); // 64KB align (ptr to return)
    uintptr_t moved     = aligned - allocated;             // moved
    uintptr_t remained  = ALLOC - byte - moved;            // remained

    if(moved) {
        munmap(ptr, moved); // trim front if aligned
    }
    munmap(reinterpret_cast<char*>(aligned + byte), remained); // trim back
    ptr = reinterpret_cast<void*>(aligned);                    // position rollback

#else
    // use default malloc
    void* src = nullptr; // real address

    src = malloc(byte + align + sizeof(void*)); // call malloc
    if(src) {
        // aligned pointer
        ptr = reinterpret_cast<void*>(bit_align(reinterpret_cast<uintptr_t>(src), align)); // first align

        // lack
        if(ptrdiff_t(reinterpret_cast<char*>(ptr) - reinterpret_cast<char*>(src)) < sizeof(void*)) {
            ptr = reinterpret_cast<char*>(ptr) + align; // second align: make space for metadata pointer
        }

        // embed source address
        *(reinterpret_cast<void**>(ptr) - 1) = src;
    }

#endif
    return ptr;
}

template<typename T> T* pal_valloc(size_t byte, size_t align) noexcept {
#if CPP_VER >= CPP_17
        return std::launder(static_cast<T*>(pal_valloc<void>(byte, align)));
#else
        return static_cast<T*>(pal_valloc<void>(byte, align));
#endif
}

void pal_vfree(void* ptr, size_t byte, size_t align) noexcept {
    if(!ptr) return;

#if CHECK_TARGET(OS_WINDOWS)
    static bool is2 = GetProcAddress(GetModuleHandleA("kernelbase.dll"), "VirtualAlloc2") != nullptr;

   if(!is2) {
        // MEMORY_BASIC_INFORMATION binary layout
        struct {
            void*    base;
            void*    allocated;
            uint32_t guard;
            int32_t  id;
            size_t   size;
            uint32_t state;
            uint32_t protect;
            uint32_t type;
        } info;
        VirtualQuery(ptr, reinterpret_cast<MEMORY_BASIC_INFORMATION*>(&info), sizeof(info));
        ptr = info.allocated;
   }
   VirtualFree(ptr, 0, 0x8000); // param: MEM_RELEASE

#elif CHECK_TARGET(OS_POSIX)
    munmap(ptr, bit_align(byte, align)); // alignment to 4096

#else
    free(*(reinterpret_cast<void**>(ptr) - 1));
#endif
}

} // namespace global
