#ifndef MINNET_HASH_H
#define MINNET_HASH_H

#include <quickjs.h>
#include <libwebsockets.h>

typedef struct hash {
  struct lws_genhash_ctx lws;
  int ref_count;
} MinnetHash;

JSValueConst minnet_hash_constructor(JSContext*, JSValueConst, int, JSValueConst argv[]);
JSValueConst minnet_hash_call(JSContext*, JSValueConst, JSValueConst, int argc, JSValueConst argv[], int flags);

extern THREAD_LOCAL JSValue minnet_hash_proto, minnet_hash_ctor;
extern THREAD_LOCAL JSClassID minnet_hash_class_id;
extern JSClassDef minnet_hash_class;
extern const JSCFunctionListEntry minnet_hash_proto_funcs[], minnet_hash_static_funcs[];
extern const size_t minnet_hash_proto_funcs_size, minnet_hash_static_funcs_size;

static inline MinnetHash*
minnet_hash_data(JSValueConst obj) {
  return JS_GetOpaque(obj, minnet_hash_class_id);
}

static inline MinnetHash*
minnet_hash_data2(JSContext* ctx, JSValueConst obj) {
  return JS_GetOpaque2(ctx, obj, minnet_hash_class_id);
}

#endif /* MINNET_HASH_H */
