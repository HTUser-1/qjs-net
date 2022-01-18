#include "minnet.h"
#include "minnet-client.h"
#include "minnet-websocket.h"
#include <quickjs-libc.h>

THREAD_LOCAL JSValue minnet_client_proto, minnet_client_ctor;
THREAD_LOCAL JSClassID minnet_client_class_id;

static int client_callback(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len);
static int http_callback(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len);
static int raw_callback(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len);

/*THREAD_LOCAL struct lws_context* minnet_client_lws = 0;
THREAD_LOCAL JSContext* minnet_client_ctx = 0;*/

static const struct lws_protocols client_protocols[] = {
    {"ws", client_callback, sizeof(MinnetClient), MINNET_BUFFER_SIZE, 0, 0, 0},
    {"http", client_callback, sizeof(MinnetClient), MINNET_BUFFER_SIZE, 0, 0, 0},
    {"raw", client_callback, sizeof(MinnetClient), MINNET_BUFFER_SIZE, 0, 0, 0},
    {0},
};

static JSValue
close_status(JSContext* ctx, const char* in, size_t len) {
  if(len >= 2)
    return JS_NewInt32(ctx, ((uint8_t*)in)[0] << 8 | ((uint8_t*)in)[1]);
  return JS_UNDEFINED;
}

static JSValue
close_reason(JSContext* ctx, const char* in, size_t len) {
  if(len > 0)
    return JS_NewStringLen(ctx, in, len);
  return JS_UNDEFINED;
}

static void
sslcert_client(JSContext* ctx, struct lws_context_creation_info* info, JSValueConst options) {
  JSValue opt_ssl_cert = JS_GetPropertyStr(ctx, options, "sslCert");
  JSValue opt_ssl_private_key = JS_GetPropertyStr(ctx, options, "sslPrivateKey");
  JSValue opt_ssl_ca = JS_GetPropertyStr(ctx, options, "sslCA");

  if(JS_IsString(opt_ssl_cert))
    info->client_ssl_cert_filepath = JS_ToCString(ctx, opt_ssl_cert);
  if(JS_IsString(opt_ssl_private_key))
    info->client_ssl_private_key_filepath = JS_ToCString(ctx, opt_ssl_private_key);
  if(JS_IsString(opt_ssl_ca))
    info->client_ssl_ca_filepath = JS_ToCString(ctx, opt_ssl_ca);
}

static int
connect_client(struct lws_context* context, MinnetURL* url, struct lws** p_wsi) {
  struct lws_client_connect_info i;

  memset(&i, 0, sizeof(i));

  if(url_is_raw(url)) {
    i.method = "RAW";
    i.local_protocol_name = "raw";
  } else if(!strncmp(url->protocol, "http", 4)) {
    i.alpn = "http/1.1";
    i.method = "GET";
    i.protocol = "http";
  } else {
    i.protocol = "ws";
  }

  i.context = context;
  i.port = url->port;
  i.address = url->host;

  if(url_is_tls(&url)) {
    i.ssl_connection = LCCSCF_USE_SSL | LCCSCF_H2_QUIRK_OVERFLOWS_TXCR | LCCSCF_H2_QUIRK_NGHTTP2_END_STREAM;
    i.ssl_connection |= LCCSCF_ALLOW_SELFSIGNED;
    i.ssl_connection |= LCCSCF_ALLOW_INSECURE;
  }
  i.path = url->location;
  i.host = i.address;
  i.origin = i.address;
  i.pwsi = p_wsi;

  url->host = 0;
  url->location = 0;
  // lwsl_user("connect_client { protocol: %s, local_protocol_name: %s, host: %s, path: %s, origin: %s }\n", i.protocol, i.local_protocol_name, i.host, i.path, i.origin);

  return !lws_client_connect_via_info(&i);
}

