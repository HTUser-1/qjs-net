#include "minnet.h"
#include "server.h"
#include "client.h"
#include "response.h"
#include "websocket.h"
#include <assert.h>
#include <curl/curl.h>
#include <sys/time.h>

#ifdef JS_SHARED_LIBRARY
#define JS_INIT_MODULE js_init_module
#else
#define JS_INIT_MODULE js_init_module_minnet
#endif

JSClassID minnet_ws_class_id;

JSValue minnet_log, minnet_log_this;
JSContext* minnet_log_ctx = 0;
BOOL minnet_exception = FALSE;

static void
lws_log_callback(int level, const char* line) {
  if(minnet_log_ctx) {
    if(JS_VALUE_GET_TAG(minnet_log) == 0 && JS_VALUE_GET_TAG(minnet_log_this) == 0)
      get_console_log(minnet_log_ctx, &minnet_log_this, &minnet_log);

    if(JS_IsFunction(minnet_log_ctx, minnet_log)) {
      size_t len = strlen(line);
      JSValueConst argv[2] = {JS_NewString(minnet_log_ctx, "minnet"),
                              JS_NewStringLen(minnet_log_ctx, line, len > 0 && line[len - 1] == '\n' ? len - 1 : len)};
      JSValue ret = JS_Call(minnet_log_ctx, minnet_log, minnet_log_this, 2, argv);

      if(JS_IsException(ret))
        minnet_exception = TRUE;

      JS_FreeValue(minnet_log_ctx, argv[0]);
      JS_FreeValue(minnet_log_ctx, argv[1]);
      JS_FreeValue(minnet_log_ctx, ret);
    }
  }
}

