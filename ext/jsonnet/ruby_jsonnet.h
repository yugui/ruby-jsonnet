#ifndef RUBY_JSONNET_RUBY_JSONNET_H_
#define RUBY_JSONNET_RUBY_JSONNET_H_

#include <ruby/ruby.h>
#include <ruby/encoding.h>

extern const rb_data_type_t jsonnet_vm_type;

struct jsonnet_vm_wrap {
    struct JsonnetVm *vm;
    VALUE callback;
};

void rubyjsonnet_init_vm(VALUE mod);
void rubyjsonnet_init_callbacks(VALUE cVM);
void rubyjsonnet_init_helpers(VALUE mod);

rb_encoding *rubyjsonnet_assert_asciicompat(VALUE str);
char *rubyjsonnet_str_to_cstr(struct JsonnetVm *vm, VALUE str);

#endif  /* RUBY_JSONNET_RUBY_JSONNET_H_ */
