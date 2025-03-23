#include "n00b.h"

static inline bool
us_written_date(n00b_string_t *input, n00b_date_time_t *result)
{
    n00b_string_t *month_part = n00b_cached_empty_string();
    int            ix         = 0;
    int            l          = n00b_string_byte_len(input);
    int            day        = 0;
    int            year       = 0;
    int            daylen;
    int            yearlen;
    int            start_ix;
    char          *s;

    if (!input || n00b_string_byte_len(input) == 0) {
        return false;
    }

    s = input->data;

    while (ix < l) {
        char c = *s;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            ++ix;
            ++s;
        }
        else {
            break;
        }
    }

    if (ix == 0) {
        return false;
    }

    month_part = n00b_string_lower(n00b_string_slice(input, 0, ix));

    while (ix < l) {
        if (*s == ' ') {
            ++ix;
            ++s;
        }
        else {
            break;
        }
    }

    start_ix = ix;

    while (ix < l) {
        switch (*s) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            day *= 10;
            day += *s - '0';
            ++ix;
            ++s;
            continue;
        case ' ':
        case ',':
            break;
        default:
            return false;
        }
        break;
    }

    daylen = ix - start_ix;

    if (ix < l && *s == ',') {
        s++;
        ix++;
    }

    while (ix < l) {
        if (*s != ' ') {
            break;
        }
        s++;
        ix++;
    }

    start_ix = ix;
    while (ix < l) {
        if (*s < '0' || *s > '9') {
            break;
        }
        year *= 10;
        year += *s - '0';
        ++ix;
        ++s;
    }

    if (year < 100) {
        year += 2000;
    }

    if (ix < l) {
        return false;
    }

    yearlen = ix - start_ix;

    if (daylen == 4 && yearlen == 0) {
        year = day;
        day  = 0;
    }

#define month_string_is(x) !strcmp(month_part->data, x)
    do {
        if (month_string_is("jan") || month_string_is("january")) {
            result->dt.tm_mon = 0;
            break;
        }
        if (month_string_is("feb") || month_string_is("february")) {
            result->dt.tm_mon = 1;
            break;
        }
        if (month_string_is("mar") || month_string_is("march")) {
            result->dt.tm_mon = 2;
            break;
        }
        if (month_string_is("apr") || month_string_is("april")) {
            result->dt.tm_mon = 3;
            break;
        }
        if (month_string_is("may")) {
            result->dt.tm_mon = 4;
            break;
        }
        if (month_string_is("jun") || month_string_is("june")) {
            result->dt.tm_mon = 5;
            break;
        }
        if (month_string_is("jul") || month_string_is("july")) {
            result->dt.tm_mon = 6;
            break;
        }
        if (month_string_is("aug") || month_string_is("august")) {
            result->dt.tm_mon = 7;
            break;
        }
        // clang-format off
        if (month_string_is("sep") || month_string_is("sept") ||
	    month_string_is("september")) {
            // clang-format on
            result->dt.tm_mon = 8;
            break;
        }
        if (month_string_is("oct") || month_string_is("october")) {
            result->dt.tm_mon = 9;
            break;
        }
        if (month_string_is("nov") || month_string_is("november")) {
            result->dt.tm_mon = 10;
            break;
        }
        if (month_string_is("dec") || month_string_is("december")) {
            result->dt.tm_mon = 11;
            break;
        }
        return false;
    } while (true);

    result->have_month = 1;

    if (day != 0) {
        if (day > 31) {
            return false;
        }
        result->have_day   = 1;
        result->dt.tm_mday = day;
    }
    if (year != 0) {
        result->have_year  = 1;
        result->dt.tm_year = year - 1900;
    }

    if (day > 31) {
        return false;
    }

    if (result->dt.tm_mon == 1 && day == 30) {
        return false;
    }

    if (day == 31) {
        switch (result->dt.tm_mon) {
        case 1:
        case 3:
        case 5:
        case 8:
        case 10:
            return false;
        default:
            break;
        }
    }

    return true;
}

