/*
 * Copyright © 2021-2022 John Viega
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *  Name:           test.c
 *
 *  Description:    Main file for test case running, along with
 *                  the function that pre-computes hash keys.
 *
 *
 *  Author:         John Viega, john@zork.org
 *
 */

#include <test/testhat.h>

#include <hatrack/hash.h>

#include <time.h>
#include <stdio.h>

hatrack_hash_t *precomputed_hashes      = NULL;
static size_t   precomputed_hashes_size = 0;
static uint64_t num_hashes              = 0;

// This is obviously meant to be run single-threaded.
void
precompute_hashes(uint64_t max_range)
{
    size_t alloc_size;

    if (num_hashes > max_range) {
        return;
    }

    alloc_size = sizeof(hatrack_hash_t) * max_range;

    if (!precomputed_hashes) {
        precomputed_hashes = (hatrack_hash_t *)hatrack_malloc(alloc_size);
    }
    else {
        precomputed_hashes
            = (hatrack_hash_t *)hatrack_realloc(precomputed_hashes,
                                                precomputed_hashes_size,
                                                alloc_size);
    }
    precomputed_hashes_size = alloc_size;

    for (; num_hashes < max_range; num_hashes++) {
        precomputed_hashes[num_hashes] = hash_int(num_hashes);
    }

    return;
}

#ifdef HATRACK_NO_PTHREAD
static pthread_key_t thread_pkey;

struct thread_data {
    size_t size;
    char   data[];
};

static void
thread_release_pthread(void *arg)
{
    pthread_setspecific(thread_pkey, NULL);

    struct thread_data *pt = arg;
    mmm_thread_release((mmm_thread_t *)pt->data);
    hatrack_free(arg, pt->size);
}

static mmm_thread_t *
thread_acquire_data(void *aux, size_t size)
{
    struct thread_data *pt = pthread_getspecific(thread_pkey);
    if (NULL == pt) {
        pt       = hatrack_zalloc(sizeof(struct thread_data) + size);
        pt->size = size;
        pthread_setspecific(thread_pkey, pt);
    }
    return (mmm_thread_t *)pt->data;
}

#endif

int
main(int argc, char *argv[])
{
#ifdef HATRACK_NO_PTHREAD
    pthread_key_create(&thread_pkey, thread_release_pthread);
    mmm_setthreadfns(thread_acquire_data, NULL);
#endif

    config_info_t *config;

    config = parse_args(argc, argv);

    if (config->run_custom_test) {
        run_performance_test(&config->custom);
    }

    if (config->run_func_tests) {
        run_functional_tests(config);
    }

    if (config->run_default_tests) {
        run_default_tests(config);
    }

    counters_output_alltime();

    printf("Press <enter> to exit.\n");
    getc(stdin);

    return 0;
}
