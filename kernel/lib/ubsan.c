/*
 * Handlers for the undefined-behaviour sanitiser (-fsanitize=undefined,
 * test builds only). Clang instruments risky operations to call these
 * when they go wrong; each reports where, then panics.
 *
 * Every handler's first argument points to a structure that starts with
 * the source location. The rest of the arguments are ignored, so one
 * signature fits all of them (the x86-64 calling convention passes extra
 * or missing integer arguments harmlessly).
 *
 * Deviations: the __ubsan_ names are reserved identifiers defined by the
 * compiler's ABI (D5).
 */
#include <stdint.h>

#include "kernel.h"

struct source_location {
    const char *file;
    uint32_t line;
    uint32_t column;
};

struct type_mismatch_data {
    struct source_location loc;
    const void *type;
    uint8_t log_alignment;
    uint8_t type_check_kind;
};

[[noreturn]] static void report(const char *what, const struct source_location *loc)
{
    panic("UBSan: %s at %s:%u:%u", what, loc->file, loc->line, loc->column);
}

#define UBSAN_HANDLER(name, what)                                                  \
    void __ubsan_handle_##name(const void *data, uintptr_t a, uintptr_t b); /* NOLINT */ \
    void __ubsan_handle_##name(const void *data, uintptr_t a, uintptr_t b) /* NOLINT */  \
    {                                                                              \
        (void)a;                                                                   \
        (void)b;                                                                   \
        report(what, data);                                                        \
    }

UBSAN_HANDLER(add_overflow, "signed addition overflow")
UBSAN_HANDLER(sub_overflow, "signed subtraction overflow")
UBSAN_HANDLER(mul_overflow, "signed multiplication overflow")
UBSAN_HANDLER(negate_overflow, "signed negation overflow")
UBSAN_HANDLER(divrem_overflow, "division by zero or overflow")
UBSAN_HANDLER(shift_out_of_bounds, "shift out of bounds")
UBSAN_HANDLER(out_of_bounds, "array index out of bounds")
UBSAN_HANDLER(pointer_overflow, "pointer arithmetic overflow")
UBSAN_HANDLER(load_invalid_value, "invalid bool or enum value")
UBSAN_HANDLER(builtin_unreachable, "unreachable code reached")
UBSAN_HANDLER(vla_bound_not_positive, "variable-length array bound not positive")
UBSAN_HANDLER(nonnull_arg, "null passed as a nonnull argument")
UBSAN_HANDLER(nonnull_return_v1, "null returned from a nonnull function")
UBSAN_HANDLER(float_cast_overflow, "float conversion overflow")
UBSAN_HANDLER(invalid_builtin, "invalid builtin argument")
UBSAN_HANDLER(alignment_assumption, "alignment assumption violated")

/* Null, misaligned or too-small object access: tell them apart. */
void __ubsan_handle_type_mismatch_v1(const struct type_mismatch_data *data, uintptr_t ptr); /* NOLINT */
void __ubsan_handle_type_mismatch_v1(const struct type_mismatch_data *data, uintptr_t ptr) /* NOLINT */
{
    uintptr_t align = (uintptr_t)1 << data->log_alignment;

    if (ptr == 0U) {
        report("null pointer access", &data->loc);
    } else if ((ptr & (align - 1U)) != 0U) {
        report("misaligned access", &data->loc);
    } else {
        report("access to an object too small for its type", &data->loc);
    }
}