static inline bool
other_written_date(n00b_string_t *input, n00b_date_time_t *result)
{
    int            l = n00b_string_byte_len(input);
    char          *s = input->data;
    char          *e = s + l;
    n00b_string_t *day;

    while (s < e) {
        char c = *s;

        if (c < '0' || c > '9') {
            break;
        }
        s++;
    }

    if (s == input->data) {
        return false;
    }

    day = n00b_string_slice(input, 0, s - input->data);

    while (s < e) {
        if (*s != ' ') {
            break;
        }
        s++;
    }

    char *month_part = s;

    while (s < e) {
        char c = *s;

        if (c < 'A' || c > 'z' || (c > 'Z' && c < 'a')) {
            break;
        }
        s++;
    }
    if (s == month_part) {
        return false;
    }

    int mstart = month_part - input->data;
    int mend   = s - input->data;

    n00b_string_t *mo           = n00b_string_slice(input, mstart, mend);
    n00b_string_t *year         = n00b_string_slice(input, mend, l);
    n00b_string_t *americanized = n00b_cformat("«#» «#»«#»", mo, day, year);

    return us_written_date(americanized, result);
}

#define WAS_DIGIT(x)      \
    if (x < 0 || x > 9) { \
        return false;     \
    }

#define ONE_DIGIT(n)  \
    if (s == e) {     \
        return false; \
    }                 \
    n = *s++ - '0';   \
    WAS_DIGIT(n)

#define PARSE_MONTH()           \
    ONE_DIGIT(m);               \
    ONE_DIGIT(tmp);             \
    m *= 10;                    \
    m += tmp;                   \
                                \
    if (m > 12) {               \
        return false;           \
    }                           \
                                \
    result->dt.tm_mon  = m - 1; \
    result->have_month = true

#define PARSE_DAY()         \
    ONE_DIGIT(d);           \
    if (d > 3) {            \
        return false;       \
    }                       \
    d *= 10;                \
    ONE_DIGIT(tmp);         \
    d += tmp;               \
    switch (m) {            \
    case 2:                 \
        if (d > 29) {       \
            return false;   \
        }                   \
        break;              \
    case 4:                 \
    case 6:                 \
    case 9:                 \
    case 11:                \
        if (d > 30) {       \
            return false;   \
        }                   \
        break;              \
    default:                \
        if (d > 31) {       \
            return false;   \
        }                   \
        break;              \
    }                       \
                            \
    result->dt.tm_mday = d; \
    result->have_day   = true

#define PARSE_YEAR4()              \
    ONE_DIGIT(y);                  \
    y *= 10;                       \
    ONE_DIGIT(tmp);                \
    y += tmp;                      \
    y *= 10;                       \
    ONE_DIGIT(tmp);                \
    y += tmp;                      \
    y *= 10;                       \
    ONE_DIGIT(tmp);                \
    y += tmp;                      \
                                   \
    result->dt.tm_year = y - 1900; \
    result->have_year  = true

#define REQUIRE_DASH() \
    if (*s++ != '-') { \
        return false;  \
    }

static inline bool
iso_date(n00b_string_t *input, n00b_date_time_t *result)
{
    int   l             = n00b_string_byte_len(input);
    char *s             = input->data;
    char *e             = s + l;
    int   m             = 0;
    int   d             = 0;
    int   y             = 0;
    bool  elided_dashes = false;
    int   tmp;

    switch (l) {
    case 4:
        REQUIRE_DASH();
        REQUIRE_DASH();
        PARSE_MONTH();
        return true;

    case 7:
        REQUIRE_DASH();
        REQUIRE_DASH();
        PARSE_MONTH();
        REQUIRE_DASH();
        PARSE_DAY();
        return true;
    case 8:
        elided_dashes = true;
        // fallthrough
    case 10:
        PARSE_YEAR4();

        if (!elided_dashes) {
            REQUIRE_DASH();
        }
        PARSE_MONTH();
        if (!elided_dashes) {
            REQUIRE_DASH();
        }
        PARSE_DAY();
        return true;

    default:
        return false;
    }
}

static bool
to_native_date(n00b_string_t *i, n00b_date_time_t *r)
{
    if (iso_date(i, r) || other_written_date(i, r) || us_written_date(i, r)) {
        return true;
    }

    return false;
}

