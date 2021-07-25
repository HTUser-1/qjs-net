#include "minnet.h"
#include "minnet-websocket.h"
#include "minnet-response.h"
#include "buffer.h"
#include <cutils.h>

JSClassID minnet_response_class_id;
JSValue minnet_response_proto, minnet_response_ctor;

enum { RESPONSE_BUFFER, RESPONSE_JSON, RESPONSE_TEXT };
enum { RESPONSE_OK, RESPONSE_URL, RESPONSE_STATUS, RESPONSE_TYPE, RESPONSE_OFFSET };

static void
state_dump(const char* n, struct http_state const* b) {
  printf("%s\t{ times = %zx, budget = %zx }\n", n, b->times, b->budget);
  fflush(stdout);
}

void
response_dump(struct http_response const* res) {
  printf("{\n  url = %s, status = %d, ok = %d, type = %s, ", res->url, res->status, res->ok, res->type);
  state_dump("state", &res->state);
  buffer_dump("buffer", &res->body);
  printf(" }\n");

  fflush(stdout);
}

static void
response_zero(struct http_response* res) {
  memset(res, 0, sizeof(MinnetResponse));
  res->body = BUFFER_0();
}

static void
response_init(MinnetResponse* res, const char* url, int32_t status, BOOL ok, const char* type) {
  memset(res, 0, sizeof(MinnetResponse));

  res->status = status;
  res->ok = ok;
  res->url = url;
  res->type = type;
  res->body = BUFFER_0();
}

void
response_free(JSRuntime* rt, MinnetResponse* res) {
  js_free_rt(rt, (void*)res->url);
  res->url = 0;
  js_free_rt(rt, (void*)res->type);
  res->type = 0;

  buffer_free(&res->body, rt);
}

MinnetResponse*
response_new(JSContext* ctx, const char* url, int32_t status, BOOL ok, const char* type) {
  MinnetResponse* res;

  if((res = js_mallocz(ctx, sizeof(MinnetResponse))))
    response_init(res, url, status, ok, type);

  return res;
}

JSValue
minnet_response_object(JSContext* ctx, const char* url, int32_t status, BOOL ok, const char* type) {
  MinnetResponse* res;

  if((res = response_new(ctx, url, status, ok, type)))
    return minnet_response_wrap(ctx, res);
  return JS_NULL;
}

JSValue
minnet_response_wrap(JSContext* ctx, MinnetResponse* res) {
  JSValue ret = JS_NewObjectProtoClass(ctx, minnet_response_proto, minnet_response_class_id);
  if(JS_IsException(ret))
    return JS_EXCEPTION;

  JS_SetOpaque(ret, res);
  return ret;
}

static JSValue
minnet_response_buffer(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  MinnetResponse* res;

  if((res = JS_GetOpaque2(ctx, this_val, minnet_response_class_id))) {
    JSValue val = JS_NewArrayBuffer /*Copy*/ (ctx, buffer_START(&res->body), buffer_SIZE(&res->body), 0, 0, 0);
    return val;
  }

  return JS_EXCEPTION;
}

static JSValue
minnet_response_json(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  MinnetResponse* res;
  if((res = JS_GetOpaque2(ctx, this_val, minnet_response_class_id)))
    return JS_ParseJSON(ctx, buffer_START(&res->body), buffer_OFFSET(&res->body), res->url);

  return JS_EXCEPTION;
}

static JSValue
minnet_response_text(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  MinnetResponse* res;
  if((res = JS_GetOpaque2(ctx, this_val, minnet_response_class_id)))
    return JS_NewStringLen(ctx, (char*)buffer_START(&res->body), buffer_OFFSET(&res->body));

  return JS_EXCEPTION;
}

