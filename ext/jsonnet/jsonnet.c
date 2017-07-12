#include <string.h>
#include <ruby/ruby.h>
#include <ruby/intern.h>
#include <ruby/encoding.h>
#include <libjsonnet.h>

static ID id_call;
static ID id_message;

/*
 * Jsonnet evaluator
 */
static VALUE cVM;
static VALUE eEvaluationError;
/**
 * Indicates that the encoding of the given string is not allowed by
 * the C++ implementation of Jsonnet.
 */
static VALUE eUnsupportedEncodingError;

struct jsonnet_vm_wrap {
    struct JsonnetVm *vm;
    VALUE callback;
};

static void vm_free(void *ptr);
static void vm_mark(void *ptr);

static const rb_data_type_t jsonnet_vm_type = {
    "JsonnetVm",
    {
      /* dmark = */ vm_mark,
      /* dfree = */ vm_free,
      /* dsize = */ 0,
    },
    /* parent = */ 0,
    /* data = */ 0,
    /* flags = */ RUBY_TYPED_FREE_IMMEDIATELY
};

/**
 * raises an EvaluationError whose message is \c msg.
 * @param[in] vm  a JsonnetVM
 * @param[in] msg must be a NUL-terminated string returned by \c vm.
 * @return never returns
 * @throw EvaluationError
 */
static void
raise_eval_error(struct JsonnetVm *vm, char *msg, rb_encoding *enc)
{
    VALUE ex = rb_exc_new3(eEvaluationError, rb_enc_str_new_cstr(msg, enc));
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
    for (ptr = buf; *ptr; ptr = json + strlen(json)+1) {
        json = ptr + strlen(ptr) + 1;
        if (!*json) {
            VALUE ex = rb_exc_new3(
                    eEvaluationError,
                    rb_enc_sprintf(enc, "output file %s without body", ptr));
            jsonnet_realloc(vm, buf, 0);
            rb_exc_raise(ex);
        }

        rb_hash_aset(fileset, rb_enc_str_new_cstr(ptr, enc), rb_enc_str_new_cstr(json, enc));
    }
    jsonnet_realloc(vm, buf, 0);
    return fileset;
}

/**
 * Allocates a C string whose content is equal to \c str with jsonnet_realloc.
 */
static char *
str_jsonnet_cstr(struct JsonnetVm *vm, VALUE str)
{
    const char *const cstr = StringValueCStr(str);
    char *const buf = jsonnet_realloc(vm, NULL, strlen(cstr));
    strcpy(buf, cstr);
    return buf;
}

static rb_encoding *
enc_assert_asciicompat(VALUE str) {
    rb_encoding *enc = rb_enc_get(str);
    if (!rb_enc_asciicompat(enc)) {
        rb_raise(
            eUnsupportedEncodingError,
            "jsonnet encoding must be ASCII-compatible but got %s",
            rb_enc_name(enc));
    }
    return enc;
}

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

static VALUE
vm_s_new(int argc, const VALUE *argv, VALUE klass)
{
    struct jsonnet_vm_wrap *vm;
    VALUE self = TypedData_Make_Struct(cVM, struct jsonnet_vm_wrap, &jsonnet_vm_type, vm);
    vm->vm = jsonnet_make();
    vm->callback = Qnil;
    rb_obj_call_init(self, argc, argv);
    return self;
}

static void
vm_free(void *ptr)
{
    struct jsonnet_vm_wrap *vm = (struct jsonnet_vm_wrap*)ptr;
    jsonnet_destroy(vm->vm);
    REALLOC_N(vm, struct jsonnet_vm_wrap, 0);
}

static void
vm_mark(void *ptr)
{
    struct jsonnet_vm_wrap *vm = (struct jsonnet_vm_wrap*)ptr;
    rb_gc_mark(vm->callback);
}