#define END_OR_TZ()   \
    if (s == e) {     \
        return true;  \
    }                 \
    switch (*s) {     \
    case 'Z':         \
    case 'z':         \
    case '+':         \
    case '-':         \
        break;        \
    default:          \
        return false; \
    }

static bool
to_native_time(n00b_string_t *input, n00b_date_time_t *result)
{
    int   hr  = 0;
    int   min = 0;
    int   sec = 0;
    int   l   = n00b_string_byte_len(input);
    char *s   = input->data;
    char *e   = s + l;
    char  tmp;

    ONE_DIGIT(hr);
    if (*s != ':') {
        hr *= 10;
        ONE_DIGIT(tmp);
        hr += tmp;
    }
    if (*s++ != ':') {
        return false;
    }
    if (hr > 23) {
        return false;
    }
    result->dt.tm_hour = hr;

    ONE_DIGIT(min);
    min *= 10;
    ONE_DIGIT(tmp);
    min += tmp;

    if (min > 59) {
        return false;
    }

    result->dt.tm_min = min;
    result->have_time = true;

    if (s == e) {
        return true;
    }
    if (*s == ':') {
        s++;
        ONE_DIGIT(sec);
        sec *= 10;
        ONE_DIGIT(tmp);
        sec += tmp;
        if (sec > 60) {
            return false;
        }
        result->have_sec  = true;
        result->dt.tm_sec = sec;

        if (s == e) {
            return true;
        }
        if (*s == '.') {
            result->fracsec       = 0;
            result->have_frac_sec = true;

            ONE_DIGIT(result->fracsec);
            while (*s >= '0' && *s <= '9') {
                result->fracsec *= 10;
                ONE_DIGIT(tmp);
                result->fracsec += tmp;
            }
        }
    }

    while (s < e && *s == ' ') {
        s++;
    }

    if (s == e) {
        return true;
    }

    switch (*s) {
    case 'a':
        if (*++s != 'm') {
            return false;
        }
        ++s;
        END_OR_TZ();
        break;
    case 'A':
        ++s;
        if (*s != 'm' && *s != 'M') {
            return false;
        }
        ++s;
        END_OR_TZ();
        break;
    case 'p':
        if (*++s != 'm') {
            return false;
        }
        ++s;
        if (result->dt.tm_hour <= 11) {
            result->dt.tm_hour += 12;
        }
        END_OR_TZ();
        break;
    case 'P':
        ++s;
        if (*s != 'm' && *s != 'M') {
            return false;
        }
        ++s;
        if (result->dt.tm_hour <= 11) {
            result->dt.tm_hour += 12;
        }
        END_OR_TZ();
        break;
    case 'Z':
    case 'z':
    case '+':
    case '-':
        break;
    default:
        return false;
    }

    result->have_offset = true;

    if (*s == 'Z' || *s == 'z') {
        s++;
    }

    if (s == e) {
        return true;
    }

    int mul    = 1;
    int offset = 0;

    if (*s == '-') {
        mul = -1;
        s++;
    }
    else {
        if (*s == '+') {
            s++;
        }
    }

    if (s == e) {
        return false;
    }

    ONE_DIGIT(offset);
    offset *= mul;

    if (s != e) {
        ONE_DIGIT(tmp);
        offset *= 10;
        offset += tmp;
    }

    // This range covers it;
    // the true *historic range is -15:56:00 - 15:13:42
    // and in practice should generally be -12 to +14 now.

    if (offset < -15 || offset > 15) {
        return false;
    }

    result->dt.tm_gmtoff = offset * 60 * 60;

    if (s == e) {
        return true;
    }

    if (*s == ':') {
        s++;
    }

    offset = 0;
    ONE_DIGIT(offset);
    ONE_DIGIT(tmp);
    offset *= 10;
    offset += tmp;
    if (offset > 59) {
        return false;
    }

    result->dt.tm_gmtoff += offset * 60;

    if (s == e) {
        return true;
    }

    if (*s++ != ':') {
        return false;
    }

    offset = 0;
    ONE_DIGIT(offset);
    ONE_DIGIT(tmp);
    offset *= 10;
    offset += tmp;
    if (offset > 60) {
        return false;
    }

    result->dt.tm_gmtoff += offset;

    return s == e;
}