JSValue
minnet_ws_client(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  int n = 0;
  JSValue ret = JS_NULL;
  BOOL raw = FALSE, block = TRUE;
  JSValue options = argv[0];
  MinnetClient* client;
  struct lws* wsi = 0;
  struct lws_context_creation_info* info;
  MinnetURL url;

  if(!(client = js_mallocz(ctx, sizeof(MinnetClient))))
    return JS_ThrowOutOfMemory(ctx);

  client->url = url;
  client->ctx = ctx;
  client->ws_obj = JS_NULL;
  /*  client->req_obj = JS_NULL;
    client->resp_obj = JS_NULL;*/

  info = &client->info;

  SETLOG(LLL_INFO)

  info->options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
  info->options |= LWS_SERVER_OPTION_H2_JUST_FIX_WINDOW_UPDATE_OVERFLOW;
  info->port = CONTEXT_PORT_NO_LISTEN;
  info->protocols = client_protocols;
  info->user = client;

  if(argc >= 2 && JS_IsObject(argv[1])) {
    const char* urlStr = JS_ToCString(ctx, argv[0]);
    client->url = url_parse(ctx, urlStr);
    JS_FreeCString(ctx, urlStr);
    options = argv[1];
  }

  {
    const char *protocol = 0, *host = 0, *path = 0;
    int32_t port = -1, ssl = -1;

    GETOPT(options, protocol);
    GETOPT(options, host);
    GETOPT(options, path);
    GETOPT(options, port);
    GETOPT(options, ssl);
    GETOPT(options, raw);
    GETOPT(options, block);

    if(JS_IsString(opt_protocol))
      protocol = JS_ToCString(ctx, opt_protocol);

    if(JS_IsString(opt_host))
      host = JS_ToCString(ctx, opt_host);

    if(JS_IsString(opt_path))
      path = JS_ToCString(ctx, opt_path);

    if(JS_IsNumber(opt_port))
      JS_ToInt32(ctx, &port, opt_port);

    ssl = JS_ToBool(ctx, opt_ssl);
    raw = JS_ToBool(ctx, opt_raw);

    if(!JS_IsUndefined(opt_block) && !JS_IsException(opt_block))
      block = JS_ToBool(ctx, opt_block);

    FREEOPT(protocol);
    FREEOPT(host);
    FREEOPT(path);
    FREEOPT(port);
    FREEOPT(ssl);
    FREEOPT(raw);
    FREEOPT(block);

    url = url_init(ctx, protocol ? protocol : ssl ? "wss" : "ws", host, port, path);

    if(protocol)
      JS_FreeCString(ctx, protocol);
    if(host)
      JS_FreeCString(ctx, host);
    if(path)
      JS_FreeCString(ctx, path);
  }

  sslcert_client(ctx, info, options);

  if(!(client->lws = lws_create_context(info))) {
    lwsl_err("minnet-client: libwebsockets init failed\n");
    return JS_ThrowInternalError(ctx, "minnet-client: libwebsockets init failed");
  }

  OPTIONS_CB(options, "onPong", client->cb_pong);
  OPTIONS_CB(options, "onError", client->cb_error);
  OPTIONS_CB(options, "onClose", client->cb_close);
  OPTIONS_CB(options, "onConnect", client->cb_connect);
  OPTIONS_CB(options, "onMessage", client->cb_message);
  OPTIONS_CB(options, "onFd", client->cb_fd);

  connect_client(client->lws, &url, &wsi);

  client->ws_obj = minnet_ws_object(ctx, wsi);

  printf("minnet_ws_client wsi=%p ws_obj=%p client=%p\n", wsi, JS_VALUE_GET_OBJ(client->ws_obj), client);

  minnet_exception = FALSE;

  if(!block) {
    ret = minnet_client_wrap(ctx, client);
    return;
  }

  if(block) {
    while(n >= 0) {
      if(minnet_exception) {
        ret = JS_EXCEPTION;
        break;
      }
      js_std_loop(ctx);

      // lws_service(minnet_client_lws, 500);
    }
    if(wsi) {
      JSValue opt_binary = JS_GetPropertyStr(ctx, options, "binary");
      if(JS_IsBool(opt_binary)) {
        MinnetWebsocket* ws = minnet_ws_data2(ctx, ret);
        ws->binary = JS_ToBool(ctx, opt_binary);
      }
    } else {
      ret = JS_ThrowInternalError(ctx, "No websocket!");
    }
    url_free(ctx, &url);
  }

  return ret;
}

