#include <string.h>

#include <libjsonnet.h>
#include <ruby/ruby.h>

#include "ruby_jsonnet.h"

/* Magic prefix which distinguishes Ruby-level non-exception global escapes
 * from other errors in Jsonnet.
 * Any other errors in Jsonnet evaluation cannot contain this fragment of message.
 */
#define RUBYJSONNET_GLOBAL_ESCAPE_MAGIC "\x07\x03\x0c:rubytag:\x07\x03\x0c:"

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
    return rb_funcall2(callback, id_call, len - 1, RARRAY_PTR(args) + 1);
}

/*
 * Handles long jump caught by rb_protect() in a callback function.
 *
 * Returns an error message which represents rb_errinfo().
 * It is the caller's responsibility to return the message to the Jsonnet VM
 * in the right way.
 * Also the caller of the VM must handle evaluation failure caused by the
 * error message.
 * \sa rubyjsonnet_jump_tag
 * \sa raise_eval_error
 */
static VALUE
rescue_callback(int state, const char *fmt, ...)
{
    VALUE err = rb_errinfo();
    if (rb_obj_is_kind_of(err, rb_eException)) {
	VALUE msg = rb_protect(rubyjsonnet_format_exception, rb_errinfo(), NULL);
	if (msg == Qnil) {
	    va_list ap;
	    va_start(ap, fmt);
	    msg = rb_vsprintf(fmt, ap);
	    va_end(ap);
	}
	rb_set_errinfo(Qnil);
	return msg;
    }

    /*
     * Other types of global escape.
     * Here, we'll report the state as an evaluation error to let
     * the Jsonnet VM clean up its internal resources.
     * But we'll translate the error into an non-exception global escape
     * in Ruby again in raise_eval_error().
     */
    return rb_sprintf("%s%d%s", RUBYJSONNET_GLOBAL_ESCAPE_MAGIC, state,
		      RUBYJSONNET_GLOBAL_ESCAPE_MAGIC);
}

/*
 * Tries to extract a jump tag from a string representation encoded by
 * rescue_callback.
 * @retval zero if \a exc_mesg is not such a string representation.
 * @retval non-zero the extracted tag
 */
int
rubyjsonnet_jump_tag(const char *exc_mesg)
{
    const char *tag;
#define JSONNET_RUNTIME_ERROR_PREFIX "RUNTIME ERROR: "
    if (strncmp(exc_mesg, JSONNET_RUNTIME_ERROR_PREFIX, strlen(JSONNET_RUNTIME_ERROR_PREFIX))) {
	return 0;
    }
    tag = strstr(exc_mesg + strlen(JSONNET_RUNTIME_ERROR_PREFIX), RUBYJSONNET_GLOBAL_ESCAPE_MAGIC);
    if (tag) {
	const char *const body = tag + strlen(RUBYJSONNET_GLOBAL_ESCAPE_MAGIC);
	char *last;
	long state = strtol(body, &last, 10);
	if (!strncmp(last, RUBYJSONNET_GLOBAL_ESCAPE_MAGIC,
		     strlen(RUBYJSONNET_GLOBAL_ESCAPE_MAGIC)) &&
	    INT_MIN <= state && state <= INT_MAX) {
	    return (int)state;
	}
    }
    return 0;
}

static char *
import_callback_entrypoint(void *ctx, const char *base, const char *rel, char **found_here,
			   int *success)
{
    struct jsonnet_vm_wrap *const vm = (struct jsonnet_vm_wrap *)ctx;
    int state;
    VALUE result, args;

    args = rb_ary_tmp_new(3);

    rb_ary_push(args, vm->import_callback);
    rb_ary_push(args, rb_enc_str_new_cstr(base, rb_filesystem_encoding()));
    rb_ary_push(args, rb_enc_str_new_cstr(rel, rb_filesystem_encoding()));
    result = rb_protect(invoke_callback, args, &state);

    rb_ary_free(args);

    if (state) {
	VALUE msg = rescue_callback(state, "cannot import %s from %s", rel, base);
	*success = 0;
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
 * Generic entrypoint of native callbacks which adapts callable objects in Ruby to \c
 * JsonnetNativeCallback.
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

    struct native_callback_ctx *const ctx = (struct native_callback_ctx *)data;
    struct JsonnetVm *const vm = rubyjsonnet_obj_to_vm(ctx->vm)->vm;
    VALUE result, args = rb_ary_tmp_new(ctx->arity + 1);

    rb_ary_push(args, ctx->callback);
    for (i = 0; i < ctx->arity; ++i) {
	rb_ary_push(args, rubyjsonnet_json_to_obj(vm, argv[i]));
    }

    result = rb_protect(invoke_callback, args, &state);

    rb_ary_free(args);

    if (state) {
	VALUE msg = rescue_callback(state, "something wrong in %" PRIsVALUE, ctx->callback);
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
	const char **buf;
    } cstr_params;
    struct native_callback_ctx *ctx;
    struct jsonnet_vm_wrap *vm = rubyjsonnet_obj_to_vm(self);
    long i;

    name = rb_to_symbol(name);
    rubyjsonnet_assert_asciicompat(name);

    params = rb_Array(params);
    cstr_params.len = RARRAY_LEN(params);
    cstr_params.buf = (const char **)rb_alloc_tmp_buffer(
	&cstr_params.store, sizeof(const char *const) * (cstr_params.len + 1));
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
    jsonnet_native_callback(vm->vm, rb_id2name(RB_SYM2ID(name)), native_callback_entrypoint, ctx,
			    cstr_params.buf);

    rb_free_tmp_buffer(&cstr_params.store);

    RB_REALLOC_N(vm->native_callbacks.contexts, struct native_callback_ctx *,
		 vm->native_callbacks.len + 1);
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
