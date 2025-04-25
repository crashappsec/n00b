#include "hatrack/hash.h"
#include "hatrack/dict.h" // For the key types.

hatrack_hash_t
hatrack_hash_by_type(uint32_t               key_type,
                     hatrack_hash_func_t    custom,
                     hatrack_offset_info_t *offsets,
                     void                  *key)
{
    hatrack_hash_t hv;
    uint8_t       *loc_to_hash;

    switch (key_type) {
    case HATRACK_DICT_KEY_TYPE_OBJ_CUSTOM:
        return (*custom)(key);

    case HATRACK_DICT_KEY_TYPE_INT:
        return hash_int((uint64_t)key);

    case HATRACK_DICT_KEY_TYPE_REAL:
        return hash_double(*(double *)key);

    case HATRACK_DICT_KEY_TYPE_CSTR:
        return hash_cstr((char *)key);

    case HATRACK_DICT_KEY_TYPE_PTR:
        return hash_pointer(key);

    default:
        break;
    }

    if (!key) {
        return hash_pointer(key);
    }

    if (offsets->cache_offset != (int32_t)HATRACK_DICT_NO_CACHE) {
        hv = *(hatrack_hash_t *)(((uint8_t *)key) + offsets->cache_offset);

        return hv;
    }

    loc_to_hash = (uint8_t *)key;
    loc_to_hash += offsets->hash_offset;

    switch (key_type) {
    case HATRACK_DICT_KEY_TYPE_OBJ_INT:
        hv = hash_int((uint64_t)loc_to_hash);
        break;
    case HATRACK_DICT_KEY_TYPE_OBJ_REAL:
        hv = hash_double(*(double *)loc_to_hash);
        break;
    case HATRACK_DICT_KEY_TYPE_OBJ_CSTR:
        if (!loc_to_hash) {
            abort();
        }
        char *val = *(char **)loc_to_hash;

        if (val) {
            hv = hash_cstr(*(char **)loc_to_hash);
        }
        else {
            hv = hash_cstr("\0");
        }
        break;
    case HATRACK_DICT_KEY_TYPE_OBJ_PTR:
        hv = hash_pointer(loc_to_hash);
        break;
    default:
        hatrack_panic("invalid key type in hatrack_dict_get_hash_value");
    }

    if (offsets->cache_offset != (int32_t)HATRACK_DICT_NO_CACHE) {
        *(hatrack_hash_t *)(((uint8_t *)key) + offsets->cache_offset) = hv;
    }

    return hv;
}
