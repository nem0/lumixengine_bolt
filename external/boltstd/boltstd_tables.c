#include "boltstd_tables.h"

#include "../bt_embedding.h"

static const char* bt_table_pairs_iter_fn_name = "pairs_iter";
static const char* bt_pair_key_key_name = "key";
static const char* bt_pair_value_key_name = "value";

static bt_Type* make_table_pair_type(bt_Context* ctx, bt_Type* tbl)
{
	bt_Type* return_type = bt_make_tableshape_type(ctx, "Pair", BT_TRUE);
	bt_Type* key_type = tbl->as.table_shape.key_type ? tbl->as.table_shape.key_type : ctx->types.any;
	bt_Type* value_type = tbl->as.table_shape.value_type ? bt_type_remove_nullable(ctx, tbl->as.table_shape.value_type) : ctx->types.any;
	bt_tableshape_add_layout(ctx, return_type, ctx->types.string, BT_VALUE_CSTRING(ctx, bt_pair_key_key_name), key_type);
	bt_tableshape_add_layout(ctx, return_type, ctx->types.string, BT_VALUE_CSTRING(ctx, bt_pair_value_key_name), value_type);

	return return_type;
}

static bt_Type* bt_table_pairs_type(bt_Context* ctx, bt_Type** args, uint8_t argc)
{
	if (argc != 1) return NULL;
	bt_Type* arg = bt_type_dealias(args[0]);

	if (arg->category != BT_TYPE_CATEGORY_TABLESHAPE) return NULL;

	bt_Type* return_type = make_table_pair_type(ctx, arg);
	bt_Type* iter_sig = bt_make_signature_type(ctx, bt_type_make_nullable(ctx, return_type), NULL, 0);

	return bt_make_signature_type(ctx, iter_sig, args, argc);
}

static void bt_table_pairs(bt_Context* ctx, bt_Thread* thread)
{
	bt_Module* module = bt_get_module(thread);
	bt_Value iter_fn = bt_module_get_storage(module, BT_VALUE_CSTRING(ctx, bt_table_pairs_iter_fn_name));
	
	bt_push(thread, iter_fn);
	bt_push(thread, bt_arg(thread, 0));
	bt_push(thread, BT_VALUE_NUMBER(0));

	bt_return(thread, bt_make_closure(thread, 2));
}

static void bt_table_pairs_iter(bt_Context* ctx, bt_Thread* thread)
{
	bt_Table* tbl = (bt_Table*)BT_AS_OBJECT(bt_getup(thread, 0));
	uint16_t idx = (uint16_t)BT_AS_NUMBER(bt_getup(thread, 1));

	if (idx >= tbl->length) {
		bt_return(thread, BT_VALUE_NULL);
	}
	else {
		bt_TablePair* raw_pair = BT_TABLE_PAIRS(tbl) + idx;
		idx++;

		bt_Table* pair = bt_make_table(ctx, 2);
		bt_table_set(ctx, pair, BT_VALUE_CSTRING(ctx, bt_pair_key_key_name), raw_pair->key);
		bt_table_set(ctx, pair, BT_VALUE_CSTRING(ctx, bt_pair_value_key_name), raw_pair->value);

		bt_return(thread, BT_VALUE_OBJECT(pair));
		bt_setup(thread, 1, BT_VALUE_NUMBER(idx));
	}
}

static bt_Type* bt_table_delete_type(bt_Context* ctx, bt_Type** args, uint8_t argc)
{
	if (argc != 2) return NULL;
	bt_Type* tbl = bt_type_dealias(args[0]);
	bt_Type* key = bt_type_dealias(args[1]);

	if (tbl->as.table_shape.sealed) return NULL;
	if (tbl->as.table_shape.key_type && !key->satisfier(tbl->as.table_shape.key_type, key)) return NULL;

	return bt_make_signature_type(ctx, ctx->types.boolean, args, argc);
}

static void bt_table_delete(bt_Context* ctx, bt_Thread* thread)
{
	bt_Table* tbl = (bt_Table*)BT_AS_OBJECT(bt_arg(thread, 0));
	bt_Value key = bt_arg(thread, 1);

	bt_bool result = bt_table_delete_key(tbl, key);

	bt_return(thread, BT_VALUE_BOOL(result));
}

static void bt_table_length(bt_Context* ctx, bt_Thread* thread)
{
	bt_Table* tbl = (bt_Table*)BT_AS_OBJECT(bt_arg(thread, 0));
	bt_return(thread, BT_VALUE_NUMBER(tbl->length));
}

void boltstd_open_tables(bt_Context* context)
{
	bt_Module* module = bt_make_module(context);

	bt_Value bt_table_pairs_iter_fn = BT_VALUE_OBJECT(bt_make_native(context, module, NULL, bt_table_pairs_iter));
	bt_module_set_storage(module, BT_VALUE_CSTRING(context, bt_table_pairs_iter_fn_name), bt_table_pairs_iter_fn);
	
	bt_Type* table_pairs_sig = bt_make_poly_signature_type(context, "pairs({}): fn: Pair?", bt_table_pairs_type);
	bt_NativeFn* fn_ref = bt_make_native(context, module, table_pairs_sig, bt_table_pairs);
	bt_module_export(context, module, table_pairs_sig, BT_VALUE_CSTRING(context, "pairs"), BT_VALUE_OBJECT(fn_ref));

	bt_Type* table_delete_sig = bt_make_poly_signature_type(context, "delete({}, any): bool", bt_table_delete_type);
	fn_ref = bt_make_native(context, module, table_delete_sig, bt_table_delete);
	bt_module_export(context, module, table_delete_sig, BT_VALUE_CSTRING(context, "delete"), BT_VALUE_OBJECT(fn_ref));

	bt_Type* table = bt_type_table(context);
	bt_module_export_native(context, module, "length", bt_table_length, bt_type_number(context), &table, 1);
	
	bt_register_module(context, BT_VALUE_CSTRING(context, "tables"), module);
}