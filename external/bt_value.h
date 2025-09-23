#pragma once

#if __cplusplus
extern "C" {
#endif

#include "bt_prelude.h"

typedef uint64_t bt_Value;
typedef struct bt_Object bt_Object;
typedef struct bt_Type bt_Type;

// IEEE 756 DOUBLE       S[Exponent-][Mantissa------------------------------------------]
#define BT_SIGN_BIT   (0b1000000000000000000000000000000000000000000000000000000000000000)
#define BT_EXPONENT   (0b0111111111110000000000000000000000000000000000000000000000000000)
#define BT_QNAN_BIT   (0b0000000000001000000000000000000000000000000000000000000000000000)
#define BT_SLOW_MASK  (0b0000000000000100000000000000000000000000000000000000000000000000)
#define BT_TYPE_MASK  (0b0000000000000011000000000000000000000000000000000000000000000000)
#define BT_VALUE_MASK (0b0000000000000000111111111111111111111111111111111111111111111111)

#define BT_NAN_MASK (BT_SIGN_BIT | BT_EXPONENT | BT_QNAN_BIT)

#define BT_TYPE_NULL    (0b0000000000000000000000000000000000000000000000000000000000000000)
#define BT_TYPE_BOOL    (0b0000000000000001000000000000000000000000000000000000000000000000)
#define BT_TYPE_ENUM    (0b0000000000000010000000000000000000000000000000000000000000000000)
#define BT_TYPE_OBJECT  (0b0000000000000011000000000000000000000000000000000000000000000000)

#define BT_VALUE_NULL       ((bt_Value)(BT_NAN_MASK | BT_TYPE_NULL))
#define BT_VALUE_FALSE      ((bt_Value)(BT_NAN_MASK | BT_TYPE_BOOL))
#define BT_VALUE_TRUE       ((bt_Value)(BT_NAN_MASK | (BT_TYPE_BOOL | 1)))
#define BT_VALUE_BOOL(x)    ((x) ? BT_VALUE_TRUE : BT_VALUE_FALSE)
#define BT_VALUE_NUMBER(x)  (bt_make_number((bt_number)(x)))
#define BT_VALUE_ENUM(x)    ((bt_Value)(BT_NAN_MASK | BT_TYPE_ENUM | (uint32_t)x))
#define BT_VALUE_OBJECT(x)  ((bt_Value)(BT_NAN_MASK | (BT_TYPE_OBJECT | (bt_Value)x)))

#define BT_IS_NUMBER(x)   (((x) & BT_NAN_MASK) != BT_NAN_MASK)
#define BT_IS_NULL(x)     ((x) == BT_VALUE_NULL)
#define BT_IS_BOOL(x)     (x == BT_VALUE_TRUE || x == BT_VALUE_FALSE)
#define BT_IS_TRUE(x)     (x == BT_VALUE_TRUE)
#define BT_IS_FALSE(x)    (x == BT_VALUE_FALSE)
#define BT_IS_TRUTHY(x)   (!(x == BT_VALUE_FALSE || x == BT_VALUE_NULL))
#define BT_IS_ENUM(x)     (!BT_IS_NUMBER(x) && (x & BT_TYPE_MASK) == BT_TYPE_ENUM)
#define BT_IS_OBJECT(x)   (!BT_IS_NUMBER(x) && (x & BT_TYPE_MASK) == BT_TYPE_OBJECT)

#define BT_IS_FAST(x)    (((x) & BT_SLOW_MASK) == 0)
#define BT_MAKE_SLOW(x)  (((x) | BT_SLOW_MASK))

#define BT_TYPEOF(x) ((x) & BT_TYPE_MASK)

#define BT_AS_NUMBER(x) (bt_get_number((bt_Value)x))
#define BT_AS_ENUM(x)   (((bt_Value)x) & 0xFFFFFFFFu)
#define BT_AS_OBJECT(x) ((bt_Object*)(BT_VALUE_MASK & ((bt_Value)x)))

/** Compare two bt_Value's, returning whether they're equal */
BOLT_API bt_bool bt_value_is_equal(bt_Value a, bt_Value b);

/** Generates a default value fo the supplied `type`, preferring the simplest types in case of unions */
BOLT_API bt_Value bt_default_value(bt_Context* ctx, bt_Type* type);

/** Creates the bt_Value representation of `null` */
BOLT_API bt_Value bt_make_null();
/** Returns whether a bt_Value is exactly equal to `null` */
BOLT_API bt_bool bt_is_null(bt_Value val);

/** Creates a boxed number value */
BOLT_API bt_Value bt_make_number(bt_number num);
/** Returns whether the bt_Value contains a number value */
BOLT_API bt_bool bt_is_number(bt_Value val);
/** Get the boxed number value from the bt_Value, assumes type is correct */
BOLT_API bt_number bt_get_number(bt_Value val);

/** Creates a boxed boolean value */
BOLT_API bt_Value bt_make_bool(bt_bool cond);
/** Returns whether the bt_Value contains a boolean */
BOLT_API bt_bool bt_is_bool(bt_Value val);
/** Get the boxed boolean from the bt_Value, assumes type is correct */
BOLT_API bt_bool bt_get_bool(bt_Value val);

/** Creates a weakly typed enum value (32-bit integer) */
BOLT_API bt_Value bt_make_enum_val(uint32_t val);
/** Returns whether the bt_Value contains a boxed enum */
BOLT_API bt_bool bt_is_enum_val(bt_Value val);
/** Get the boxed enum value from the bt_Value, assumes type is correct */
BOLT_API uint32_t bt_get_enum_val(bt_Value val);

/** Box a bt_object pointer into a bt_Value stack value */
BOLT_API bt_Value bt_value(bt_Object* obj);
/** Returns whether the bt_Value contains a masked bt_Object */
BOLT_API bt_bool bt_is_object(bt_Value val);
/** Get the boxed object pointer from the bt_Value, assumes type is correct */
BOLT_API bt_Object* bt_object(bt_Value val);

#if __cplusplus
}
#endif