static bool
to_native_date_and_or_time(n00b_string_t *input, n00b_date_time_t *result)
{
    int  ix           = n00b_string_find(input, n00b_cstring("T"));
    bool im_exhausted = false;

    if (ix == -1) {
        ix = n00b_string_find(input, n00b_cstring("t"));
    }

try_a_slice:
    if (ix != -1) {
        int            l    = n00b_string_codepoint_len(input);
        n00b_string_t *date = n00b_string_slice(input, 0, ix);
        n00b_string_t *time = n00b_string_slice(input, ix + 1, l);

        if (iso_date(date, result) && to_native_time(time, result)) {
            return true;
        }

        if (to_native_date(date, result) && to_native_time(time, result)) {
            return true;
        }

        if (im_exhausted) {
            // We've been up here twice, why loop forever when it isn't
            // going to work out?
            return false;
        }
    }

    // Otherwise, first look for the first colon after a space.
    ix = n00b_string_find(input, n00b_cached_colon());

    n00b_string_require_u32(input);

    if (ix != -1) {
        int               last_space = -1;
        n00b_codepoint_t *cptr       = (n00b_codepoint_t *)input->u32_data;

        for (int i = 0; i < ix; i++) {
            if (*cptr++ == ' ') {
                last_space = i;
            }
        }

        if (last_space != -1) {
            ix           = last_space;
            im_exhausted = true;
            goto try_a_slice;
        }
    }

    if (to_native_date(input, result)) {
        return true;
    }

    memset(result, 0, sizeof(n00b_date_time_t));

    if (to_native_time(input, result)) {
        return true;
    }

    return false;
}

#define YEAR_NOT_SET 0x7fffffff

static void
datetime_init(n00b_date_time_t *self, va_list args)
{
    n00b_string_t *to_parse   = NULL;
    int32_t        hr         = -1;
    int32_t        min        = -1;
    int32_t        sec        = -1;
    int32_t        month      = -1;
    int32_t        day        = -1;
    int32_t        year       = YEAR_NOT_SET;
    int32_t        offset_hr  = -100;
    int32_t        offset_min = 0;
    int32_t        offset_sec = 0;
    int64_t        fracsec    = -1;
    int64_t        timestamp  = -1;

    n00b_karg_va_init(args);
    n00b_kw_ptr("to_parse", args);
    n00b_kw_int32("hr", hr);
    n00b_kw_int32("min", min);
    n00b_kw_int32("sec", sec);
    n00b_kw_int32("month", month);
    n00b_kw_int32("day", day);
    n00b_kw_int32("year", year);
    n00b_kw_int32("offset_hr", offset_hr);
    n00b_kw_int32("offset_min", offset_min);
    n00b_kw_int32("offset_sec", offset_sec);
    n00b_kw_int64("fracsec", fracsec);
    n00b_kw_int64("timestamp", timestamp);

    if (timestamp != -1) {
        localtime_r((time_t *)&timestamp, &self->dt);
        n00b_type_t *t = n00b_get_my_type(self);

        if (n00b_type_is_datetime(t)) {
            self->have_time   = true;
            self->have_sec    = true;
            self->have_month  = true;
            self->have_year   = true;
            self->have_day    = true;
            self->have_offset = true;
            return;
        }
        if (n00b_type_is_date(t)) {
            self->have_month = true;
            self->have_year  = true;
            self->have_day   = true;
            return;
        }
        self->have_time   = true;
        self->have_sec    = true;
        self->have_offset = true;
        return;
    }

    if (to_parse != NULL) {
        if (!to_native_date_and_or_time(to_parse, self)) {
            N00B_CRAISE("Invalid date-time literal.");
        }
        return;
    }

    self->dt.tm_isdst = -1;

    if (hr != -1) {
        if (hr < 0 || hr > 23) {
            N00B_CRAISE("Invalid hour (must be 0 - 23)");
        }
        self->dt.tm_hour = hr;
        self->have_time  = true;

        if (min != -1) {
            if (min < 0 || min > 59) {
                N00B_CRAISE("Invalid minute (must be 0 - 59)");
            }
            self->dt.tm_min = min;
        }

        if (sec != -1) {
            if (sec < 0 || sec > 61) {
                N00B_CRAISE("Invalid second (must be 0 - 60)");
            }
            self->dt.tm_sec = sec;
        }

        if (fracsec > 0) {
            self->fracsec = fracsec;
        }
    }

    if (year != YEAR_NOT_SET) {
        self->have_year  = true;
        self->dt.tm_year = year;
    }

    if (month != -1) {
        if (month < 1 || month > 12) {
            N00B_CRAISE("Invalid month (must be 1 - 12)");
        }
        self->dt.tm_mon  = month - 1;
        self->have_month = true;
    }

    if (day != -1) {
        if (day < 1 || day > 31) {
            N00B_CRAISE("Invalid day of month");
        }
        self->dt.tm_mday = day;
        self->have_day   = true;
    }

    int offset = 0;

    if (offset_hr >= -15 && offset_hr <= 15) {
        offset            = offset_hr * 60 * 60;
        self->have_offset = true;
    }

    if (offset_min > 0 && offset_min < 60) {
        offset += offset_min * 60;
        self->have_offset = true;
    }

    if (offset_sec > 0 && offset_sec <= 60) {
        offset += offset_min;
    }

    self->dt.tm_gmtoff = offset;
}