static VALUE
vm_evaluate_file(VALUE self, VALUE fname, VALUE encoding, VALUE multi_p)
{
    struct jsonnet_vm_wrap *vm;
    int error;
    char *result;
    rb_encoding *const enc = rb_to_encoding(encoding);

    TypedData_Get_Struct(self, struct jsonnet_vm_wrap, &jsonnet_vm_type, vm);
    FilePathValue(fname);
    if (RTEST(multi_p)) {
        result = jsonnet_evaluate_file_multi(vm->vm, StringValueCStr(fname), &error);
    }
    else {
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
    struct jsonnet_vm_wrap *vm;
    int error;
    char *result;

    rb_encoding *enc = enc_assert_asciicompat(StringValue(snippet));
    TypedData_Get_Struct(self, struct jsonnet_vm_wrap, &jsonnet_vm_type, vm);
    FilePathValue(fname);
    if (RTEST(multi_p)) {
        result = jsonnet_evaluate_snippet_multi(
                vm->vm,
                StringValueCStr(fname), StringValueCStr(snippet), &error);
    }
    else {
        result = jsonnet_evaluate_snippet(
                vm->vm,
                StringValueCStr(fname), StringValueCStr(snippet), &error);
    }

    if (error) {
        raise_eval_error(vm->vm, result, rb_enc_get(fname));
    }
    return RTEST(multi_p) ? fileset_new(vm->vm, result, enc) : str_new_json(vm->vm, result, enc);
}

static VALUE
import_callback_thunk0(VALUE args)
{
    VALUE callback = rb_ary_entry(args, 0);
    return rb_funcall(callback, id_call, 2, rb_ary_entry(args, 1), rb_ary_entry(args, 2));
}

static char *
import_callback_thunk(void *ctx, const char *base, const char *rel, char **found_here, int *success)
{
    struct jsonnet_vm_wrap *const vm = (struct jsonnet_vm_wrap*)ctx;
    int state;
    VALUE result, args;

    args = rb_ary_tmp_new(3);
    rb_ary_push(args, vm->callback);
    rb_ary_push(args, rb_enc_str_new_cstr(base, rb_filesystem_encoding()));
    rb_ary_push(args, rb_enc_str_new_cstr(rel, rb_filesystem_encoding()));
    result = rb_protect(import_callback_thunk0, args, &state);

    if (state) {
        VALUE err = rb_errinfo();
        VALUE msg, name;

        rb_set_errinfo(Qnil);
        name = rb_class_name(rb_obj_class(err));
        msg = rb_funcall(err, id_message, 0);
        if (rb_str_strlen(name)) {
            if (rb_str_strlen(msg)) {
                msg = rb_str_concat(rb_str_cat_cstr(name, " : "), msg);
            } else {
                msg = name;
            }
        } else if (!rb_str_strlen(msg)) {
            msg = rb_sprintf("cannot import %s from %s", rel, base);
        }
        *success = 0;
        return str_jsonnet_cstr(vm->vm, msg);
    }

    result = rb_Array(result);
    *success = 1;
    *found_here = str_jsonnet_cstr(vm->vm, rb_ary_entry(result, 1));
    return str_jsonnet_cstr(vm->vm, rb_ary_entry(result, 0));
}

/**
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
    struct jsonnet_vm_wrap *vm;
    TypedData_Get_Struct(self, struct jsonnet_vm_wrap, &jsonnet_vm_type, vm);

    vm->callback = callback;
    jsonnet_import_callback(vm->vm, import_callback_thunk, vm);
    return callback;
}

#define vm_bind_variable(type, self, key, val) \
    do { \
        struct jsonnet_vm_wrap *vm; \
        \
        enc_assert_asciicompat(StringValue(key)); \
        enc_assert_asciicompat(StringValue(val)); \
        TypedData_Get_Struct(self, struct jsonnet_vm_wrap, &jsonnet_vm_type, vm); \
        jsonnet_##type(vm->vm, StringValueCStr(key), StringValueCStr(val)); \
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

static VALUE
vm_set_max_stack(VALUE self, VALUE val)
{
    struct jsonnet_vm_wrap *vm;
    TypedData_Get_Struct(self, struct jsonnet_vm_wrap, &jsonnet_vm_type, vm);
    jsonnet_max_stack(vm->vm, NUM2UINT(val));
    return Qnil;
}

static VALUE
vm_set_gc_min_objects(VALUE self, VALUE val)
{
    struct jsonnet_vm_wrap *vm;
    TypedData_Get_Struct(self, struct jsonnet_vm_wrap, &jsonnet_vm_type, vm);
    jsonnet_gc_min_objects(vm->vm, NUM2UINT(val));
    return Qnil;
}

static VALUE
vm_set_gc_growth_trigger(VALUE self, VALUE val)
{
    struct jsonnet_vm_wrap *vm;
    TypedData_Get_Struct(self, struct jsonnet_vm_wrap, &jsonnet_vm_type, vm);
    jsonnet_gc_growth_trigger(vm->vm, NUM2DBL(val));
    return Qnil;
}

/*
 * Let #evaluate and #evaluate_file return a raw String instead of JSON-encoded string if val is true
 * @param [Boolean] val
 */
static VALUE
vm_set_string_output(VALUE self, VALUE val)
{
    struct jsonnet_vm_wrap *vm;
    TypedData_Get_Struct(self, struct jsonnet_vm_wrap, &jsonnet_vm_type, vm);
    jsonnet_string_output(vm->vm, RTEST(val));
    return Qnil;
}

static VALUE
vm_set_max_trace(VALUE self, VALUE val)
{
    struct jsonnet_vm_wrap *vm;
    TypedData_Get_Struct(self, struct jsonnet_vm_wrap, &jsonnet_vm_type, vm);
    jsonnet_max_trace(vm->vm, NUM2UINT(val));
    return Qnil;
}

void
Init_jsonnet_wrap(void)
{
    id_call = rb_intern("call");
    id_message = rb_intern("message");

    VALUE mJsonnet = rb_define_module("Jsonnet");
    rb_define_singleton_method(mJsonnet, "libversion", jw_s_version, 0);

    cVM = rb_define_class_under(mJsonnet, "VM", rb_cData);
    rb_define_singleton_method(cVM, "new", vm_s_new, -1);
    rb_define_private_method(cVM, "eval_file", vm_evaluate_file, 3);
    rb_define_private_method(cVM, "eval_snippet", vm_evaluate, 3);
    rb_define_method(cVM, "ext_var", vm_ext_var, 2);
    rb_define_method(cVM, "ext_code", vm_ext_code, 2);
    rb_define_method(cVM, "tla_var", vm_tla_var, 2);
    rb_define_method(cVM, "max_stack=", vm_set_max_stack, 1);
    rb_define_method(cVM, "gc_min_objects=", vm_set_gc_min_objects, 1);
    rb_define_method(cVM, "gc_growth_trigger=", vm_set_gc_growth_trigger, 1);
    rb_define_method(cVM, "string_output=", vm_set_string_output, 1);
    rb_define_method(cVM, "max_trace=", vm_set_max_trace, 1);
    rb_define_method(cVM, "import_callback=", vm_set_import_callback, 1);

    eEvaluationError = rb_define_class_under(mJsonnet, "EvaluationError", rb_eRuntimeError);
    eUnsupportedEncodingError =
        rb_define_class_under(mJsonnet, "UnsupportedEncodingError", rb_eEncodingError);
}