static JSValue
minnet_response_get(JSContext* ctx, JSValueConst this_val, int magic) {
  MinnetResponse* res;
  JSValue ret = JS_UNDEFINED;
  if(!(res = JS_GetOpaque2(ctx, this_val, minnet_response_class_id)))
    return JS_EXCEPTION;

  switch(magic) {
    case RESPONSE_STATUS: {
      ret = JS_NewInt32(ctx, res->status);
      break;
    }
    case RESPONSE_OK: {
      ret = JS_NewBool(ctx, res->ok);
      break;
    }
    case RESPONSE_URL: {
      ret = res->url ? JS_NewString(ctx, res->url) : JS_NULL;
      break;
    }
    case RESPONSE_TYPE: {
      ret = res->type ? JS_NewString(ctx, res->type) : JS_NULL;
      break;
    }
    case RESPONSE_OFFSET: {
      ret = JS_NewInt64(ctx, buffer_OFFSET(&res->body));
      break;
    }
  }

  return ret;
}

JSValue
minnet_response_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst argv[]) {
  JSValue proto, obj;
  MinnetResponse* resp;
  int i;

  if(!(resp = js_mallocz(ctx, sizeof(MinnetResponse))))
    return JS_ThrowOutOfMemory(ctx);

  /* using new_target to get the prototype is necessary when the
     class is extended. */
  proto = JS_GetPropertyStr(ctx, new_target, "prototype");
  if(JS_IsException(proto))
    proto = JS_DupValue(ctx, minnet_response_proto);

  obj = JS_NewObjectProtoClass(ctx, proto, minnet_response_class_id);
  JS_FreeValue(ctx, proto);
  if(JS_IsException(obj))
    goto fail;

  for(i = 0; i < argc; i++) {
    if(JS_IsString(argv[argc])) {
      const char* str = JS_ToCString(ctx, argv[argc]);
      if(resp->url)
        resp->type = str;
      else
        resp->url = str;
    } else if(JS_IsBool(argv[argc])) {
      resp->ok = JS_ToBool(ctx, argv[argc]);
    } else if(JS_IsNumber(argv[argc])) {
      int32_t s;
      if(!JS_ToInt32(ctx, &s, argv[argc]))
        resp->status = s;
    }
  }

  JS_SetOpaque(obj, resp);

  return obj;

fail:
  js_free(ctx, resp);
  JS_FreeValue(ctx, obj);
  return JS_EXCEPTION;
}

void
minnet_response_finalizer(JSRuntime* rt, JSValue val) {
  MinnetResponse* res = JS_GetOpaque(val, minnet_response_class_id);
  if(res) {
    if(res->body.start)
      js_free_rt(rt, res->body.start - LWS_PRE);

    js_free_rt(rt, res);
  }
}

JSClassDef minnet_response_class = {
    "MinnetResponse",
    .finalizer = minnet_response_finalizer,
};

const JSCFunctionListEntry minnet_response_proto_funcs[] = {
    JS_CFUNC_DEF("arrayBuffer", 0, minnet_response_buffer),
    JS_CFUNC_DEF("json", 0, minnet_response_json),
    JS_CFUNC_DEF("text", 0, minnet_response_text),
    JS_CGETSET_MAGIC_FLAGS_DEF("status", minnet_response_get, 0, RESPONSE_STATUS, JS_PROP_ENUMERABLE),
    JS_CGETSET_MAGIC_FLAGS_DEF("ok", minnet_response_get, 0, RESPONSE_OK, JS_PROP_ENUMERABLE),
    JS_CGETSET_MAGIC_FLAGS_DEF("url", minnet_response_get, 0, RESPONSE_URL, JS_PROP_ENUMERABLE),
    JS_CGETSET_MAGIC_FLAGS_DEF("type", minnet_response_get, 0, RESPONSE_TYPE, JS_PROP_ENUMERABLE),
    JS_CGETSET_MAGIC_DEF("offset", minnet_response_get, 0, RESPONSE_OFFSET),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "MinnetResponse", JS_PROP_CONFIGURABLE),
};

const size_t minnet_response_proto_funcs_size = countof(minnet_response_proto_funcs);
