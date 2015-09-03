#include <ruby/ruby.h>
#include <libjsonnet.h>

static VALUE cVM;

static VALUE
jsonnet_s_version(VALUE mod) {
    return rb_usascii_str_new_cstr(jsonnet_version());
}

void
Init_jsonnet_wrap(void) {
    VALUE mJsonnet = rb_define_module("Jsonnet");
    rb_define_singleton_method(mJsonnet, "libversion", jsonnet_s_version, 0);
    cVM = rb_define_class_under(mJsonnet, "VM", rb_cData);
}
