#include <libjsonnet.h>
#include <ruby/ruby.h>
#include <ruby/intern.h>

#include "ruby_jsonnet.h"

static struct JsonnetJsonValue *protect_obj_to_json(struct JsonnetVm *vm, VALUE obj,
						    struct JsonnetJsonValue *parent);

/**
 * Converts a Jsonnet JSON value into a Ruby object.
 *
 * Arrays and objects in JSON are not supported due to the limitation of
 * libjsonnet API.
 */
VALUE
rubyjsonnet_json_to_obj(struct JsonnetVm *vm, const struct JsonnetJsonValue *value)
{
    union {
	const char *str;
	double num;
    } typed_value;

    if ((typed_value.str = jsonnet_json_extract_string(vm, value))) {
	return rb_enc_str_new_cstr(typed_value.str, rb_utf8_encoding());
    }
    if (jsonnet_json_extract_number(vm, value, &typed_value.num)) {
	return DBL2NUM(typed_value.num);
    }

    switch (jsonnet_json_extract_bool(vm, value)) {
	case 0:
	    return Qfalse;
	case 1:
	    return Qtrue;
	case 2:
	    break;
	default:
	    /* never happens */
	    rb_raise(rb_eRuntimeError, "unrecognized json bool value");
    }
    if (jsonnet_json_extract_null(vm, value)) {
	return Qnil;
    }

    /* TODO: support arrays and objects when they get accessible */
    rb_raise(rb_eArgError, "unsupported type of JSON value");
}

static struct JsonnetJsonValue *
string_to_json(struct JsonnetVm *vm, VALUE str)
{
    rubyjsonnet_assert_asciicompat(str);
    return jsonnet_json_make_string(vm, RSTRING_PTR(str));
}

static struct JsonnetJsonValue *
num_to_json(struct JsonnetVm *vm, VALUE n)
{
    return jsonnet_json_make_number(vm, NUM2DBL(n));
}

static struct JsonnetJsonValue *
ary_to_json(struct JsonnetVm *vm, VALUE ary)
{
    struct JsonnetJsonValue *const json_array = jsonnet_json_make_array(vm);

    int i;
    for (i = 0; i < RARRAY_LEN(ary); ++i) {
	struct JsonnetJsonValue *const v = protect_obj_to_json(vm, RARRAY_AREF(ary, i), json_array);
	jsonnet_json_array_append(vm, json_array, v);
    }
    return json_array;
}

struct hash_to_json_params {
    struct JsonnetVm *vm;
    struct JsonnetJsonValue *obj;
};

static int
hash_item_to_json(VALUE key, VALUE value, VALUE paramsval)
{
    const struct hash_to_json_params *const params = (const struct hash_to_json_params *)paramsval;

    StringValue(key);
    rubyjsonnet_assert_asciicompat(key);

    jsonnet_json_object_append(params->vm, params->obj, StringValueCStr(key),
			       protect_obj_to_json(params->vm, value, params->obj));

    return ST_CONTINUE;
}

static struct JsonnetJsonValue *
hash_to_json(struct JsonnetVm *vm, VALUE hash)
{
    struct JsonnetJsonValue *const json_obj = jsonnet_json_make_object(vm);
    const struct hash_to_json_params args = {vm, json_obj};
    rb_hash_foreach(hash, hash_item_to_json, (VALUE)&args);
    return json_obj;
}

/**
 * Converts a Ruby object into a Jsonnet JSON value
 *
 * TODO(yugui): Safely destorys an intermediate object on exception.
 */
static struct JsonnetJsonValue *
obj_to_json(struct JsonnetVm *vm, VALUE obj)
{
    VALUE converted;

    switch (obj) {
	case Qnil:
	    return jsonnet_json_make_null(vm);
	case Qtrue:
	    return jsonnet_json_make_bool(vm, 1);
	case Qfalse:
	    return jsonnet_json_make_bool(vm, 0);
    }

    converted = rb_check_string_type(obj);
    if (converted != Qnil) {
	return string_to_json(vm, converted);
    }

    converted = rb_check_to_float(obj);
    if (converted != Qnil) {
	return num_to_json(vm, converted);
    }

    converted = rb_check_array_type(obj);
    if (converted != Qnil) {
	return ary_to_json(vm, converted);
    }

    converted = rb_check_hash_type(obj);
    if (converted != Qnil) {
	return hash_to_json(vm, converted);
    }

    converted = rb_any_to_s(obj);
    return string_to_json(vm, converted);
}

struct protect_args {
    struct JsonnetVm *vm;
    VALUE obj;
};

static VALUE
protect_obj_to_json_block(VALUE paramsval)
{
    const struct protect_args *const params = (const struct protect_args *)paramsval;
    return (VALUE)obj_to_json(params->vm, params->obj);
}

/**
 * Safely converts a Ruby object into a JSON value.
 *
 * It automatically destroys \a parent on exception.
 * @param[in] vm  a Jsonnet VM
 * @param[in] obj a Ruby object to be converted
 * @param[in] parent destroys this value on failure
 * @throws can throw \c TypeError or other exceptions
 */
static struct JsonnetJsonValue *
protect_obj_to_json(struct JsonnetVm *vm, VALUE obj, struct JsonnetJsonValue *parent)
{
    const struct protect_args args = {vm, obj};
    int state = 0;

    VALUE result = rb_protect(protect_obj_to_json_block, (VALUE)&args, &state);
    if (!state) {
	return (struct JsonnetJsonValue *)result;
    }

    jsonnet_json_destroy(vm, parent);
    rb_jump_tag(state);
}

/**
 * Converts a Ruby object into a JSON value.
 * Returns an error message on failure.
 *
 * @param[in] vm  a Jsonnet VM
 * @param[in] obj a Ruby object to be converted
 * @param[out] success  set to 1 on success, set to 0 on failure.
 * @returns the converted value on success, an error message on failure.
 */
struct JsonnetJsonValue *
rubyjsonnet_obj_to_json(struct JsonnetVm *vm, VALUE obj, int *success)
{
    int state = 0;
    const struct protect_args args = {vm, obj};
    VALUE result = rb_protect(protect_obj_to_json_block, (VALUE)&args, &state);
    if (state) {
	const VALUE msg = rubyjsonnet_format_exception(rb_errinfo());
	rb_set_errinfo(Qnil);
	*success = 0;
	return string_to_json(vm, msg);
    }
    *success = 1;
    return (struct JsonnetJsonValue *)result;
}
