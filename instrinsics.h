int findFirstSignificantBit(uint32_t value) {
#ifdef _MSC_VER 
    unsigned long index = 0;
    _BitScanForward(&index, value);
    return index;
#else
    for (int i = 0; i < 32; i++) {
        if (value & (0x1 << i)) {
            return i;
        }
    }
    return 0;
#endif
}