#define DT_HAVE_TIME 1
#define DT_HAVE_SEC  2
#define DT_HAVE_FRAC 4
#define DT_HAVE_MO   8
#define DT_HAVE_Y    16
#define DT_HAVE_D    32
#define DT_HAVE_OFF  64

static n00b_string_t *
datetime_repr(n00b_date_time_t *self)
{
    // TODO: this could use a lot more logic to make it more sane
    // when bits aren't fully filled out.
    //
    // Also, for now we are just omitting the fractional second.

    char *fmt = NULL;

    if (self->have_time) {
        if (self->have_day || self->have_month || self->have_year) {
            if (self->have_offset) {
                fmt = "%Y-%m-%dT%H:%M:%S%z";
            }
            else {
                fmt = "%Y-%m-%dT%H:%M:%S";
            }
        }
        else {
            fmt = "%H:%M:%S";
        }
    }
    else {
        fmt = "%Y-%m-%d";
    }

    char buf[1024];

    if (!strftime(buf, 1024, fmt, &self->dt)) {
        return n00b_cstring("<<invalid time>>");
    }

    return n00b_cstring(buf);
}

static n00b_date_time_t *
datetime_lit(n00b_string_t        *s,
             n00b_lit_syntax_t     st,
             n00b_string_t        *mod,
             n00b_compile_error_t *err)
{
    n00b_date_time_t *result = n00b_new(n00b_type_datetime());

    if (!to_native_date_and_or_time(s, result)) {
        *err = n00b_err_invalid_dt_spec;
        return NULL;
    }

    return result;
}

static bool
datetime_can_coerce_to(n00b_type_t *my_type, n00b_type_t *target_type)
{
    switch (target_type->base_index) {
    case N00B_T_DATETIME:
    case N00B_T_DATE:
    case N00B_T_TIME:
        return true;
    default:
        return false;
    }
}

static n00b_date_time_t *
datetime_coerce_to(n00b_date_time_t *dt)
{
    return dt;
}

static n00b_date_time_t *
datetime_copy(n00b_date_time_t *dt)
{
    n00b_date_time_t *result = n00b_new(n00b_get_my_type(dt));

    memcpy(result, dt, sizeof(n00b_date_time_t));

    return result;
}

static inline void
internal_error()
{
    N00B_CRAISE("Internal error (when calling strftime)");
}

#define N00B_MAX_DATETIME_OUTPUT 1024

