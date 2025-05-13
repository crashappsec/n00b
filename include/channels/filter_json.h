#pragma once
#include "n00b.h"
#include "channels/filter.h"

n00b_list_t *n00b_chan_filter_to_json(void *cookie, void *msg);
n00b_list_t *n00b_chan_filter_from_json(void *cookie, void *msg);

extern void n00b_chan_filter_add_to_json_on_write(n00b_channel_t *c);
extern void n00b_chan_filter_add_to_json_on_read(n00b_channel_t *c);
extern void n00b_chan_filter_add_from_json_on_write(n00b_channel_t *c);
extern void n00b_chan_filter_add_from_json_on_read(n00b_channel_t *c);

extern n00b_filter_impl to_json_impl_write;
extern n00b_filter_impl to_json_impl_read;
extern n00b_filter_impl from_json_impl_write;
extern n00b_filter_impl from_json_impl_read; 