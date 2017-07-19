#include <ruby/ruby.h>
#include <libjsonnet.h>

#include "ruby_jsonnet.h"

/*
 * callback support in VM
 */

static ID id_call;

/*
 * Just invokes a callback with arguments, but also adapts the invocation to rb_protect.
 * @param[in] args  list of arguments whose first value is the callable object itself.
 * @return result of the callback
 */
static VALUE
invoke_callback(VALUE args)
{
    long len = RARRAY_LEN(args);
    VALUE callback = rb_ary_entry(args, 0);
    return rb_funcall2(callback, id_call, len-1, RARRAY_PTR(args)+1);
}

static char *
import_callback_entrypoint(void *ctx, const char *base, const char *rel, char **found_here, int *success)
{
    struct jsonnet_vm_wrap *const vm = (struct jsonnet_vm_wrap*)ctx;
    int state;
    VALUE result, args;

    args = rb_ary_tmp_new(3);

    rb_ary_push(args, vm->import_callback);
    rb_ary_push(args, rb_enc_str_new_cstr(base, rb_filesystem_encoding()));
    rb_ary_push(args, rb_enc_str_new_cstr(rel, rb_filesystem_encoding()));
    result = rb_protect(invoke_callback, args, &state);

    rb_ary_free(args);

    if (state) {
        VALUE msg = rb_protect(rubyjsonnet_format_exception, rb_errinfo(), 0);
        if (msg == Qnil) {
            msg = rb_sprintf("cannot import %s from %s", rel, base);
        }
        *success = 0;
        rb_set_errinfo(Qnil);
        return rubyjsonnet_str_to_cstr(vm->vm, msg);
    }

    result = rb_Array(result);
    *success = 1;
    *found_here = rubyjsonnet_str_to_cstr(vm->vm, rb_ary_entry(result, 1));
    return rubyjsonnet_str_to_cstr(vm->vm, rb_ary_entry(result, 0));
}

/*
 * Sets a custom way to resolve "import" expression.
 * @param [#call] callback receives two parameters and returns two values.
 *                The first parameter "base" is a base directory to resolve
 *                "rel" from.
 *                The second parameter "rel" is an absolute or a relative
 *                path to the file to import.
 *                The first return value is the content of the imported file.
 *                The second return value is the resolved path of the imported file.
 */
static VALUE
vm_set_import_callback(VALUE self, VALUE callback)
{
    struct jsonnet_vm_wrap *const vm = rubyjsonnet_obj_to_vm(self);

    vm->import_callback = callback;
    jsonnet_import_callback(vm->vm, import_callback_entrypoint, vm);

    return callback;
}

/**
 * Generic entrypoint of native callbacks which adapts callable objects in Ruby to \c JsonnetNativeCallback.
 *
 * @param[in] data pointer to a {\c struct native_callback_ctx}
 * @param[in] argv NULL-terminated array of arguments
 * @param[out] success set to 1 on success, or 0 if otherwise.
 * @returns the result of the callback on success, an error message on failure.
 */
static struct JsonnetJsonValue *
native_callback_entrypoint(void *data, const struct JsonnetJsonValue *const *argv, int *success)
{
    long i;
    int state = 0;

    struct native_callback_ctx *ctx = (struct native_callback_ctx *)data;
    struct JsonnetVm *const vm = rubyjsonnet_obj_to_vm(ctx->vm)->vm;
    VALUE result, args = rb_ary_tmp_new(ctx->arity + 1);

    rb_ary_push(args, ctx->callback);
    for (i = 0; i < ctx->arity; ++i) {
        rb_ary_push(args, rubyjsonnet_json_to_obj(vm, argv[i]));
    }

    result = rb_protect(invoke_callback, args, &state);

    rb_ary_free(args);

    if (state) {
        VALUE msg = rb_protect(rubyjsonnet_format_exception, rb_errinfo(), 0);
        if (msg == Qnil) {
            msg = rb_sprintf("something wrong in %+"PRIsVALUE, ctx->callback);
        }
        rb_set_errinfo(Qnil);

        *success = 0;
        return rubyjsonnet_obj_to_json(vm, msg, &state);
    }

    return rubyjsonnet_obj_to_json(vm, result, success);
}

/*
 * Registers a native extension written in Ruby.
 * @param callback [#call] a PURE callable object
 * @param params [Array]   names of the parameters of the function
 */
static VALUE
vm_register_native_callback(VALUE self, VALUE name, VALUE callback, VALUE params)
{
    struct {
        volatile VALUE store;
        long len;
        const char * *buf;
    } cstr_params;
    struct native_callback_ctx *ctx;
    struct jsonnet_vm_wrap *vm = rubyjsonnet_obj_to_vm(self);
    long i;

    name = rb_to_symbol(name);
    rubyjsonnet_assert_asciicompat(name);

    params = rb_Array(params);
    cstr_params.len = RARRAY_LEN(params);
    cstr_params.buf = (const char * *)rb_alloc_tmp_buffer(
            &cstr_params.store, sizeof(const char *const) * (cstr_params.len+1));
    for (i = 0; i < cstr_params.len; ++i) {
        const VALUE pname = rb_to_symbol(RARRAY_AREF(params, i));
        rubyjsonnet_assert_asciicompat(pname);
        cstr_params.buf[i] = rb_id2name(RB_SYM2ID(pname));
    }
    cstr_params.buf[cstr_params.len] = NULL;

    ctx = RB_ALLOC_N(struct native_callback_ctx, 1);
    ctx->callback = callback;
    ctx->arity = cstr_params.len;
    ctx->vm = self;
    jsonnet_native_callback(vm->vm, rb_id2name(RB_SYM2ID(name)),
            native_callback_entrypoint, ctx, cstr_params.buf);

    rb_free_tmp_buffer(&cstr_params.store);

    RB_REALLOC_N(vm->native_callbacks.contexts, struct native_callback_ctx *,
            vm->native_callbacks.len+1);
    vm->native_callbacks.contexts[vm->native_callbacks.len] = ctx;
    vm->native_callbacks.len++;

    return name;
}

void
rubyjsonnet_init_callbacks(VALUE cVM)
{
    id_call = rb_intern("call");

    rb_define_method(cVM, "import_callback=", vm_set_import_callback, 1);
    rb_define_private_method(cVM, "register_native_callback", vm_register_native_callback, 3);
}
