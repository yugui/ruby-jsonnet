#ifndef RUBY_JSONNET_VM_H_
#define RUBY_JSONNET_VM_H_

#include <ruby/ruby.h>
#include <ruby/encoding.h>

struct native_callback_ctx;

struct jsonnet_vm_wrap {
    struct JsonnetVm *vm;
    VALUE import_callback;

    struct {
        long len;
        struct native_callback_ctx **contexts;
    } native_callbacks;
};

rb_encoding *rubyjsonnet_enc_assert_asciicompat(VALUE str);
struct jsonnet_vm_wrap *rubyjsonnet_obj_to_vm(VALUE vm);
VALUE jv_json_to_obj(struct JsonnetVm *vm, const struct JsonnetJsonValue* value);
struct JsonnetJsonValue * jv_obj_to_json(struct JsonnetVm *vm, VALUE obj, int *success);
VALUE rubyjsonnet_format_exception(VALUE exc);

#endif  /* RUBY_JSONNET_VM_H_ */
