#include <ruby/ruby.h>
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

static VALUE
vm_evaluate_file(VALUE self, VALUE fname) {
    struct JsonnetVm *vm;
    int error;
    char* result;

    TypedData_Get_Struct(self, struct JsonnetVm, &jsonnet_vm_type, vm);
    FilePathValue(fname);
    result = jsonnet_evaluate_file(vm, StringValueCStr(fname), &error);
    if (error) {
        rb_raise(eEvaluationError, "%s", result);
    }
    return rb_utf8_str_new_cstr(result);
}

static VALUE
vm_evaluate(VALUE self, VALUE fname, VALUE snippet) {
    struct JsonnetVm *vm;
    int error;
    char* result;

    TypedData_Get_Struct(self, struct JsonnetVm, &jsonnet_vm_type, vm);
    FilePathValue(fname);
    result = jsonnet_evaluate_snippet(
            vm,
            StringValueCStr(snippet), StringValueCStr(fname),
            &error);
    if (error) {
        rb_raise(eEvaluationError, "%s", result);
    }
    return rb_utf8_str_new_cstr(result);
}

void
Init_jsonnet_wrap(void) {
    VALUE mJsonnet = rb_define_module("Jsonnet");
    rb_define_singleton_method(mJsonnet, "libversion", jw_s_version, 0);

    cVM = rb_define_class_under(mJsonnet, "VM", rb_cData);
    rb_define_singleton_method(cVM, "new", vm_s_new, 0);
    rb_define_method(cVM, "evaluate_file", vm_evaluate_file, 1);
    rb_define_method(cVM, "evaluate", vm_evaluate, 2);

    eEvaluationError = rb_define_class_under(mJsonnet, "EvaluationError", rb_eRuntimeError);
}
