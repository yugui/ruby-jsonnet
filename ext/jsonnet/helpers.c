#include <string.h>

#include <ruby/ruby.h>
#include <ruby/encoding.h>
#include <libjsonnet.h>

#include "ruby_jsonnet.h"

/**
 * Indicates that the encoding of the given string is not allowed by
 * the C++ implementation of Jsonnet.
 */
static VALUE eUnsupportedEncodingError;

rb_encoding *
rubyjsonnet_assert_asciicompat(VALUE str)
{
    rb_encoding *enc = rb_enc_get(str);
    if (!rb_enc_asciicompat(enc)) {
        rb_raise(
            eUnsupportedEncodingError,
            "jsonnet encoding must be ASCII-compatible but got %s",
            rb_enc_name(enc));
    }
    return enc;
}

/**
 * Allocates a C string whose content is equal to \c str with jsonnet_realloc.
 */
char *
rubyjsonnet_str_to_cstr(struct JsonnetVm *vm, VALUE str)
{
    const char *const cstr = StringValueCStr(str);
    char *const buf = jsonnet_realloc(vm, NULL, strlen(cstr));
    strcpy(buf, cstr);
    return buf;
}

void
rubyjsonnet_init_helpers(VALUE mod)
{
    eUnsupportedEncodingError =
        rb_define_class_under(mod, "UnsupportedEncodingError", rb_eEncodingError);
}
