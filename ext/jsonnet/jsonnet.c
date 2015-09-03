#include <ruby/ruby.h>
#include <libjsonnet.h>

/*
 * Jsonnet evaluator
 */
static VALUE cVM;

static void vm_free(void *ptr);
static const rb_data_type_t jsonnet_vm_type = {
    "JsonnetVm",
    {
      /* dmark = */ 0,
      /* dfree = */ vm_free,
      /* dsize = */ 0,
    },
    /* parent = */ 0,
    /* data = */ 0,
    /* flags = */ RUBY_TYPED_FREE_IMMEDIATELY
};

/*
 * call-seq:
 *  Jsonnet.version -> String
 *
 * Returns the version of the underlying C++ implementation of Jsonnet.
 */
static VALUE
jw_s_version(VALUE mod) {
    return rb_usascii_str_new_cstr(jsonnet_version());
}

static VALUE
vm_s_new(VALUE mod) {
    struct JsonnetVm *const vm = jsonnet_make();
    return TypedData_Wrap_Struct(cVM, &jsonnet_vm_type, vm);
}

static void
vm_free(void *ptr) {
    jsonnet_destroy((struct JsonnetVm*)ptr);
}

void
Init_jsonnet_wrap(void) {
    VALUE mJsonnet = rb_define_module("Jsonnet");
    rb_define_singleton_method(mJsonnet, "libversion", jw_s_version, 0);

    cVM = rb_define_class_under(mJsonnet, "VM", rb_cData);
    rb_define_singleton_method(cVM, "new", vm_s_new, 0);
}
