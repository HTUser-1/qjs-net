#include "closure.h"
#include "context.h"

union closure*
closure_new(JSContext* ctx) {
  union closure* closure;

  if((closure = js_mallocz(ctx, sizeof(union closure)))) {
    closure->ref_count = 1;
    closure->ctx = ctx;
  }

  return closure;
}

void
closure_free_object(void* ptr, JSRuntime* rt) {
  JS_FreeValueRT(rt, JS_MKPTR(JS_TAG_OBJECT, ptr));
}

union closure*
closure_object(JSContext* ctx, JSValueConst val) {
  JSObject* obj = JS_VALUE_GET_OBJ(val);
  union closure* ret;
  JS_DupValue(ctx, val);

  ret = closure_new(ctx);
  ret->pointer = obj;
  ret->free_func = &closure_free_object;
}

union closure*
closure_dup(union closure* c) {
  ++c->ref_count;
  return c;
}

void
closure_free(void* ptr) {
  union closure* closure = ptr;

  if(--closure->ref_count == 0) {
    JSContext* ctx = closure->ctx;
    // printf("%s() pointer=%p\n", __func__, closure->pointer);

    if(closure->free_func) {
      closure->free_func(closure->pointer, JS_GetRuntime(ctx));
      closure->pointer = NULL;
    }

    js_free(ctx, closure);
  }
}