const char*
lws_callback_name(int reason) {
  return ((const char* const[]){
      "LWS_CALLBACK_ESTABLISHED",
      "LWS_CALLBACK_CLIENT_CONNECTION_ERROR",
      "LWS_CALLBACK_CLIENT_FILTER_PRE_ESTABLISH",
      "LWS_CALLBACK_CLIENT_ESTABLISHED",
      "LWS_CALLBACK_CLOSED",
      "LWS_CALLBACK_CLOSED_HTTP",
      "LWS_CALLBACK_RECEIVE",
      "LWS_CALLBACK_RECEIVE_PONG",
      "LWS_CALLBACK_CLIENT_RECEIVE",
      "LWS_CALLBACK_CLIENT_RECEIVE_PONG",
      "LWS_CALLBACK_CLIENT_WRITEABLE",
      "LWS_CALLBACK_SERVER_WRITEABLE",
      "LWS_CALLBACK_HTTP",
      "LWS_CALLBACK_HTTP_BODY",
      "LWS_CALLBACK_HTTP_BODY_COMPLETION",
      "LWS_CALLBACK_HTTP_FILE_COMPLETION",
      "LWS_CALLBACK_HTTP_WRITEABLE",
      "LWS_CALLBACK_FILTER_NETWORK_CONNECTION",
      "LWS_CALLBACK_FILTER_HTTP_CONNECTION",
      "LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED",
      "LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION",
      "LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS",
      "LWS_CALLBACK_OPENSSL_LOAD_EXTRA_SERVER_VERIFY_CERTS",
      "LWS_CALLBACK_OPENSSL_PERFORM_CLIENT_CERT_VERIFICATION",
      "LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER",
      "LWS_CALLBACK_CONFIRM_EXTENSION_OKAY",
      "LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED",
      "LWS_CALLBACK_PROTOCOL_INIT",
      "LWS_CALLBACK_PROTOCOL_DESTROY",
      "LWS_CALLBACK_WSI_CREATE",
      "LWS_CALLBACK_WSI_DESTROY",
      "LWS_CALLBACK_GET_THREAD_ID",
      "LWS_CALLBACK_ADD_POLL_FD",
      "LWS_CALLBACK_DEL_POLL_FD",
      "LWS_CALLBACK_CHANGE_MODE_POLL_FD",
      "LWS_CALLBACK_LOCK_POLL",
      "LWS_CALLBACK_UNLOCK_POLL",
      "LWS_CALLBACK_OPENSSL_CONTEXT_REQUIRES_PRIVATE_KEY",
      "LWS_CALLBACK_WS_PEER_INITIATED_CLOSE",
      "LWS_CALLBACK_WS_EXT_DEFAULTS",
      "LWS_CALLBACK_CGI",
      "LWS_CALLBACK_CGI_TERMINATED",
      "LWS_CALLBACK_CGI_STDIN_DATA",
      "LWS_CALLBACK_CGI_STDIN_COMPLETED",
      "LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP",
      "LWS_CALLBACK_CLOSED_CLIENT_HTTP",
      "LWS_CALLBACK_RECEIVE_CLIENT_HTTP",
      "LWS_CALLBACK_COMPLETED_CLIENT_HTTP",
      "LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ",
      "LWS_CALLBACK_HTTP_BIND_PROTOCOL",
      "LWS_CALLBACK_HTTP_DROP_PROTOCOL",
      "LWS_CALLBACK_CHECK_ACCESS_RIGHTS",
      "LWS_CALLBACK_PROCESS_HTML",
      "LWS_CALLBACK_ADD_HEADERS",
      "LWS_CALLBACK_SESSION_INFO",
      "LWS_CALLBACK_GS_EVENT",
      "LWS_CALLBACK_HTTP_PMO",
      "LWS_CALLBACK_CLIENT_HTTP_WRITEABLE",
      "LWS_CALLBACK_OPENSSL_PERFORM_SERVER_CERT_VERIFICATION",
      "LWS_CALLBACK_RAW_RX",
      "LWS_CALLBACK_RAW_CLOSE",
      "LWS_CALLBACK_RAW_WRITEABLE",
      "LWS_CALLBACK_RAW_ADOPT",
      "LWS_CALLBACK_RAW_ADOPT_FILE",
      "LWS_CALLBACK_RAW_RX_FILE",
      "LWS_CALLBACK_RAW_WRITEABLE_FILE",
      "LWS_CALLBACK_RAW_CLOSE_FILE",
      "LWS_CALLBACK_SSL_INFO",
      0,
      "LWS_CALLBACK_CHILD_CLOSING",
      "LWS_CALLBACK_CGI_PROCESS_ATTACH",
      "LWS_CALLBACK_EVENT_WAIT_CANCELLED",
      "LWS_CALLBACK_VHOST_CERT_AGING",
      "LWS_CALLBACK_TIMER",
      "LWS_CALLBACK_VHOST_CERT_UPDATE",
      "LWS_CALLBACK_CLIENT_CLOSED",
      "LWS_CALLBACK_CLIENT_HTTP_DROP_PROTOCOL",
      "LWS_CALLBACK_WS_SERVER_BIND_PROTOCOL",
      "LWS_CALLBACK_WS_SERVER_DROP_PROTOCOL",
      "LWS_CALLBACK_WS_CLIENT_BIND_PROTOCOL",
      "LWS_CALLBACK_WS_CLIENT_DROP_PROTOCOL",
      "LWS_CALLBACK_RAW_SKT_BIND_PROTOCOL",
      "LWS_CALLBACK_RAW_SKT_DROP_PROTOCOL",
      "LWS_CALLBACK_RAW_FILE_BIND_PROTOCOL",
      "LWS_CALLBACK_RAW_FILE_DROP_PROTOCOL",
      "LWS_CALLBACK_CLIENT_HTTP_BIND_PROTOCOL",
      "LWS_CALLBACK_HTTP_CONFIRM_UPGRADE",
      0,
      0,
      "LWS_CALLBACK_RAW_PROXY_CLI_RX",
      "LWS_CALLBACK_RAW_PROXY_SRV_RX",
      "LWS_CALLBACK_RAW_PROXY_CLI_CLOSE",
      "LWS_CALLBACK_RAW_PROXY_SRV_CLOSE",
      "LWS_CALLBACK_RAW_PROXY_CLI_WRITEABLE",
      "LWS_CALLBACK_RAW_PROXY_SRV_WRITEABLE",
      "LWS_CALLBACK_RAW_PROXY_CLI_ADOPT",
      "LWS_CALLBACK_RAW_PROXY_SRV_ADOPT",
      "LWS_CALLBACK_RAW_PROXY_CLI_BIND_PROTOCOL",
      "LWS_CALLBACK_RAW_PROXY_SRV_BIND_PROTOCOL",
      "LWS_CALLBACK_RAW_PROXY_CLI_DROP_PROTOCOL",
      "LWS_CALLBACK_RAW_PROXY_SRV_DROP_PROTOCOL",
      "LWS_CALLBACK_RAW_CONNECTED",
      "LWS_CALLBACK_VERIFY_BASIC_AUTHORIZATION",
      "LWS_CALLBACK_WSI_TX_CREDIT_GET",
      "LWS_CALLBACK_CLIENT_HTTP_REDIRECT",
      "LWS_CALLBACK_CONNECTING",
  })[reason];
}

