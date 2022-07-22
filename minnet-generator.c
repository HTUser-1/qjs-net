#include "minnet-generator.h"
#include "jsutils.h"
#include <quickjs.h>
#include <assert.h>
#include <libwebsockets.h>

#define MIN(asynciterator_read, b) ((asynciterator_read) < (b) ? (asynciterator_read) : (b))

THREAD_LOCAL JSClassID minnet_generator_class_id;
THREAD_LOCAL JSValue minnet_generator_proto, minnet_generator_ctor;

void
generator_zero(struct generator* gen) {
  gen->buffer = BUFFER_0();
  asynciterator_zero(&gen->iterator);
  gen->ref_count = 0;
  gen->bytes_written = 0;
  gen->bytes_read = 0;
}

void
generator_free(struct generator** gen_p) {
  struct generator* gen;

  if((gen = *gen_p)) {
    if(--gen->ref_count == 0) {
      asynciterator_clear(&gen->iterator, JS_GetRuntime(gen->ctx));
      buffer_free(&gen->buffer, JS_GetRuntime(gen->ctx));
      js_free(gen->ctx, gen);

      *gen_p = 0;
    }
  }
}

struct generator*
generator_new(JSContext* ctx) {
  struct generator* gen;

  if((gen = js_malloc(ctx, sizeof(MinnetGenerator)))) {
    generator_zero(gen);
    gen->ctx = ctx;
    gen->ref_count = 1;
    // gen->iterator.ctx = ctx;
  }
  return gen;
}

JSValue
generator_next(MinnetGenerator* gen, JSContext* ctx) {
  JSValue ret = JS_UNDEFINED;

  ret = asynciterator_yield(&gen->iterator, ctx);

  if(buffer_HEAD(&gen->buffer)) {
    size_t len;
    int64_t bytes;
    JSValue value = buffer_toarraybuffer_size(&gen->buffer, &len, ctx);
    gen->buffer = BUFFER_0();

    if((bytes = asynciterator_push(&gen->iterator, value, ctx)) > 0)
      gen->bytes_read += bytes;
  }

  return ret;
}

ssize_t
generator_write(MinnetGenerator* gen, const void* data, size_t len) {
  ssize_t ret = -1;

  if(!list_empty(&gen->iterator.reads)) {
    JSValue buf = JS_NewArrayBufferCopy(gen->ctx, data, len);
    int64_t bytes = asynciterator_push(&gen->iterator, buf, gen->ctx);

    if((ret = bytes) > 0)
      gen->bytes_read += bytes;

  } else {
    ret = buffer_append(&gen->buffer, data, len, gen->ctx);
    if(ret > 0)
      gen->bytes_written += ret;
  }

  return ret;
}

static JSValue
minnet_generator_next(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic, void* opaque) {
  MinnetGenerator* gen = *(MinnetGenerator**)opaque;
  JSValue ret = JS_UNDEFINED;

  ret = generator_next(gen, ctx);

  return ret;
}

JSValue
minnet_generator_wrap(JSContext* ctx, MinnetGenerator** gen_p) {
  JSValue ret = JS_NewObject(ctx);

  if(!*gen_p)
    *gen_p = generator_new(ctx);
  else
    ++(*gen_p)->ref_count;

  JS_SetPropertyStr(ctx, ret, "next", JS_NewCClosure(ctx, minnet_generator_next, 0, 0, gen_p, (void*)&generator_free));

  return ret;
}
