#include <libjsonnet.h>
#include <ruby/ruby.h>
#include <ruby/encoding.h>

#include "ruby_jsonnet.h"

/*
 * call-seq:
 *  Jsonnet.version -> String
 *
 * Returns the version of the underlying C++ implementation of Jsonnet.
 */
static VALUE
jw_s_version(VALUE mod)
{
    return rb_usascii_str_new_cstr(jsonnet_version());
}

void
Init_jsonnet_wrap(void)
{
    VALUE mJsonnet = rb_define_module("Jsonnet");
    rb_define_singleton_method(mJsonnet, "libversion", jw_s_version, 0);

    rubyjsonnet_init_helpers(mJsonnet);
    rubyjsonnet_init_vm(mJsonnet);
}
