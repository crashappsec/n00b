#include "hatrack/hatring.h" // IWYU pragma: export

enum {
    HATRING_ENQUEUED = 0x8000000000000000,
    HATRING_DEQUEUED = 0x4000000000000000,
    HATRING_MASK     = 0xcfffffffffffffff
};

static inline bool
hatring_is_lagging(uint32_t read_epoch, uint32_t write_epoch, uint64_t size)
{
    if (read_epoch + size < write_epoch) {
        return true;
    }

    return false;
}

static inline uint32_t
hatring_enqueue_epoch(uint64_t ptrs)
{
    return (uint32_t)(ptrs >> 32);
}

static inline uint32_t
hatring_dequeue_epoch(uint64_t ptrs)
{
    return (uint32_t)(ptrs & 0x00000000ffffffff);
}

static inline uint32_t
hatring_dequeue_ix(uint64_t epochs, uint32_t last_slot)
{
    return (uint32_t)(epochs & last_slot);
}

static inline uint32_t
hatring_cell_epoch(uint64_t state)
{
    return (uint32_t)(state & 0x00000000ffffffff);
}

static inline bool
hatring_is_enqueued(uint64_t state)
{
    return state & HATRING_ENQUEUED;
}

static inline uint64_t
hatring_fixed_epoch(uint32_t write_epoch, uint64_t store_size)
{
    return (((uint64_t)write_epoch) << 32) | (write_epoch - store_size);
}