void
lws_print_unhandled(int reason) {
  printf("Unhandled LWS client event: %i %s\n", reason, lws_callback_name(reason));
}

JSValue
minnet_service_handler(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic, JSValue* func_data) {
  int32_t rw = 0;
  uint32_t calls = ++func_data[3].u.int32;
  struct lws_pollfd pfd;
  struct lws_pollargs args = *(struct lws_pollargs*)&JS_VALUE_GET_PTR(func_data[4]);
  struct lws_context* context = JS_VALUE_GET_PTR(func_data[2]);

  if(argc >= 1)
    JS_ToInt32(ctx, &rw, argv[0]);

  pfd.fd = JS_VALUE_GET_INT(func_data[0]);
  pfd.revents = rw ? POLLOUT : POLLIN;
  pfd.events = JS_VALUE_GET_INT(func_data[1]);

  if(pfd.events != (POLLIN | POLLOUT) || poll(&pfd, 1, 0) > 0)
    lws_service_fd(context, &pfd);

  /*if (calls <= 100)
    printf("minnet %s handler calls=%i fd=%d events=%d revents=%d pfd=[%d "
         "%d %d]\n",
         rw ? "writable" : "readable", calls, pfd.fd, pfd.events,
         pfd.revents, args.fd, args.events, args.prev_events);*/

  return JS_UNDEFINED;
}

JSValue
minnet_make_handler(JSContext* ctx, struct lws_pollargs* pfd, struct lws* wsi, int magic) {
  JSValue data[5] = {
      JS_MKVAL(JS_TAG_INT, pfd->fd),
      JS_MKVAL(JS_TAG_INT, pfd->events),
      JS_MKPTR(0, lws_get_context(wsi)),
      JS_MKVAL(JS_TAG_INT, 0),
      JS_MKPTR(0, *(void**)pfd),
  };

  return JS_NewCFunctionData(ctx, minnet_service_handler, 0, magic, countof(data), data);
}

JSValue
minnet_get_log(JSContext* ctx, JSValueConst this_val) {
  return JS_DupValue(ctx, minnet_log);
}
JSValue
minnet_set_log(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst argv[]) {
  JSValue ret = minnet_log;

  minnet_log_ctx = ctx;
  minnet_log = JS_DupValue(ctx, argv[0]);
  if(argc > 1) {
    JS_FreeValue(ctx, minnet_log_this);
    minnet_log_this = JS_DupValue(ctx, argv[1]);
  }
  return ret;
}

void
minnet_ws_sslcert(JSContext* ctx, struct lws_context_creation_info* info, JSValueConst options) {
  JSValue opt_ssl_cert = JS_GetPropertyStr(ctx, options, "sslCert");
  JSValue opt_ssl_private_key = JS_GetPropertyStr(ctx, options, "sslPrivateKey");
  JSValue opt_ssl_ca = JS_GetPropertyStr(ctx, options, "sslCA");

  if(JS_IsString(opt_ssl_cert))
    info->ssl_cert_filepath = JS_ToCString(ctx, opt_ssl_cert);
  if(JS_IsString(opt_ssl_private_key))
    info->ssl_private_key_filepath = JS_ToCString(ctx, opt_ssl_private_key);
  if(JS_IsString(opt_ssl_ca))
    info->client_ssl_ca_filepath = JS_ToCString(ctx, opt_ssl_ca);
}

