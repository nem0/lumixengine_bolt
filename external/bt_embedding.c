#include "bt_embedding.h"
#include "bt_context.h"

BOLT_API uint8_t bt_argc(bt_Thread* thread)
{
	return thread->native_stack[thread->native_depth - 1].argc;
}

BOLT_API bt_Value bt_arg(bt_Thread* thread, uint8_t idx)
{
	return thread->stack[thread->top + idx];
}

BOLT_API void bt_return(bt_Thread* thread, bt_Value value)
{
	thread->stack[thread->top + thread->native_stack[thread->native_depth - 1].return_loc] = value;
}

BOLT_API bt_Value bt_get_returned(bt_Thread* thread)
{
	return thread->stack[thread->top];
}

BOLT_API bt_Value bt_getup(bt_Thread* thread, uint8_t idx)
{
	return BT_CLOSURE_UPVALS(BT_STACKFRAME_GET_CALLABLE(thread->callstack[thread->depth - 1]))[idx];
}

BOLT_API void bt_setup(bt_Thread* thread, uint8_t idx, bt_Value value)
{
	BT_CLOSURE_UPVALS(BT_STACKFRAME_GET_CALLABLE(thread->callstack[thread->depth - 1]))[idx] = value;
}

bt_Module* bt_get_module(bt_Thread* thread)
{
	bt_Callable* callable = BT_STACKFRAME_GET_CALLABLE(thread->callstack[thread->depth - 1]);
	return bt_get_owning_module(callable);
}