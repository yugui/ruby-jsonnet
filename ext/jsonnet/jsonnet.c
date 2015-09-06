#include <string.h>
#include <ruby/ruby.h>
#include <ruby/intern.h>
#include <libjsonnet.h>

/*
 * Jsonnet evaluator
 */
static VALUE cVM;
static VALUE eEvaluationError;

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

/**
 * raises an EvaluationError whose message is \c msg.
 * @param[in] vm  a JsonnetVM
 * @param[in] msg must be a NUL-terminated string returned by \c vm.
 * @return never returns
 * @throw EvaluationError
 */
static void
raise_eval_error(struct JsonnetVm *vm, char *msg)
{
    VALUE ex = rb_exc_new_cstr(eEvaluationError, msg);
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
str_new_json(struct JsonnetVm *vm, char *json)
{
    VALUE str = rb_utf8_str_new_cstr(json);
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
fileset_new(struct JsonnetVm *vm, char *buf)
{
    VALUE fileset = rb_hash_new();
    char *ptr, *json;
    for (ptr = buf; *ptr; ptr = json + strlen(json)+1) {
        json = ptr + strlen(ptr) + 1;
        if (!*json) {
            VALUE ex = rb_exc_new3(
                    eEvaluationError,
                    rb_sprintf("output file %s without body", ptr));
            jsonnet_realloc(vm, buf, 0);
            rb_exc_raise(ex);
        }

        rb_hash_aset(fileset, rb_utf8_str_new_cstr(ptr), rb_utf8_str_new_cstr(json));
    }
    jsonnet_realloc(vm, buf, 0);
    return fileset;
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
vm_s_new(VALUE mod) {
    struct JsonnetVm *const vm = jsonnet_make();
    return TypedData_Wrap_Struct(cVM, &jsonnet_vm_type, vm);
}

static void
vm_free(void *ptr) {
    jsonnet_destroy((struct JsonnetVm*)ptr);
}

static VALUE
vm_evaluate_file(VALUE self, VALUE fname, VALUE multi_p)
{
    struct JsonnetVm *vm;
    int error;
    char *result;

    TypedData_Get_Struct(self, struct JsonnetVm, &jsonnet_vm_type, vm);
    FilePathValue(fname);
    if (RTEST(multi_p)) {
        result = jsonnet_evaluate_file_multi(vm, StringValueCStr(fname), &error);
    }
    else {
        result = jsonnet_evaluate_file(vm, StringValueCStr(fname), &error);
    }

    if (error) {
        raise_eval_error(vm, result);
    }
    return RTEST(multi_p) ? fileset_new(vm, result) : str_new_json(vm, result);
}

static VALUE
vm_evaluate(VALUE self, VALUE snippet, VALUE fname, VALUE multi_p)
{
    struct JsonnetVm *vm;
    int error;
    char *result;

    TypedData_Get_Struct(self, struct JsonnetVm, &jsonnet_vm_type, vm);
    FilePathValue(fname);
    if (RTEST(multi_p)) {
        result = jsonnet_evaluate_snippet_multi(
                vm,
                StringValueCStr(fname), StringValueCStr(snippet), &error);
    }
    else {
        result = jsonnet_evaluate_snippet(
                vm,
                StringValueCStr(fname), StringValueCStr(snippet), &error);
    }

    if (error) {
        raise_eval_error(vm, result);
    }
    return RTEST(multi_p) ? fileset_new(vm, result) : str_new_json(vm, result);
}

void
Init_jsonnet_wrap(void)
{
    VALUE mJsonnet = rb_define_module("Jsonnet");
    rb_define_singleton_method(mJsonnet, "libversion", jw_s_version, 0);

    cVM = rb_define_class_under(mJsonnet, "VM", rb_cData);
    rb_define_singleton_method(cVM, "new", vm_s_new, 0);
    rb_define_private_method(cVM, "eval_file", vm_evaluate_file, 2);
    rb_define_private_method(cVM, "eval_snippet", vm_evaluate, 3);

    eEvaluationError = rb_define_class_under(mJsonnet, "EvaluationError", rb_eRuntimeError);
}
