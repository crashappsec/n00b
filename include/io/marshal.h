#pragma once
#include "n00b.h"

// Temporarilly calling this all 'pickle' until we excise old marshal.

typedef struct n00b_pickle_wl_item_t {
    n00b_alloc_hdr               *src;
    n00b_alloc_hdr               *dst;
    int64_t                       cur_offset;
    int64_t                       next_offset;
    struct n00b_pickle_wl_item_t *next;
} n00b_pickle_wl_item_t;

typedef struct {
    n00b_mutex_t lock;
    n00b_dict_t *memos;
    n00b_dict_t *needed_patches;
#if defined(N00B_ADD_ALLOC_LOC_INFO)
    n00b_dict_t *file_cache;
#endif
    n00b_buf_t            *current_output_buf;
    n00b_list_t           *pieces;
    n00b_list_t           *buffered_heap_info;
    n00b_pickle_wl_item_t *wl;
    uint64_t               base;
    uint64_t               offset;
    uint64_t               last_offset_end;
    uint64_t               min_heap_len;
    bool                   started;
    bool                   closed;
    n00b_gc_root_info_t   *root_entry;
} n00b_pickle_ctx;

typedef struct n00b_alloc_record_t n00b_marshaled_hdr;

// When we're on the unmarshaling side, we buffer until we see the end
// of a 'message'. For each sent message, we allocate a new memory
// region to store the results. So the 'virtual' heap is not
// contiguous on the unmarshaling side if broken up into messages.  As
// a result, we keep a list of the virtual address offset to where
// the heap starts; we binary search through it as necessary.

typedef struct {
    uint64_t vaddr_start_offset;
    uint64_t vaddr_end_offset;
    char    *true_segment_loc;
} n00b_vaddr_map_t;

typedef struct {
    n00b_buf_t          *partial;
    // Keep track of the most recent allocation record start when
    // we're buffering.
    char                *part_cursor;
    n00b_list_t         *vaddr_info;
    n00b_mutex_t         lock;
    uint64_t             vaddr_start;
    // For the current object we're unmarshaling, what's our offset
    // from vaddr_start? This should map to the front of the buffer
    // passed to n00b_unpickle_one.
    uint64_t             cur_record_start_offset;
    // This maps to the end of the virtual address space used for one
    // object unmarshal. It should tell us where the end record lives.
    uint64_t             cur_record_end_offset;
    char                *cur_record;
    n00b_gc_root_info_t *root_entry;
} n00b_unpickle_ctx;

typedef union {
    n00b_pickle_ctx   pickle;
    n00b_unpickle_ctx unpickle;
} n00b_marshal_filter_t;

// Note that the magic value is intended to act as a version
// number. As we change things about the memory format, we should
// be changing the magic value appropriately.
//
// The left-most 24 bits are intended to be a proper version
// number. Subtract 0xc0cac0 to get the version number.  The
// remainder of the bits (starting from the right) indicate
// compile-time variations of the memory layout.
//
// In this, the first implementaiton, if N00B_ALLOC_LOC_INFO is on
// during marshalling, we flip the rightmost bit.  And if
// N00b_FULL_MEMCHECK is on, we flip the second bit.
//
// We are not currently accomodating incompatibilities (even though we
// could with more memory munging).
#define N00B_MARSHAL_RECORD_GUARD 0x13addbbeab3ddddaULL
// #define N00B_MARSHAL_RECORD_GUARD 0xccccccccccccccccULL
#define N00B_MARSHAL_MAGIC_BASE   0xc0cac01ab0ba1ceeULL
// For compat w/ original version, until it is excised.
#define N00B_MARSHAL_MAGIC        N00B_MARSHAL_MAGIC_BASE

extern n00b_buf_t         *n00b_automarshal(void *);
extern void               *n00b_autounmarshal(n00b_buf_t *);
extern n00b_filter_spec_t *n00b_filter_marshal(bool);
n00b_filter_spec_t        *n00b_filter_unmarshal(bool);
extern n00b_buf_t         *n00b_autopickle(void *);
extern void               *n00b_autounpickle(n00b_buf_t *);
