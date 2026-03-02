#ifndef MEM_ALIGNER_HPP
#    include "aligner.hpp"
#endif

struct Aligner::Header {
    Header* next;  //!< next chunk ptr
    Header* prev;  //!< prev chunk ptr
    void*   outer; //!< ID: parent allocator ptr
    size_t  used;  //!< counter
};

constexpr size_t Aligner::ptr(size_t in) {
    return global::bit_align(in, sizeof(void*));
}

constexpr size_t Aligner::page(size_t in) {
    if(in >= global::PAL_HUGEPAGE) {
        return global::bit_align(in, global::PAL_HUGEPAGE);
    }
    else return global::bit_align(in, global::PAL_PAGE);
}

constexpr size_t Aligner::boundary(size_t in) {
    if(in >= global::PAL_HUGEPAGE) {
        return global::bit_align(in, global::PAL_HUGEPAGE);
    }
    else return global::bit_align(in, global::PAL_BOUNDARY);
}

constexpr size_t Aligner::ceil(size_t in) {
    return global::bit_pow2(in);
}

constexpr size_t Aligner::floor(size_t in) {
    if(global::bit_aligned(in)) {
        return in;
    }
    return global::bit_pow2(in >> 1);
}

constexpr size_t Aligner::count(size_t in, size_t div) {
    return (in + div - 1) / div;
}

constexpr size_t Aligner::chunk(size_t block) {
    // gauranteed 15, place metadata in the remainder
    size_t size = ptr(block) * 16;

    // min is 64KiB: 15 blocks with metadata based on 4 KiB
    if(size <= global::PAL_BOUNDARY) {
        return global::PAL_BOUNDARY;
    }
    // if, (sizeof(Header) + sizeof(State)) >= 4KiB ? impossible.

    return ceil(boundary(size));
}

constexpr size_t Aligner::blocks(size_t chunk, size_t block) {
    // (calculated chunk remainder bits) / (block bits + mask 1 bits)
    return ((chunk - sizeof(Header)) * 8) / (block * 8 + 1);
}

constexpr size_t Aligner::offset(size_t chunk, size_t block) {
    size_t temp = blocks(chunk, block); // bit flag count

    // TODO: uint64_t -> Bitmask::Word
    size_t meta = sizeof(Header) + count(temp, sizeof(uint64_t)); 

    return count(meta, block) * block;
}

class Aligner::List {
public:
    bool remove(void* in) {
        Header* meta = static_cast<Header*>(in); // cast and get

        Header* prev = meta->prev; // temp
        Header* next = meta->next; // temp

        if(prev) prev->next = next; // skip this
        if(next) next->prev = prev; // skip this
        if(in == head) head = next; // skip head

        return true;
    }

    bool push(void* in) {
        Header* meta = static_cast<Header*>(in); // cast and get

        meta->prev = nullptr;
        meta->next = head; // push front
        if(head) {
            head->prev = meta; // link
        }
        head = meta; // new head

        return true;
    }

    void* pop() {
        Header* out = head;
        if(out) {
            head      = out->next;
            out->next = nullptr;
            out->prev = nullptr;
        }
        return static_cast<void*>(out);
    }

    bool empty() { return head == nullptr; }

    bool full() { return false; }

private:
    Header* head = nullptr;
};

class Aligner::Array {
public:
    bool remove(void* in) {
        for(int i = 0; i < top; ++i) {
            if(vec[i] == in) {
                --top;             // reduce
                vec[i] = vec[top]; // swap and delete
                return true;
            }
        }
        return false;
    }

    bool push(void* in) {
        static constexpr size_t EX = (global::PAL_PAGE) / sizeof(void*); // pointer counter

        size_t old = cap * sizeof(void*);
        if(top >= cap) {
            void** temp = global::pal_valloc<void*>(old + global::PAL_PAGE); // alloc
            if(!temp) {
                return false; // failed
            }

            // realloc
            if(vec) {
                std::memcpy(temp, vec, old); // copy
                global::pal_vfree(vec, old); // free
            }

            // new vector
            vec  = temp;
            cap += EX;
        }
        vec[top++] = in; // push
        return true;
    }

    void* pop() {
        if(top == 0) {
            return nullptr;
        }
        return vec[--top];
    }

    bool empty() { return top == 0; }

    bool full() { return top == cap; }

private:
    void** vec = nullptr;
    size_t top = 0;
    size_t cap = 0;
};

struct Aligner::Binder {
    void*  (*getter)(void*)        = nullptr;
    void   (*setter)(void*, void*) = nullptr;
    size_t CHUNK                   = 0;
};
