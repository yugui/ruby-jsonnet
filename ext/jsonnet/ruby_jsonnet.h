#ifndef RUBY_JSONNET_RUBY_JSONNET_H_
#define RUBY_JSONNET_RUBY_JSONNET_H_

#include <ruby/ruby.h>
#include <ruby/encoding.h>

extern const rb_data_type_t jsonnet_vm_type;

struct native_callback_ctx {
    VALUE callback;
    long arity;
    VALUE vm;
};

struct jsonnet_vm_wrap {
    struct JsonnetVm *vm;

    VALUE import_callback;
    struct {
	long len;
	struct native_callback_ctx **contexts;
    } native_callbacks;
};

void rubyjsonnet_init_vm(VALUE mod);
void rubyjsonnet_init_callbacks(VALUE cVM);
void rubyjsonnet_init_helpers(VALUE mod);

struct jsonnet_vm_wrap *rubyjsonnet_obj_to_vm(VALUE vm);

VALUE rubyjsonnet_json_to_obj(struct JsonnetVm *vm, const struct JsonnetJsonValue *value);
struct JsonnetJsonValue *rubyjsonnet_obj_to_json(struct JsonnetVm *vm, VALUE obj, int *success);

rb_encoding *rubyjsonnet_assert_asciicompat(VALUE str);
char *rubyjsonnet_str_to_cstr(struct JsonnetVm *vm, VALUE str);
VALUE rubyjsonnet_format_exception(VALUE exc);
int rubyjsonnet_jump_tag(const char *exc_mesg);

#endif /* RUBY_JSONNET_RUBY_JSONNET_H_ */
