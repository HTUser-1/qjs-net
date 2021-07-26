#ifndef MINNET_RESPONSE_H
#define MINNET_RESPONSE_H

#include <quickjs.h>
#include "buffer.h"

struct http_request;

/* class MinnetResponse */

typedef struct http_state {
  size_t times, budget, content_lines;
} MinnetHttpState;

typedef struct http_response {
  BOOL read_only;
  const char* url;
  int status;
  BOOL ok;
  const char* type;
  union {
    struct http_state state;
    JSValue iterator;
  };
  struct byte_buffer body;
} MinnetResponse;

void response_dump(struct http_response const*);
void response_zero(struct http_response*);
void response_init(struct http_response*, const char* url, int32_t status, BOOL ok, const char* type);
void response_free(JSRuntime*, struct http_response* res);
struct http_response* response_new(JSContext*, const char* url, int32_t status, BOOL ok, const char* type);
JSValue minnet_response_object(JSContext*, const char* url, int32_t status, BOOL ok, const char* type);
JSValue minnet_response_wrap(JSContext*, struct http_response* res);
JSValue minnet_response_constructor(JSContext*, JSValue new_target, int argc, JSValue argv[]);
void minnet_response_finalizer(JSRuntime*, JSValue val);

extern JSClassDef minnet_response_class;
extern const JSCFunctionListEntry minnet_response_proto_funcs[];
extern const size_t minnet_response_proto_funcs_size;
extern JSValue minnet_response_proto, minnet_response_ctor;
extern JSClassID minnet_response_class_id;

static inline MinnetResponse*
minnet_response_data(JSContext* ctx, JSValueConst obj) {
  return JS_GetOpaque2(ctx, obj, minnet_response_class_id);
}

#endif /* MINNET_RESPONSE_H */