static n00b_string_t *
datetime_format_base(n00b_date_time_t *dt,
                     n00b_string_t    *fmt,
                     char             *default_fmt)
{
    char buf[N00B_MAX_DATETIME_OUTPUT];

    if (!fmt || !fmt->codepoints) {
        if (!strftime(buf, 1024, default_fmt, &dt->dt)) {
            internal_error();
        }

        return n00b_cstring(buf);
    }

    if (!strftime(buf, N00B_MAX_DATETIME_OUTPUT, fmt->data, &dt->dt)) {
        internal_error();
    }

    return n00b_cstring(buf);
}

#define N00B_MAX_DATETIME_OUTPUT 1024

static n00b_string_t *
datetime_format(n00b_date_time_t *dt, n00b_string_t *fmt)
{
    return datetime_format_base(dt, fmt, "%F %r %z");
}

static n00b_string_t *
date_format(n00b_date_time_t *dt, n00b_string_t *fmt)
{
    return datetime_format_base(dt, fmt, "%F");
}

static n00b_string_t *
time_format(n00b_date_time_t *dt, n00b_string_t *fmt)
{
    return datetime_format_base(dt, fmt, "%r %z");
}

static n00b_date_time_t *
date_lit(n00b_string_t        *s,
         n00b_lit_syntax_t     st,
         n00b_string_t        *mod,
         n00b_compile_error_t *err)
{
    n00b_date_time_t *result = n00b_new(n00b_type_date());

    if (!to_native_date(s, result)) {
        *err = n00b_err_invalid_date_spec;
        return NULL;
    }

    return result;
}

static n00b_date_time_t *
time_lit(n00b_string_t        *s,
         n00b_lit_syntax_t     st,
         n00b_string_t        *mod,
         n00b_compile_error_t *err)
{
    n00b_date_time_t *result = n00b_new(n00b_type_time());

    if (!to_native_time(s, result)) {
        *err = n00b_err_invalid_time_spec;
        return NULL;
    }

    return result;
}

const n00b_vtable_t n00b_datetime_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR]  = (n00b_vtable_entry)datetime_init,
        [N00B_BI_TO_STRING]    = (n00b_vtable_entry)datetime_repr,
        [N00B_BI_FORMAT]       = (n00b_vtable_entry)datetime_format,
        [N00B_BI_COERCIBLE]    = (n00b_vtable_entry)datetime_can_coerce_to,
        [N00B_BI_COERCE]       = (n00b_vtable_entry)datetime_coerce_to,
        [N00B_BI_FROM_LITERAL] = (n00b_vtable_entry)datetime_lit,
        [N00B_BI_COPY]         = (n00b_vtable_entry)datetime_copy,
        [N00B_BI_GC_MAP]       = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
        [N00B_BI_FINALIZER]    = NULL,
    },
};

const n00b_vtable_t n00b_date_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR]  = (n00b_vtable_entry)datetime_init,
        [N00B_BI_TO_STRING]    = (n00b_vtable_entry)datetime_repr,
        [N00B_BI_FORMAT]       = (n00b_vtable_entry)date_format,
        [N00B_BI_COERCIBLE]    = (n00b_vtable_entry)datetime_can_coerce_to,
        [N00B_BI_COERCE]       = (n00b_vtable_entry)datetime_coerce_to,
        [N00B_BI_FROM_LITERAL] = (n00b_vtable_entry)date_lit,
        [N00B_BI_COPY]         = (n00b_vtable_entry)datetime_copy,
        [N00B_BI_GC_MAP]       = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
        [N00B_BI_FINALIZER]    = NULL,
    },
};

const n00b_vtable_t n00b_time_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR]  = (n00b_vtable_entry)datetime_init,
        [N00B_BI_TO_STRING]    = (n00b_vtable_entry)datetime_repr,
        [N00B_BI_FORMAT]       = (n00b_vtable_entry)time_format,
        [N00B_BI_COERCIBLE]    = (n00b_vtable_entry)datetime_can_coerce_to,
        [N00B_BI_COERCE]       = (n00b_vtable_entry)datetime_coerce_to,
        [N00B_BI_FROM_LITERAL] = (n00b_vtable_entry)time_lit,
        [N00B_BI_COPY]         = (n00b_vtable_entry)datetime_copy,
        [N00B_BI_GC_MAP]       = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
        [N00B_BI_FINALIZER]    = NULL,

    },
};
