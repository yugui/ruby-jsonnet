#include <libjsonnet.h>
#include <ruby/ruby.h>
#include <ruby/intern.h>

#include "ruby_jsonnet.h"

/*
 * defines the core part of Jsonnet::VM
 */

/*
 * Jsonnet evaluator
 *
 * call-seq:
 *   Jsonnet::VM
 */
static VALUE cVM;

/*
 * Raised on evaluation errors in a Jsonnet VM.
 */
static VALUE eEvaluationError;

static void raise_eval_error(struct JsonnetVm *vm, char *msg, rb_encoding *enc);
static VALUE str_new_json(struct JsonnetVm *vm, char *json, rb_encoding *enc);
static VALUE fileset_new(struct JsonnetVm *vm, char *buf, rb_encoding *enc);

static void vm_free(void *ptr);
static void vm_mark(void *ptr);

const rb_data_type_t jsonnet_vm_type = {
    "JsonnetVm",
    {
	/* dmark = */ vm_mark,
	/* dfree = */ vm_free,
	/* dsize = */ 0,
    },
    /* parent = */ 0,
    /* data = */ 0,
    /* flags = */ RUBY_TYPED_FREE_IMMEDIATELY,
};

struct jsonnet_vm_wrap *
rubyjsonnet_obj_to_vm(VALUE wrap)
{
    struct jsonnet_vm_wrap *vm;
    TypedData_Get_Struct(wrap, struct jsonnet_vm_wrap, &jsonnet_vm_type, vm);

    return vm;
}

static VALUE
vm_s_new(int argc, const VALUE *argv, VALUE klass)
{
    struct jsonnet_vm_wrap *vm;
    VALUE self = TypedData_Make_Struct(cVM, struct jsonnet_vm_wrap, &jsonnet_vm_type, vm);
    vm->vm = jsonnet_make();
    vm->import_callback = Qnil;
    vm->native_callbacks.len = 0;
    vm->native_callbacks.contexts = NULL;

    rb_obj_call_init(self, argc, argv);
    return self;
}

static void
vm_free(void *ptr)
{
    int i;
    struct jsonnet_vm_wrap *vm = (struct jsonnet_vm_wrap *)ptr;
    jsonnet_destroy(vm->vm);

    for (i = 0; i < vm->native_callbacks.len; ++i) {
	struct native_callback_ctx *ctx = vm->native_callbacks.contexts[i];
	RB_REALLOC_N(ctx, struct native_callback_ctx, 0);
    }
    RB_REALLOC_N(vm->native_callbacks.contexts, struct native_callback_ctx *, 0);

    RB_REALLOC_N(vm, struct jsonnet_vm_wrap, 0);
}

static void
vm_mark(void *ptr)
{
    int i;
    struct jsonnet_vm_wrap *vm = (struct jsonnet_vm_wrap *)ptr;

    rb_gc_mark(vm->import_callback);
    for (i = 0; i < vm->native_callbacks.len; ++i) {
	rb_gc_mark(vm->native_callbacks.contexts[i]->callback);
    }
}

static VALUE
vm_evaluate_file(VALUE self, VALUE fname, VALUE encoding, VALUE multi_p)
{
    int error;
    char *result;
    rb_encoding *const enc = rb_to_encoding(encoding);
    struct jsonnet_vm_wrap *vm = rubyjsonnet_obj_to_vm(self);

    FilePathValue(fname);
    if (RTEST(multi_p)) {
	result = jsonnet_evaluate_file_multi(vm->vm, StringValueCStr(fname), &error);
    } else {
	result = jsonnet_evaluate_file(vm->vm, StringValueCStr(fname), &error);
    }

    if (error) {
	raise_eval_error(vm->vm, result, rb_enc_get(fname));
    }
    return RTEST(multi_p) ? fileset_new(vm->vm, result, enc) : str_new_json(vm->vm, result, enc);
}

