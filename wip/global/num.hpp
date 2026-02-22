namespace global {

constexpr size_t num_align(size_t in, size_t align) {
    return (in + align - 1) & ~(align - 1);
}

} // namespace global
