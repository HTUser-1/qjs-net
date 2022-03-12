#ifndef MINNET_JSUTILS_H
#define MINNET_JSUTILS_H

#include <quickjs.h>
#include <cutils.h>
#include <list.h>

typedef struct JSThreadState {
  struct list_head os_rw_handlers;
  struct list_head os_signal_handlers;
  struct list_head os_timers;
  struct list_head port_list;
  int eval_script_recurse;
  void *recv_pipe, *send_pipe;
} JSThreadState;

typedef struct input_buffer {
  uint8_t* data;
  size_t size;
  void (*free)(JSRuntime*, void* opaque, void* ptr);
  JSValue value;
} JSBuffer;

typedef union resolve_functions {
  JSValue array[2];
  struct {
    JSValue resolve, reject;
  };
} ResolveFunctions;

JSValue vector2array(JSContext*, int, JSValue);
void js_console_log(JSContext*, JSValue*, JSValue*);
JSValue js_function_bound(JSContext*, JSValue, int, JSValue argv[], int magic, JSValue* func_data);
JSValue js_function_bind(JSContext*, JSValue, int, JSValue argv[]);
JSValue js_function_bind_1(JSContext*, JSValue, JSValue);
JSValue js_iterator_next(JSContext*, JSValue, JSValue*, BOOL* done_p, int argc, JSValue argv[]);
int js_copy_properties(JSContext*, JSValue, JSValue, int flags);
JSBuffer js_buffer_from(JSContext*, JSValue);
void js_buffer_to(JSBuffer, void**, size_t*);
void js_buffer_to3(JSBuffer, const char**, void**, size_t* plen);
BOOL js_buffer_valid(const JSBuffer*);
JSBuffer js_buffer_clone(const JSBuffer*, JSContext*);
void js_buffer_dump(const JSBuffer*, DynBuf*);
void js_buffer_free(JSBuffer*, JSContext*);
BOOL js_is_iterable(JSContext*, JSValue);
BOOL js_is_iterator(JSContext*, JSValue);
BOOL js_is_promise(JSContext*, JSValue);
JSAtom js_symbol_static_atom(JSContext*, const char*);
JSValue js_symbol_static_value(JSContext*, const char*);
JSValue js_symbol_ctor(JSContext*);
JSValue js_global_get(JSContext*, const char*);
char* js_tostringlen(JSContext*, size_t*, JSValue);
char* js_tostring(JSContext*, JSValue);
JSValue js_invoke(JSContext*, JSValue, const char*, int argc, JSValue argv[]);
JSValue js_promise_create(JSContext*, ResolveFunctions*);
JSValue js_promise_resolve(JSContext*, ResolveFunctions*, JSValue);
JSValue js_promise_reject(JSContext*, ResolveFunctions*, JSValue);
void js_promise_zero(ResolveFunctions*);
BOOL js_promise_pending(ResolveFunctions const*);
BOOL js_promise_done(ResolveFunctions const*);
JSValue js_global_get(JSContext*, const char*);
BOOL js_is_promise(JSContext*, JSValue);

static inline void
js_dump_string(const char* str, size_t len, size_t maxlen) {
  size_t i, n = 2;
  putchar('\'');
  for(i = 0; i < len; i++) {
    if(str[i] == '\n') {
      putchar('\\');
      putchar('n');
      n += 2;
    } else {
      putchar(str[i]);
      n++;
    }
    if(maxlen > 0 && n + 1 >= maxlen) {
      fputs("'...", stdout);
      return;
    }
  }
  putchar('\'');
}

static inline char*
js_to_string(JSContext* ctx, JSValueConst value) {
  const char* s;
  char* ret = 0;

  if((s = JS_ToCString(ctx, value))) {
    ret = js_strdup(ctx, s);
    JS_FreeCString(ctx, s);
  }
  return ret;
}
static inline BOOL
js_is_nullish(JSValueConst value) {
  return JS_IsNull(value) || JS_IsUndefined(value);
}

static inline void
js_buffer_free_default(JSRuntime* rt, void* opaque, void* ptr) {
  JSBuffer* buf = opaque;

  if(JS_IsString(buf->value))
    JS_FreeValueRT(rt, buf->value);
  else if(!JS_IsUndefined(buf->value))
    JS_FreeValueRT(rt, buf->value);
}

static inline const uint8_t*
js_buffer_begin(const JSBuffer* in) {
  return in->data;
}

static inline const uint8_t*
js_buffer_end(const JSBuffer* in) {
  return in->data + in->size;
}

#endif /* MINNET_JS_UTILS_H */