static JSValue
minnet_ws_send(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  MinnetWebsocket* ws_obj;
  const char* msg;
  uint8_t* data;
  size_t len;
  int m, n;

  if(!(ws_obj = JS_GetOpaque2(ctx, this_val, minnet_ws_class_id)))
    return JS_EXCEPTION;

  if(JS_IsString(argv[0])) {
    msg = JS_ToCString(ctx, argv[0]);
    len = strlen(msg);
    uint8_t buffer[LWS_PRE + len];

    n = lws_snprintf((char*)&buffer[LWS_PRE], len + 1, "%s", msg);
    m = lws_write(ws_obj->lwsi, &buffer[LWS_PRE], len, LWS_WRITE_TEXT);
    if(m < n) {
      // Sending message failed
      return JS_EXCEPTION;
    }
    return JS_UNDEFINED;
  }

  data = JS_GetArrayBuffer(ctx, &len, argv[0]);
  if(data) {
    uint8_t buffer[LWS_PRE + len];
    memcpy(&buffer[LWS_PRE], data, len);

    m = lws_write(ws_obj->lwsi, &buffer[LWS_PRE], len, LWS_WRITE_BINARY);
    if(m < len) {
      // Sending data failed
      return JS_EXCEPTION;
    }
  }
  return JS_UNDEFINED;
}

static JSValue
minnet_ws_respond(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
  MinnetWebsocket* ws_obj;
  JSValue ret = JS_UNDEFINED;
  struct http_header* header;

  if(!(ws_obj = JS_GetOpaque2(ctx, this_val, minnet_ws_class_id)))
    return JS_EXCEPTION;

  if((header = ws_obj->header) == 0) {
    header = ws_obj->header = js_mallocz(ctx, sizeof(struct http_header));
  }

  switch(magic) {
    case 0: {
      const char* msg = 0;
      uint32_t status = 0;

      JS_ToUint32(ctx, &status, argv[0]);

      if(argc >= 2)
        msg = JS_ToCString(ctx, argv[1]);

      lws_return_http_status(ws_obj->lwsi, status, msg);
      if(msg)
        JS_FreeCString(ctx, msg);
      break;
    }
    case 1: {

      const char* msg = 0;
      size_t len = 0;
      uint32_t status = 0;

      JS_ToUint32(ctx, &status, argv[0]);

      if(argc >= 2)
        msg = JS_ToCStringLen(ctx, &len, argv[1]);

      if(lws_http_redirect(ws_obj->lwsi, status, (unsigned char*)msg, len, &header->pos, header->end) < 0)
        ret = JS_NewInt32(ctx, -1);
      if(msg)
        JS_FreeCString(ctx, msg);
      break;
    }
    case 2: {
      size_t namelen;
      const char* namestr = JS_ToCStringLen(ctx, &namelen, argv[0]);
      char* name = js_malloc(ctx, namelen + 2);
      size_t len;
      const char* value = JS_ToCStringLen(ctx, &len, argv[1]);

      memcpy(name, namestr, namelen);
      name[namelen] = ':';
      name[namelen + 1] = '\0';

      if(lws_add_http_header_by_name(ws_obj->lwsi, name, value, len, &header->pos, header->end) < 0)
        ret = JS_NewInt32(ctx, -1);

      js_free(ctx, name);
      JS_FreeCString(ctx, namestr);
      JS_FreeCString(ctx, value);
      break;
    }
  }

  return ret;
}

static JSValue
minnet_ws_ping(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  MinnetWebsocket* ws_obj;
  uint8_t* data;
  size_t len;

  if(!(ws_obj = JS_GetOpaque2(ctx, this_val, minnet_ws_class_id)))
    return JS_EXCEPTION;

  data = JS_GetArrayBuffer(ctx, &len, argv[0]);
  if(data) {
    uint8_t buffer[len + LWS_PRE];
    memcpy(&buffer[LWS_PRE], data, len);

    int m = lws_write(ws_obj->lwsi, &buffer[LWS_PRE], len, LWS_WRITE_PING);
    if(m < len) {
      // Sending ping failed
      return JS_EXCEPTION;
    }
  } else {
    uint8_t buffer[LWS_PRE];
    lws_write(ws_obj->lwsi, &buffer[LWS_PRE], 0, LWS_WRITE_PING);
  }
  return JS_UNDEFINED;
}

