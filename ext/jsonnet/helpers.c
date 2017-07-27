#include <string.h>

#include <libjsonnet.h>
#include <ruby/ruby.h>
#include <ruby/encoding.h>

#include "ruby_jsonnet.h"

static ID id_message;

/*
 * Indicates that the encoding of the given string is not allowed by
 * the C++ implementation of Jsonnet.
 */
static VALUE eUnsupportedEncodingError;

/**
 * Asserts that the given string-like object is in an
 * ASCII-compatible encoding.
 *
 * @param[in] str a String-like object
 * @throw UnsupportedEncodingError on assertion failure.
 */
rb_encoding *
rubyjsonnet_assert_asciicompat(VALUE str)
{
    rb_encoding *enc = rb_enc_get(str);
    if (!rb_enc_asciicompat(enc)) {
	rb_raise(eUnsupportedEncodingError, "jsonnet encoding must be ASCII-compatible but got %s",
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

/**
 * @return a human readable string which contains the class name of the
 *   exception and its message. It might be nil on failure
 */
VALUE
rubyjsonnet_format_exception(VALUE exc)
{
    VALUE name = rb_class_name(rb_obj_class(exc));
    VALUE msg = rb_funcall(exc, id_message, 0);
    if (RB_TYPE_P(name, RUBY_T_STRING) && rb_str_strlen(name)) {
	if (RB_TYPE_P(msg, RUBY_T_STRING) && rb_str_strlen(msg)) {
	    return rb_str_concat(rb_str_cat_cstr(name, " : "), msg);
	} else {
	    return name;
	}
    } else if (RB_TYPE_P(msg, RUBY_T_STRING) && rb_str_strlen(msg)) {
	return msg;
    }
    return Qnil;
}

void
rubyjsonnet_init_helpers(VALUE mJsonnet)
{
    id_message = rb_intern("message");
    eUnsupportedEncodingError =
	rb_define_class_under(mJsonnet, "UnsupportedEncodingError", rb_eEncodingError);
}