static VALUE
vm_evaluate(VALUE self, VALUE snippet, VALUE fname, VALUE multi_p)
{
    int error;
    char *result;
    struct jsonnet_vm_wrap *vm = rubyjsonnet_obj_to_vm(self);

    rb_encoding *enc = rubyjsonnet_assert_asciicompat(StringValue(snippet));
    FilePathValue(fname);
    if (RTEST(multi_p)) {
	result = jsonnet_evaluate_snippet_multi(vm->vm, StringValueCStr(fname),
						StringValueCStr(snippet), &error);
    } else {
	result = jsonnet_evaluate_snippet(vm->vm, StringValueCStr(fname), StringValueCStr(snippet),
					  &error);
    }

    if (error) {
	raise_eval_error(vm->vm, result, rb_enc_get(fname));
    }
    return RTEST(multi_p) ? fileset_new(vm->vm, result, enc) : str_new_json(vm->vm, result, enc);
}

#define vm_bind_variable(type, self, key, val)                                    \
    do {                                                                          \
	struct jsonnet_vm_wrap *vm;                                               \
                                                                                  \
	rubyjsonnet_assert_asciicompat(StringValue(key));                         \
	rubyjsonnet_assert_asciicompat(StringValue(val));                         \
	TypedData_Get_Struct(self, struct jsonnet_vm_wrap, &jsonnet_vm_type, vm); \
	jsonnet_##type(vm->vm, StringValueCStr(key), StringValueCStr(val));       \
    } while (0)

/*
 * Binds an external variable to a value.
 * @param [String] key name of the variable
 * @param [String] val the value
 */
static VALUE
vm_ext_var(VALUE self, VALUE key, VALUE val)
{
    vm_bind_variable(ext_var, self, key, val);
    return Qnil;
}

/*
 * Binds an external variable to a code fragment.
 * @param [String] key  name of the variable
 * @param [String] code Jsonnet expression
 */
static VALUE
vm_ext_code(VALUE self, VALUE key, VALUE code)
{
    vm_bind_variable(ext_code, self, key, code);
    return Qnil;
}

/*
 * Binds a top-level argument to a value.
 * @param [String] key name of the variable
 * @param [String] val the value
 */
static VALUE
vm_tla_var(VALUE self, VALUE key, VALUE val)
{
    vm_bind_variable(tla_var, self, key, val);
    return Qnil;
}

/*
 * Binds a top-level argument to a code fragment.
 * @param [String] key  name of the variable
 * @param [String] code Jsonnet expression
 */
static VALUE
vm_tla_code(VALUE self, VALUE key, VALUE code)
{
    vm_bind_variable(tla_code, self, key, code);
    return Qnil;
}

/*
 * Adds library search paths
 */
static VALUE
vm_jpath_add_m(int argc, const VALUE *argv, VALUE self)
{
    int i;
    struct jsonnet_vm_wrap *vm = rubyjsonnet_obj_to_vm(self);

    for (i = 0; i < argc; ++i) {
	VALUE jpath = argv[i];
	FilePathValue(jpath);
	jsonnet_jpath_add(vm->vm, StringValueCStr(jpath));
    }
    return Qnil;
}

static VALUE
vm_set_max_stack(VALUE self, VALUE val)
{
    struct jsonnet_vm_wrap *vm = rubyjsonnet_obj_to_vm(self);
    jsonnet_max_stack(vm->vm, NUM2UINT(val));
    return Qnil;
}

static VALUE
vm_set_gc_min_objects(VALUE self, VALUE val)
{
    struct jsonnet_vm_wrap *vm = rubyjsonnet_obj_to_vm(self);
    jsonnet_gc_min_objects(vm->vm, NUM2UINT(val));
    return Qnil;
}

static VALUE
vm_set_gc_growth_trigger(VALUE self, VALUE val)
{
    struct jsonnet_vm_wrap *vm = rubyjsonnet_obj_to_vm(self);
    jsonnet_gc_growth_trigger(vm->vm, NUM2DBL(val));
    return Qnil;
}

/*
 * Let #evaluate and #evaluate_file return a raw String instead of JSON-encoded string if val is
 * true
 * @param [Boolean] val
 */
static VALUE
vm_set_string_output(VALUE self, VALUE val)
{
    struct jsonnet_vm_wrap *vm = rubyjsonnet_obj_to_vm(self);
    jsonnet_string_output(vm->vm, RTEST(val));
    return Qnil;
}