static JSValue
minnet_ws_pong(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  MinnetWebsocket* ws_obj;
  uint8_t* data;
  size_t len;

  if(!(ws_obj = JS_GetOpaque2(ctx, this_val, minnet_ws_class_id)))
    return JS_EXCEPTION;

  data = JS_GetArrayBuffer(ctx, &len, argv[0]);
  if(data) {
    uint8_t buffer[len + LWS_PRE];
    memcpy(&buffer[LWS_PRE], data, len);

    int m = lws_write(ws_obj->lwsi, &buffer[LWS_PRE], len, LWS_WRITE_PONG);
    if(m < len) {
      // Sending pong failed
      return JS_EXCEPTION;
    }
  } else {
    uint8_t buffer[LWS_PRE];
    lws_write(ws_obj->lwsi, &buffer[LWS_PRE], 0, LWS_WRITE_PONG);
  }
  return JS_UNDEFINED;
}

static JSValue
minnet_ws_close(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  MinnetWebsocket* ws_obj;
  const char* reason = 0;
  size_t rlen = 0;

  if(!(ws_obj = JS_GetOpaque2(ctx, this_val, minnet_ws_class_id)))
    return JS_EXCEPTION;

  if(ws_obj->lwsi) {
    int optind = 0;
    uint32_t status = LWS_CLOSE_STATUS_NORMAL;

    if(optind < argc && JS_IsNumber(argv[optind]))
      JS_ToInt32(ctx, &status, argv[optind++]);

    if(optind < argc) {
      reason = JS_ToCStringLen(ctx, &rlen, argv[optind++]);
      if(rlen > 124)
        rlen = 124;
    }

    if(reason)
      lws_close_reason(ws_obj->lwsi, status, reason, rlen);

    lws_close_free_wsi(ws_obj->lwsi, status, "minnet_ws_close");

    ws_obj->lwsi = 0;
    return JS_TRUE;
  }

  return JS_FALSE;
}

static JSValue
minnet_ws_get(JSContext* ctx, JSValueConst this_val, int magic) {
  MinnetWebsocket* ws_obj;
  JSValue ret = JS_UNDEFINED;
  if(!(ws_obj = JS_GetOpaque2(ctx, this_val, minnet_ws_class_id)))
    return JS_EXCEPTION;

  switch(magic) {
    case 0: {
      ret = JS_NewInt32(ctx, lws_get_socket_fd(ws_obj->lwsi));
      break;
    }
    case 1: {
      char address[1024];
      lws_get_peer_simple(ws_obj->lwsi, address, sizeof(address));

      ret = JS_NewString(ctx, address);
      break;
    }
    case 2:
    case 3: {
      struct sockaddr_in addr;
      socklen_t addrlen = sizeof(addr);
      int fd = lws_get_socket_fd(ws_obj->lwsi);

      if(getpeername(fd, &addr, &addrlen) != -1) {
        ret = JS_NewInt32(ctx, magic == 2 ? addr.sin_family : addr.sin_port);
      }
      break;
    }
    case 4: {
      struct sockaddr_in addr;
      socklen_t addrlen = sizeof(addr);
      int fd = lws_get_socket_fd(ws_obj->lwsi);

      if(getpeername(fd, &addr, &addrlen) != -1) {
        ret = JS_NewArrayBufferCopy(ctx, &addr, addrlen);
      }
      break;
    }
  }
  return ret;
}

static void
minnet_ws_finalizer(JSRuntime* rt, JSValue val) {
  MinnetWebsocket* ws_obj = JS_GetOpaque(val, minnet_ws_class_id);
  if(ws_obj) {
    if(--ws_obj->ref_count == 0)
      js_free_rt(rt, ws_obj);
  }
}

#include "server.h"
#include "client.h"

static const JSCFunctionListEntry minnet_funcs[] = {
    JS_CFUNC_DEF("server", 1, minnet_ws_server),
    JS_CFUNC_DEF("client", 1, minnet_ws_client),
    JS_CFUNC_DEF("fetch", 1, minnet_fetch),
    JS_CFUNC_DEF("setLog", 1, minnet_set_log),
};