static int
client_callback(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len) {
  MinnetClient* client;
  JSContext* ctx = 0;

  if((client = lws_context_user(lws_get_context(wsi))))
    ctx = client->ctx;

  if(reason >= LWS_CALLBACK_ADD_POLL_FD && reason <= LWS_CALLBACK_UNLOCK_POLL) {
    return fd_callback(wsi, reason, &client->cb_fd, in);
  }

  lwsl_user("client_callback " FG("%d") "%-25s" NC " wsi=%p ws_obj=%p client=%p is_ssl=%i len=%zu in='%.*s'\n",
            22 + (reason * 2),
            lws_callback_name(reason) + 13,
            wsi,
            client ? JS_VALUE_GET_OBJ(client->ws_obj) : 0,
            client,
            lws_is_ssl(wsi),
            len,
            (int)MIN(len, 40),
            (char*)in);

  /*  if((client = lws_get_opaque_user_data(wsi)))
      ctx = client->ctx;*/

  switch(reason) {
    case LWS_CALLBACK_PROTOCOL_INIT: {

      return 0;
    }
    case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS: {

      return 0;
    }
    case LWS_CALLBACK_CLIENT_HTTP_BIND_PROTOCOL:
    case LWS_CALLBACK_WS_CLIENT_BIND_PROTOCOL:
    case LWS_CALLBACK_RAW_SKT_BIND_PROTOCOL: {
      return 0;
    }
    case LWS_CALLBACK_CLIENT_HTTP_DROP_PROTOCOL:
    case LWS_CALLBACK_WS_CLIENT_DROP_PROTOCOL:
    case LWS_CALLBACK_RAW_SKT_DROP_PROTOCOL: {
      // buffer_free(&client->body, JS_GetRuntime(ctx));

      /* JS_FreeValue(ctx, client->ws_obj);
       client->ws_obj = JS_NULL;*/
      return 0;
    }

    case LWS_CALLBACK_CLIENT_FILTER_PRE_ESTABLISH:
    case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
    case LWS_CALLBACK_CONNECTING: {
      return 0;
    }
    case LWS_CALLBACK_WSI_CREATE:
    case LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED: {
      return 0;
    }

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
    case LWS_CALLBACK_CLOSED_CLIENT_HTTP:
    case LWS_CALLBACK_CLIENT_CLOSED:
    case LWS_CALLBACK_RAW_CLOSE:
    case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE: {
      struct wsi_opaque_user_data* opaque = lws_get_opaque_user_data(wsi);
      const char* errstr = in;
      int err = opaque ? opaque->error : 0;
      BOOL error = err || errstr;
      MinnetCallback* cb = error ? &client->cb_error : &client->cb_close;

      if(cb->ctx || (cb->ctx = ctx)) {
        JSValueConst cb_argv[] = {
            client->ws_obj,
            //  errstr ? JS_NewString(ctx, errstr) : JS_NewInt32(ctx, err),
            error ? JS_NewString(cb->ctx, errstr ? errstr : strerror(err)) : JS_NewInt32(ctx, err),
        };
        minnet_emit(cb, error ? 2 : 1, cb_argv);
        JS_FreeValue(ctx, cb_argv[1]);
      }
      break;
    }
    case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:
    case LWS_CALLBACK_ESTABLISHED:
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
    case LWS_CALLBACK_RAW_CONNECTED: {
      if(!client->connected) {
        client->connected = TRUE;
        buffer_alloc(&client->body, MINNET_BUFFER_SIZE, ctx);

        if(client->cb_connect.ctx || (client->cb_connect.ctx = ctx)) {
          minnet_emit(&client->cb_connect, 1, &client->ws_obj);
        }
      }
      break;
    }
    case LWS_CALLBACK_HTTP_WRITEABLE:
    case LWS_CALLBACK_CLIENT_HTTP_WRITEABLE:
    case LWS_CALLBACK_CLIENT_WRITEABLE:
    case LWS_CALLBACK_RAW_WRITEABLE: {
      // lws_callback_on_writable(wsi);
      break;
    }

    case LWS_CALLBACK_RECEIVE_CLIENT_HTTP: {
      /*      int ret, n = buffer_AVAIL(&client->body);
            char* buf = client->body.write; */

      char buffer[MINNET_BUFFER_SIZE + LWS_PRE];
      char* buf = buffer + LWS_PRE;
      int ret, n = sizeof(buffer) - LWS_PRE;

      if((ret = lws_http_client_read(wsi, &buf, &n)))
        return -1;

      break;
    }

    case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ: {
      printf("RECEIVE_CLIENT_HTTP_READ in=%.*s len=%i\n", (int)MIN(len, 100), in, len);
      //  buffer_append(&client->body, in, len, ctx);
      return 0;
    }

    case LWS_CALLBACK_COMPLETED_CLIENT_HTTP: {
      in = client->body.read;
      len = buffer_REMAIN(&client->body);
    }
    case LWS_CALLBACK_RECEIVE:
    case LWS_CALLBACK_CLIENT_RECEIVE:
    case LWS_CALLBACK_RAW_RX: {
      MinnetWebsocket* ws = 0; // minnet_ws_data(client->ws_obj);

      if(!ws)
        ws = minnet_ws_from_wsi(wsi);

      if(client->cb_message.ctx || (client->cb_message.ctx = ctx)) {
        JSValue msg = ws->binary ? JS_NewArrayBufferCopy(client->cb_message.ctx, in, len) : JS_NewStringLen(client->cb_message.ctx, in, len);
        JSValue cb_argv[2] = {client->ws_obj, msg};
        minnet_emit(&client->cb_message, 2, cb_argv);
        JS_FreeValue(ctx, cb_argv[1]);
      }
      return 0;
    }
    case LWS_CALLBACK_CLIENT_RECEIVE_PONG: {
      if(client->cb_pong.ctx || (client->cb_pong.ctx = ctx)) {
        JSValue data = JS_NewArrayBufferCopy(client->cb_pong.ctx, in, len);
        JSValue cb_argv[2] = {client->ws_obj, data};
        minnet_emit(&client->cb_pong, 2, cb_argv);
        JS_FreeValue(ctx, cb_argv[1]);
      }
      break;
    }
    case LWS_CALLBACK_LOCK_POLL:
    case LWS_CALLBACK_UNLOCK_POLL: {
      break;
    }
    case LWS_CALLBACK_ADD_POLL_FD:
    case LWS_CALLBACK_DEL_POLL_FD:
    case LWS_CALLBACK_CHANGE_MODE_POLL_FD: {
      if(!client)
        client = lws_context_user(lws_get_context(lws_get_network_wsi(wsi)));
      MinnetCallback* cb_fd = &client->cb_fd;

      return fd_callback(wsi, reason, cb_fd, in);
    }

    default: {
      // minnet_lws_unhandled(reason);
      break;
    }
  }

  /*if(reason < LWS_CALLBACK_ADD_POLL_FD || reason > LWS_CALLBACK_UNLOCK_POLL)
    lwsl_user("client  %-25s fd=%i, in='%.*s'\n", lws_callback_name(reason) + 13, lws_get_socket_fd(lws_get_network_wsi(wsi)), (int)len, (char*)in);*/

  return 0;
  //  return lws_callback_http_dummy(wsi, reason, user, in, len);
}