static VALUE
vm_set_max_trace(VALUE self, VALUE val)
{
    struct jsonnet_vm_wrap *vm = rubyjsonnet_obj_to_vm(self);
    jsonnet_max_trace(vm->vm, NUM2UINT(val));
    return Qnil;
}

void
rubyjsonnet_init_vm(VALUE mJsonnet)
{
    cVM = rb_define_class_under(mJsonnet, "VM", rb_cData);
    rb_define_singleton_method(cVM, "new", vm_s_new, -1);
    rb_define_private_method(cVM, "eval_file", vm_evaluate_file, 3);
    rb_define_private_method(cVM, "eval_snippet", vm_evaluate, 3);
    rb_define_method(cVM, "ext_var", vm_ext_var, 2);
    rb_define_method(cVM, "ext_code", vm_ext_code, 2);
    rb_define_method(cVM, "tla_var", vm_tla_var, 2);
    rb_define_method(cVM, "tla_code", vm_tla_code, 2);
    rb_define_method(cVM, "jpath_add", vm_jpath_add_m, -1);
    rb_define_method(cVM, "max_stack=", vm_set_max_stack, 1);
    rb_define_method(cVM, "gc_min_objects=", vm_set_gc_min_objects, 1);
    rb_define_method(cVM, "gc_growth_trigger=", vm_set_gc_growth_trigger, 1);
    rb_define_method(cVM, "string_output=", vm_set_string_output, 1);
    rb_define_method(cVM, "max_trace=", vm_set_max_trace, 1);

    rubyjsonnet_init_callbacks(cVM);

    eEvaluationError = rb_define_class_under(mJsonnet, "EvaluationError", rb_eRuntimeError);
}

/**
 * raises an EvaluationError whose message is \c msg.
 * @param[in] vm  a JsonnetVM
 * @param[in] msg must be a NUL-terminated string returned by \c vm.
 * @return never returns
 * @throw EvaluationError
 * @sa rescue_callback
 */
static void
raise_eval_error(struct JsonnetVm *vm, char *msg, rb_encoding *enc)
{
    VALUE ex;
    const int state = rubyjsonnet_jump_tag(msg);
    if (state) {
	/*
	 * This is not actually an exception but another type of long jump
	 * with the state, temporarily caught by rescue_callback().
	 */
	jsonnet_realloc(vm, msg, 0);
	rb_jump_tag(state);
    }

    ex = rb_exc_new3(eEvaluationError, rb_enc_str_new_cstr(msg, enc));
    jsonnet_realloc(vm, msg, 0);
    rb_exc_raise(ex);
}

/**
 * Returns a String whose contents is equal to \c json.
 * It automatically frees \c json just after constructing the return value.
 *
 * @param[in] vm   a JsonnetVM
 * @param[in] json must be a NUL-terminated string returned by \c vm.
 * @return Ruby string equal to \c json.
 */
static VALUE
str_new_json(struct JsonnetVm *vm, char *json, rb_encoding *enc)
{
    VALUE str = rb_enc_str_new_cstr(json, enc);
    jsonnet_realloc(vm, json, 0);
    return str;
}

/**
 * Returns a Hash, whose keys are file names in the multi-mode of Jsonnet,
 * and whose values are corresponding JSON values.
 * It automatically frees \c json just after constructing the return value.
 *
 * @param[in] vm   a JsonnetVM
 * @param[in] buf  NUL-separated and double-NUL-terminated sequence of strings returned by \c vm.
 * @return Hash
 */
static VALUE
fileset_new(struct JsonnetVm *vm, char *buf, rb_encoding *enc)
{
    VALUE fileset = rb_hash_new();
    char *ptr, *json;
    for (ptr = buf; *ptr; ptr = json + strlen(json) + 1) {
	json = ptr + strlen(ptr) + 1;
	if (!*json) {
	    VALUE ex = rb_exc_new3(eEvaluationError,
				   rb_enc_sprintf(enc, "output file %s without body", ptr));
	    jsonnet_realloc(vm, buf, 0);
	    rb_exc_raise(ex);
	}

	rb_hash_aset(fileset, rb_enc_str_new_cstr(ptr, enc), rb_enc_str_new_cstr(json, enc));
    }
    jsonnet_realloc(vm, buf, 0);
    return fileset;
}
