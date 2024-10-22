#pragma once
#include "n00b.h"

typedef struct n00b_exception_st       n00b_exception_t;
typedef struct n00b_exception_frame_st n00b_exception_frame_t;

struct n00b_exception_st {
    n00b_utf8_t      *msg;
    n00b_obj_t       *context;
    n00b_grid_t      *c_trace;
    n00b_exception_t *previous;
    int64_t          code;
    const char      *file;
    uint64_t         line;
};

struct n00b_exception_frame_st {
    jmp_buf               *buf;
    n00b_exception_t       *exception;
    n00b_exception_frame_t *next;
};

typedef struct {
    n00b_grid_t            *c_trace;
    n00b_exception_frame_t *top;
    n00b_exception_frame_t *free_frames;
} n00b_exception_stack_t;