static const JSCFunctionListEntry minnet_ws_proto_funcs[] = {
    JS_CFUNC_DEF("send", 1, minnet_ws_send),
    JS_CFUNC_MAGIC_DEF("respond", 1, minnet_ws_respond, 0),
    JS_CFUNC_MAGIC_DEF("redirect", 2, minnet_ws_respond, 1),
    JS_CFUNC_MAGIC_DEF("header", 2, minnet_ws_respond, 2),
    JS_CFUNC_DEF("ping", 1, minnet_ws_ping),
    JS_CFUNC_DEF("pong", 1, minnet_ws_pong),
    JS_CFUNC_DEF("close", 1, minnet_ws_close),
    JS_CGETSET_MAGIC_FLAGS_DEF("fd", minnet_ws_get, 0, 0, JS_PROP_ENUMERABLE),
    JS_CGETSET_MAGIC_FLAGS_DEF("address", minnet_ws_get, 0, 1, JS_PROP_ENUMERABLE),
    JS_CGETSET_MAGIC_FLAGS_DEF("family", minnet_ws_get, 0, 2, JS_PROP_ENUMERABLE),
    JS_CGETSET_MAGIC_FLAGS_DEF("port", minnet_ws_get, 0, 3, JS_PROP_ENUMERABLE),
    JS_CGETSET_MAGIC_FLAGS_DEF("peer", minnet_ws_get, 0, 4, 0),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "MinnetWebSocket", JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("CLOSE_STATUS_NORMAL", LWS_CLOSE_STATUS_NORMAL, 0),
    JS_PROP_INT32_DEF("CLOSE_STATUS_GOINGAWAY", LWS_CLOSE_STATUS_GOINGAWAY, 0),
    JS_PROP_INT32_DEF("CLOSE_STATUS_PROTOCOL_ERR", LWS_CLOSE_STATUS_PROTOCOL_ERR, 0),
    JS_PROP_INT32_DEF("CLOSE_STATUS_UNACCEPTABLE_OPCODE", LWS_CLOSE_STATUS_UNACCEPTABLE_OPCODE, 0),
    JS_PROP_INT32_DEF("CLOSE_STATUS_RESERVED", LWS_CLOSE_STATUS_RESERVED, 0),
    JS_PROP_INT32_DEF("CLOSE_STATUS_NO_STATUS", LWS_CLOSE_STATUS_NO_STATUS, 0),
    JS_PROP_INT32_DEF("CLOSE_STATUS_ABNORMAL_CLOSE", LWS_CLOSE_STATUS_ABNORMAL_CLOSE, 0),
    JS_PROP_INT32_DEF("CLOSE_STATUS_INVALID_PAYLOAD", LWS_CLOSE_STATUS_INVALID_PAYLOAD, 0),
    JS_PROP_INT32_DEF("CLOSE_STATUS_POLICY_VIOLATION", LWS_CLOSE_STATUS_POLICY_VIOLATION, 0),
    JS_PROP_INT32_DEF("CLOSE_STATUS_MESSAGE_TOO_LARGE", LWS_CLOSE_STATUS_MESSAGE_TOO_LARGE, 0),
    JS_PROP_INT32_DEF("CLOSE_STATUS_EXTENSION_REQUIRED", LWS_CLOSE_STATUS_EXTENSION_REQUIRED, 0),
    JS_PROP_INT32_DEF("CLOSE_STATUS_UNEXPECTED_CONDITION", LWS_CLOSE_STATUS_UNEXPECTED_CONDITION, 0),
    JS_PROP_INT32_DEF("CLOSE_STATUS_TLS_FAILURE", LWS_CLOSE_STATUS_TLS_FAILURE, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_CONTINUE", HTTP_STATUS_CONTINUE, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_OK", HTTP_STATUS_OK, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_NO_CONTENT", HTTP_STATUS_NO_CONTENT, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_PARTIAL_CONTENT", HTTP_STATUS_PARTIAL_CONTENT, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_MOVED_PERMANENTLY", HTTP_STATUS_MOVED_PERMANENTLY, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_FOUND", HTTP_STATUS_FOUND, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_SEE_OTHER", HTTP_STATUS_SEE_OTHER, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_NOT_MODIFIED", HTTP_STATUS_NOT_MODIFIED, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_BAD_REQUEST", HTTP_STATUS_BAD_REQUEST, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_UNAUTHORIZED", HTTP_STATUS_UNAUTHORIZED, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_PAYMENT_REQUIRED", HTTP_STATUS_PAYMENT_REQUIRED, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_FORBIDDEN", HTTP_STATUS_FORBIDDEN, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_NOT_FOUND", HTTP_STATUS_NOT_FOUND, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_METHOD_NOT_ALLOWED", HTTP_STATUS_METHOD_NOT_ALLOWED, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_NOT_ACCEPTABLE", HTTP_STATUS_NOT_ACCEPTABLE, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_PROXY_AUTH_REQUIRED", HTTP_STATUS_PROXY_AUTH_REQUIRED, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_REQUEST_TIMEOUT", HTTP_STATUS_REQUEST_TIMEOUT, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_CONFLICT", HTTP_STATUS_CONFLICT, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_GONE", HTTP_STATUS_GONE, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_LENGTH_REQUIRED", HTTP_STATUS_LENGTH_REQUIRED, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_PRECONDITION_FAILED", HTTP_STATUS_PRECONDITION_FAILED, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_REQ_ENTITY_TOO_LARGE", HTTP_STATUS_REQ_ENTITY_TOO_LARGE, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_REQ_URI_TOO_LONG", HTTP_STATUS_REQ_URI_TOO_LONG, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE", HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_REQ_RANGE_NOT_SATISFIABLE", HTTP_STATUS_REQ_RANGE_NOT_SATISFIABLE, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_EXPECTATION_FAILED", HTTP_STATUS_EXPECTATION_FAILED, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_INTERNAL_SERVER_ERROR", HTTP_STATUS_INTERNAL_SERVER_ERROR, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_NOT_IMPLEMENTED", HTTP_STATUS_NOT_IMPLEMENTED, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_BAD_GATEWAY", HTTP_STATUS_BAD_GATEWAY, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_SERVICE_UNAVAILABLE", HTTP_STATUS_SERVICE_UNAVAILABLE, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_GATEWAY_TIMEOUT", HTTP_STATUS_GATEWAY_TIMEOUT, 0),
    JS_PROP_INT32_DEF("HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED", HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED, 0),
};

static int
js_minnet_init(JSContext* ctx, JSModuleDef* m) {
  return JS_SetModuleExportList(ctx, m, minnet_funcs, countof(minnet_funcs));
}

__attribute__((visibility("default"))) JSModuleDef*
JS_INIT_MODULE(JSContext* ctx, const char* module_name) {
  JSModuleDef* m;
  m = JS_NewCModule(ctx, module_name, js_minnet_init);
  if(!m)
    return NULL;
  JS_AddModuleExportList(ctx, m, minnet_funcs, countof(minnet_funcs));

  // Add class Response
  JS_NewClassID(&minnet_response_class_id);
  JS_NewClass(JS_GetRuntime(ctx), minnet_response_class_id, &minnet_response_class);
  JSValue response_proto = JS_NewObject(ctx);
  JS_SetPropertyFunctionList(ctx, response_proto, minnet_response_proto_funcs, countof(minnet_response_proto_funcs));
  JS_SetClassProto(ctx, minnet_response_class_id, response_proto);

  // Add class WebSocket
  JS_NewClassID(&minnet_ws_class_id);
  JS_NewClass(JS_GetRuntime(ctx), minnet_ws_class_id, &minnet_ws_class);
  JSValue websocket_proto = JS_NewObject(ctx);
  JS_SetPropertyFunctionList(ctx, websocket_proto, minnet_ws_proto_funcs, countof(minnet_ws_proto_funcs));
  JS_SetClassProto(ctx, minnet_ws_class_id, websocket_proto);

  minnet_log_ctx = ctx;

  lws_set_log_level(LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE, lws_log_callback);

  return m;
}

JSValue
minnet_fetch(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  CURL* curl;
  CURLcode curlRes;
  const char* url;
  FILE* fi;
  MinnetResponse* res;
  uint8_t* buffer;
  long bufSize;
  long status;
  char* type;
  const char* body_str = NULL;
  struct curl_slist* headerlist = NULL;
  char* buf = calloc(1, 1);
  size_t bufsize = 1;

  JSValue resObj = JS_NewObjectClass(ctx, minnet_response_class_id);
  if(JS_IsException(resObj))
    return JS_EXCEPTION;

  res = js_mallocz(ctx, sizeof(*res));

  if(!res) {
    JS_FreeValue(ctx, resObj);
    return JS_EXCEPTION;
  }

  if(!JS_IsString(argv[0]))
    return JS_EXCEPTION;

  res->url = argv[0];
  url = JS_ToCString(ctx, argv[0]);

  if(argc > 1 && JS_IsObject(argv[1])) {
    JSValue method, body, headers;
    const char* method_str;
    method = JS_GetPropertyStr(ctx, argv[1], "method");
    body = JS_GetPropertyStr(ctx, argv[1], "body");
    headers = JS_GetPropertyStr(ctx, argv[1], "headers");

    if(!JS_IsUndefined(headers)) {
      JSValue global_obj, object_ctor, /* object_proto, */ keys, names, length;
      int i;
      int32_t len;

      global_obj = JS_GetGlobalObject(ctx);
      object_ctor = JS_GetPropertyStr(ctx, global_obj, "Object");
      keys = JS_GetPropertyStr(ctx, object_ctor, "keys");

      names = JS_Call(ctx, keys, object_ctor, 1, (JSValueConst*)&headers);
      length = JS_GetPropertyStr(ctx, names, "length");

      JS_ToInt32(ctx, &len, length);

      for(i = 0; i < len; i++) {
        char* h;
        JSValue key, value;
        const char *key_str, *value_str;
        size_t key_len, value_len;
        key = JS_GetPropertyUint32(ctx, names, i);
        key_str = JS_ToCString(ctx, key);
        key_len = strlen(key_str);

        value = JS_GetPropertyStr(ctx, headers, key_str);
        value_str = JS_ToCString(ctx, value);
        value_len = strlen(value_str);

        buf = realloc(buf, bufsize + key_len + 2 + value_len + 2 + 1);
        h = &buf[bufsize];

        strcpy(&buf[bufsize], key_str);
        bufsize += key_len;
        strcpy(&buf[bufsize], ": ");
        bufsize += 2;
        strcpy(&buf[bufsize], value_str);
        bufsize += value_len;
        strcpy(&buf[bufsize], "\0\n");
        bufsize += 2;

        JS_FreeCString(ctx, key_str);
        JS_FreeCString(ctx, value_str);

        headerlist = curl_slist_append(headerlist, h);
      }

      JS_FreeValue(ctx, global_obj);
      JS_FreeValue(ctx, object_ctor);
      // JS_FreeValue(ctx, object_proto);
      JS_FreeValue(ctx, keys);
      JS_FreeValue(ctx, names);
      JS_FreeValue(ctx, length);
    }

    method_str = JS_ToCString(ctx, method);

    if(!JS_IsUndefined(body) || !strcasecmp(method_str, "post")) {
      body_str = JS_ToCString(ctx, body);
    }

    JS_FreeCString(ctx, method_str);

    JS_FreeValue(ctx, method);
    JS_FreeValue(ctx, body);
    JS_FreeValue(ctx, headers);
  }

  curl = curl_easy_init();
  if(!curl)
    return JS_EXCEPTION;

  fi = tmpfile();

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "minimal-network-quickjs");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, fi);

  if(body_str)
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str);

  curlRes = curl_easy_perform(curl);
  if(curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status) == CURLE_OK)
    res->status = JS_NewInt32(ctx, (int32_t)status);

  if(curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &type) == CURLE_OK)
    res->type = type ? JS_NewString(ctx, type) : JS_NULL;

  res->ok = JS_FALSE;

  if(curlRes != CURLE_OK) {
    fprintf(stderr, "CURL failed: %s\n", curl_easy_strerror(curlRes));
    goto finish;
  }

  bufSize = ftell(fi);
  rewind(fi);

  buffer = calloc(1, bufSize + 1);
  if(!buffer) {
    fclose(fi), fputs("memory alloc fails", stderr);
    goto finish;
  }

  /* copy the file into the buffer */
  if(1 != fread(buffer, bufSize, 1, fi)) {
    fclose(fi), free(buffer), fputs("entire read fails", stderr);
    goto finish;
  }

  fclose(fi);

  res->ok = JS_TRUE;
  res->buffer = buffer;
  res->size = bufSize;

finish:
  curl_slist_free_all(headerlist);
  free(buf);
  if(body_str)
    JS_FreeCString(ctx, body_str);

  curl_easy_cleanup(curl);
  JS_SetOpaque(resObj, res);

  return resObj;
}
