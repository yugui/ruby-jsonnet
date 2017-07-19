#include <ruby/ruby.h>
#include <libjsonnet.h>

#include "ruby_jsonnet.h"

/*
 * callback support in VM
 */

static ID id_call;

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
        VALUE msg = rubyjsonnet_format_exception(rb_errinfo());
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

void
rubyjsonnet_init_callbacks(VALUE cVM)
{
    id_call = rb_intern("call");

    rb_define_method(cVM, "import_callback=", vm_set_import_callback, 1);
}
