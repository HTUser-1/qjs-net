#ifndef QJSNET_LIB_CONTEXT_H
#define QJSNET_LIB_CONTEXT_H

#include <quickjs.h>
#include <cutils.h>
#include <libwebsockets.h>

struct context {
  int ref_count;
  JSContext* js;
  struct lws_context* lws;
  struct lws_context_creation_info info;
  BOOL exception;
  JSValue error;
  JSValue crt, key, ca;
  struct TimerClosure* timer;
};

JSValue context_exception(struct context*, JSValueConst retval);
void context_clear(struct context*);

#endif /* QJSNET_LIB_CONTEXT_H */