JSValue
minnet_client_wrap(JSContext* ctx, MinnetClient* cli) {
  JSValue ret;

  ret = JS_NewObjectProtoClass(ctx, minnet_client_proto, minnet_client_class_id);

  JS_SetOpaque(ret, cli);
  return ret;
}

enum {
  EVENT_MESSAGE = 0,
  EVENT_CONNECT,
  EVENT_CLOSE,
  EVENT_PONG,
  EVENT_FD,
};

static JSValue
minnet_client_get(JSContext* ctx, JSValueConst this_val, int magic) {
  MinnetClient* cli;
  JSValue ret = JS_UNDEFINED;

  if(!(cli = minnet_client_data(this_val)))
    return JS_UNDEFINED;

  switch(magic) {
    case EVENT_MESSAGE:
    case EVENT_CONNECT:
    case EVENT_CLOSE:
    case EVENT_PONG:
    case EVENT_FD: {
      ret = JS_DupValue(ctx, cli->callbacks[magic - EVENT_MESSAGE].func_obj);
      break;
    }
  }
  return ret;
}

static JSValue
minnet_client_set(JSContext* ctx, JSValueConst this_val, JSValueConst value, int magic) {
  MinnetClient* cli;
  JSValue ret = JS_UNDEFINED;

  if(!(cli = JS_GetOpaque2(ctx, this_val, minnet_client_class_id)))
    return JS_EXCEPTION;

  switch(magic) {
    case EVENT_MESSAGE:
    case EVENT_CONNECT:
    case EVENT_CLOSE:
    case EVENT_PONG:
    case EVENT_FD: {
      int index = magic - EVENT_MESSAGE;
      if(!JS_IsFunction(ctx, value))
        return JS_ThrowTypeError(ctx, "event handler must be function");

      JS_FreeValue(ctx, cli->callbacks[index].func_obj);
      cli->callbacks[index].func_obj = JS_DupValue(ctx, value);
      break;
    }
  }
  return ret;
}

static void
minnet_client_finalizer(JSRuntime* rt, JSValue val) {
  MinnetClient* cli = JS_GetOpaque(val, minnet_client_class_id);
  if(cli) {
    js_free_rt(rt, cli);
  }
}

JSClassDef minnet_client_class = {
    "MinnetClient",
    .finalizer = minnet_client_finalizer,
};

const JSCFunctionListEntry minnet_client_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("onmessage", minnet_client_get, minnet_client_set, EVENT_MESSAGE),
    JS_CGETSET_MAGIC_DEF("onconnect", minnet_client_get, minnet_client_set, EVENT_CONNECT),
    JS_CGETSET_MAGIC_DEF("onclose", minnet_client_get, minnet_client_set, EVENT_CLOSE),
    JS_CGETSET_MAGIC_DEF("onpong", minnet_client_get, minnet_client_set, EVENT_PONG),
    JS_CGETSET_MAGIC_DEF("onfd", minnet_client_get, minnet_client_set, EVENT_FD),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "MinnetClient", JS_PROP_CONFIGURABLE),
};

const size_t minnet_client_proto_funcs_size = countof(minnet_client_proto_funcs);
