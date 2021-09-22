#include "jsutils.h"
#include "minnet-websocket.h"
#include "minnet-server.h"
#include "minnet-server-http.h"
#include "minnet-response.h"
#include "minnet-request.h"
#include <list.h>
#include <quickjs-libc.h>
#include <libwebsockets.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

MinnetServer minnet_server = {0};

static int minnet_ws_callback(struct lws*, enum lws_callback_reasons, void*, void*, size_t);
int http_writable(struct lws*, struct http_response*, BOOL done);
int minnet_http_callback(struct lws*, enum lws_callback_reasons, void* user, void* in, size_t len);

/**
 * @brief      Create HTTP server mount
 */
static MinnetHttpMount*
mount_create(JSContext* ctx, const char* mountpoint, const char* origin, const char* def, enum lws_mount_protocols origin_proto) {
  MinnetHttpMount* m = js_mallocz(ctx, sizeof(MinnetHttpMount));

  // printf("mount_create mnt=%-10s org=%-10s def=%s\n", mountpoint, origin, def);

  m->lws.mountpoint = js_strdup(ctx, mountpoint);
  m->lws.origin = origin ? js_strdup(ctx, origin) : 0;
  m->lws.def = def ? js_strdup(ctx, def) : 0;
  m->lws.protocol = "http";
  m->lws.origin_protocol = origin_proto;
  m->lws.mountpoint_len = strlen(mountpoint);

  return m;
}

static MinnetHttpMount*
mount_new(JSContext* ctx, JSValueConst obj) {
  MinnetHttpMount* ret;
  JSValue mnt = JS_UNDEFINED, org = JS_UNDEFINED, def = JS_UNDEFINED;

  if(JS_IsArray(ctx, obj)) {
    mnt = JS_GetPropertyUint32(ctx, obj, 0);
    org = JS_GetPropertyUint32(ctx, obj, 1);
    def = JS_GetPropertyUint32(ctx, obj, 2);
  } else if(JS_IsFunction(ctx, obj)) {
    size_t namelen;
    JSValue name = JS_GetPropertyStr(ctx, obj, "name");
    const char* namestr = JS_ToCStringLen(ctx, &namelen, name);
    char buf[namelen + 2];
    pstrcpy(&buf[1], namelen + 1, namestr);
    buf[0] = '/';
    buf[namelen + 1] = '\0';
    JS_FreeCString(ctx, namestr);
    mnt = JS_NewString(ctx, buf);
    org = JS_DupValue(ctx, obj);
    JS_FreeValue(ctx, name);
  }

  const char* path = JS_ToCString(ctx, mnt);

  if(JS_IsFunction(ctx, org)) {
    ret = mount_create(ctx, path, 0, 0, LWSMPRO_CALLBACK);

    GETCBTHIS(org, ret->callback, JS_UNDEFINED);

  } else {
    const char* dest = JS_ToCString(ctx, org);
    const char* dotslashslash = strstr(dest, "://");
    size_t plen = dotslashslash ? dotslashslash - dest : 0;
    const char* origin = &dest[plen ? plen + 3 : 0];
    const char* index = JS_IsUndefined(def) ? 0 : JS_ToCString(ctx, def);
    enum lws_mount_protocols proto = plen == 0 ? LWSMPRO_CALLBACK : !strncmp(dest, "https", plen) ? LWSMPRO_HTTPS : LWSMPRO_HTTP;

    ret = mount_create(ctx, path, origin, index, proto);

    if(index)
      JS_FreeCString(ctx, index);
    JS_FreeCString(ctx, dest);
  }

  JS_FreeCString(ctx, path);

  JS_FreeValue(ctx, mnt);
  JS_FreeValue(ctx, org);
  JS_FreeValue(ctx, def);

  return ret;
}

static struct http_mount*
mount_find(const char* x, size_t n) {
  struct lws_http_mount *ptr, *m = 0;
  int protocol = n == 0 ? LWSMPRO_CALLBACK : LWSMPRO_HTTP;
  size_t l = 0;
  if(n == 0)
    n = strlen(x);
  if(protocol == LWSMPRO_CALLBACK && x[0] == '/') {
    x++;
    n--;
  }
  int i = 0;
  for(ptr = (struct lws_http_mount*)minnet_server.info.mounts; ptr; ptr = (struct lws_http_mount*)ptr->mount_next) {
    if(protocol != LWSMPRO_CALLBACK || ptr->origin_protocol == LWSMPRO_CALLBACK) {
      const char* mnt = ptr->mountpoint;
      size_t len = ptr->mountpoint_len;
      if(protocol == LWSMPRO_CALLBACK && mnt[0] == '/') {
        mnt++;
        len--;
      }
      // printf("mount_find [%i] %.*s\n", i++, (int)len, mnt);
      if(len == n && !strncmp(x, mnt, n)) {
        m = ptr;
        l = n;
        break;
      }
      if(n >= len && len >= l && !strncmp(mnt, x, MIN(len, n))) {
        m = ptr;
        l = len;
      }
    }
  }
  return (struct http_mount*)m;
}

static void
mount_free(JSContext* ctx, MinnetHttpMount const* m) {
  js_free(ctx, (void*)m->lws.mountpoint);

  if(m->lws.origin)
    js_free(ctx, (void*)m->lws.origin);

  if(m->lws.def)
    js_free(ctx, (void*)m->lws.def);

  js_free(ctx, (void*)m);
}

static struct lws_protocols protocols[] = {
    {"minnet", minnet_ws_callback, sizeof(MinnetSession), 0, 0, 0, 1024},
    {"http", minnet_http_callback, sizeof(MinnetSession), 0, 0, 0, 1024},
    LWS_PROTOCOL_LIST_TERM,
};

// static const MinnetHttpMount* mount_dyn;

JSValue
minnet_ws_server(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  int a = 0;
  int port = 7981;
  memset(&minnet_server, 0, sizeof minnet_server);

  lwsl_user("Minnet WebSocket Server\n");
  JSValue ret = JS_NewInt32(ctx, 0);
  JSValue options = argv[0];

  JSValue opt_port = JS_GetPropertyStr(ctx, options, "port");
  JSValue opt_host = JS_GetPropertyStr(ctx, options, "host");
  JSValue opt_on_pong = JS_GetPropertyStr(ctx, options, "onPong");
  JSValue opt_on_close = JS_GetPropertyStr(ctx, options, "onClose");
  JSValue opt_on_connect = JS_GetPropertyStr(ctx, options, "onConnect");
  JSValue opt_on_message = JS_GetPropertyStr(ctx, options, "onMessage");
  JSValue opt_on_fd = JS_GetPropertyStr(ctx, options, "onFd");
  JSValue opt_on_http = JS_GetPropertyStr(ctx, options, "onHttp");
  // JSValue opt_on_body = JS_GetPropertyStr(ctx, options, "onBody");
  JSValue opt_mounts = JS_GetPropertyStr(ctx, options, "mounts");

  if(!JS_IsUndefined(opt_port))
    JS_ToInt32(ctx, &port, opt_port);

  if(JS_IsString(opt_host))
    minnet_server.info.vhost_name = js_to_string(ctx, opt_host);
  else
    minnet_server.info.vhost_name = js_strdup(ctx, "localhost");

  GETCB(opt_on_pong, minnet_server.cb_pong)
  GETCB(opt_on_close, minnet_server.cb_close)
  GETCB(opt_on_connect, minnet_server.cb_connect)
  GETCB(opt_on_message, minnet_server.cb_message)
  GETCB(opt_on_fd, minnet_server.cb_fd)
  GETCB(opt_on_http, minnet_server.cb_http)

  protocols[0].user = ctx;
  protocols[1].user = ctx;

  minnet_server.ctx = ctx;
  minnet_server.info.port = port;
  minnet_server.info.protocols = protocols;
  minnet_server.info.mounts = 0;
  minnet_server.info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT /*| LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE*/;

  minnet_ws_sslcert(ctx, &minnet_server.info, options);

  if(JS_IsArray(ctx, opt_mounts)) {
    MinnetHttpMount** ptr = (MinnetHttpMount**)&minnet_server.info.mounts;
    uint32_t i;

    for(i = 0;; i++) {
      JSValue mount = JS_GetPropertyUint32(ctx, opt_mounts, i);

      if(JS_IsUndefined(mount))
        break;

      ADD(ptr, mount_new(ctx, mount), next);

      /**ptr = mount_new(ctx, mount);
      ptr = (MinnetHttpMount const**)&(*ptr)->next;*/
    }
  }

  if(!(minnet_server.context = lws_create_context(&minnet_server.info))) {
    lwsl_err("Libwebsockets init failed\n");
    return JS_EXCEPTION;
  }

  lws_service_adjust_timeout(minnet_server.context, 1, 0);

  while(a >= 0) {
    if(minnet_exception) {
      ret = JS_EXCEPTION;
      break;
    }

    if(minnet_server.cb_fd.ctx)
      js_std_loop(ctx);
    else
      a = lws_service(minnet_server.context, 20);
  }

  lws_context_destroy(minnet_server.context);

  if(minnet_server.info.mounts) {
    const MinnetHttpMount *mount, *next;

    for(mount = (MinnetHttpMount*)minnet_server.info.mounts; mount; mount = next) {
      next = (MinnetHttpMount*)mount->lws.mount_next;
      mount_free(ctx, mount);
    }
  }

  if(minnet_server.info.ssl_cert_filepath)
    JS_FreeCString(ctx, minnet_server.info.ssl_cert_filepath);

  if(minnet_server.info.ssl_private_key_filepath)
    JS_FreeCString(ctx, minnet_server.info.ssl_private_key_filepath);

  js_free(ctx, (void*)minnet_server.info.vhost_name);

  FREECB(minnet_server.cb_pong)
  FREECB(minnet_server.cb_close)
  FREECB(minnet_server.cb_connect)
  FREECB(minnet_server.cb_message)
  FREECB(minnet_server.cb_fd)
  FREECB(minnet_server.cb_http)

  return ret;
}

static struct session_data*
get_context(void* user, struct lws* wsi) {
  MinnetSession* serv;

  if((serv = (MinnetSession*)user)) {

    if(!JS_IsObject(serv->ws_obj))
      serv->ws_obj = minnet_ws_object(minnet_server.ctx, wsi);
  }

  return serv;
}

static int
respond(struct lws* wsi, MinnetBuffer* buf, MinnetResponse* resp) {
  struct list_head* el;
  // printf("RESPOND\tstatus=%d type=%s\n", resp->status, resp->type);

  resp->read_only = TRUE;
  /*
   * prepare and write http headers... with regards to content-
   * length, there are three approaches:
   *
   *  - http/1.0 or connection:close: no need, but no pipelining
   *  - http/1.1 or connected:keep-alive
   *     (keep-alive is default for 1.1): content-length required
   *  - http/2: no need, LWS_WRITE_HTTP_FINAL closes the stream
   *
   * giving the api below LWS_ILLEGAL_HTTP_CONTENT_LEN instead of
   * a content length forces the connection response headers to
   * send back "connection: close", disabling keep-alive.
   *
   * If you know the final content-length, it's always OK to give
   * it and keep-alive can work then if otherwise possible.  But
   * often you don't know it and avoiding having to compute it
   * at header-time makes life easier at the minnet_server.
   */
  if(lws_add_http_common_headers(wsi, resp->status, resp->type, LWS_ILLEGAL_HTTP_CONTENT_LEN, &buf->write, buf->end))
    return 1;

  list_for_each(el, &resp->headers) {
    struct http_header* hdr = list_entry(el, struct http_header, link);

    if((lws_add_http_header_by_name(wsi, (const unsigned char*)hdr->name, (const unsigned char*)hdr->value, strlen(hdr->value), &buf->write, buf->end)))
      JS_ThrowInternalError(minnet_server.cb_http.ctx, "lws_add_http_header_by_name failed");
  }

  if(lws_finalize_write_http_header(wsi, buf->start, &buf->write, buf->end))
    return 1;

  return 0;
}

static MinnetResponse*
request_handler(MinnetSession* serv, MinnetCallback* cb) {
  MinnetResponse* resp = minnet_response_data(minnet_server.ctx, serv->resp_obj);

  if(cb->ctx) {
    JSValue ret = minnet_emit_this(cb, serv->ws_obj, 2, serv->args);
    if(JS_IsObject(ret) && minnet_response_data(cb->ctx, ret)) {
      JS_FreeValue(cb->ctx, serv->args[1]);
      serv->args[1] = ret;
      resp = minnet_response_data(cb->ctx, ret);
      response_dump(resp);
    } else {
      JS_FreeValue(cb->ctx, ret);
    }
  }

  return resp;
}

static int
headers(JSContext* ctx, MinnetBuffer* headers, struct lws* wsi) {
  int tok, len, count = 0;

  if(!headers->start)
    buffer_alloc(headers, 1024, ctx);

  for(tok = WSI_TOKEN_HOST; tok < WSI_TOKEN_COUNT; tok++) {
    if(tok == WSI_TOKEN_HTTP)
      continue;

    if((len = lws_hdr_total_length(wsi, tok)) > 0) {
      char hdr[len + 1];
      const char* name = lws_token_to_string(tok);
      int namelen = byte_chr(name, strlen(name), ':');
      lws_hdr_copy(wsi, hdr, len + 1, tok);
      hdr[len] = '\0';
      // printf("headers %i %.*s '%s'\n", tok, namelen, name, hdr);
      while(!buffer_printf(headers, "%.*s: %s\n", namelen, name, hdr)) buffer_grow(headers, 1024, ctx);
      ++count;
    }
  }
  return count;
}

static inline int
is_h2(struct lws* wsi) {
  return lws_get_network_wsi(wsi) != wsi;
}

static size_t
file_size(FILE* fp) {
  long pos = ftell(fp);
  size_t size = 0;

  if(fseek(fp, 0, SEEK_END) != -1) {
    size = ftell(fp);
    fseek(fp, pos, SEEK_SET);
  }
  return size;
}

static int
serve_file(struct lws* wsi, const char* path, struct http_mount* mount, struct http_response* resp, JSContext* ctx) {
  FILE* fp;
  // printf("\033[38;5;226mSERVE FILE\033[0m\tis_h2=%i path=%s mount=%s\n", is_h2(wsi), path, mount ? mount->mnt : 0);

  const char* mime = lws_get_mimetype(path, &mount->lws);

  if(path[0] == '\0')
    path = mount->def;

  if((fp = fopen(path, "rb"))) {
    size_t n = file_size(fp);

    buffer_alloc(&resp->body, n, ctx);

    if(fread(resp->body.write, n, 1, fp) == 1)
      resp->body.write += n;

    if(mime) {
      if(resp->type)
        js_free(ctx, resp->type);

      resp->type = js_strdup(ctx, mime);
    }

    fclose(fp);
  } else {
    const char* body = "<html><head><title>404 Not Found</title><meta charset=utf-8 http-equiv=\"Content-Language\" content=\"en\"/></head><body><h1>404 Not Found</h1></body></html>";
    resp->status = 404;
    resp->ok = FALSE;

    response_write(resp, body, strlen(body), ctx);
  }

  return 0;
}

static int
minnet_ws_callback(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len) {
  JSValue ws_obj = JS_UNDEFINED;
  MinnetSession* serv = user;
  MinnetHttpMethod method;
  char* url = lws_uri_and_method(wsi, minnet_server.cb_connect.ctx, &method);

  // printf("ws %s\tfd=%d in='%.*s'\n", lws_callback_name(reason), lws_get_socket_fd(wsi), len, in);

  switch((int)reason) {
    case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS:
    case LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED:
    case LWS_CALLBACK_PROTOCOL_INIT: return 0;

    case LWS_CALLBACK_ESTABLISHED: {

      if(minnet_server.cb_connect.ctx) {
        struct wsi_opaque_user_data* user = lws_get_opaque_user_data(wsi);

        serv->args[1] = minnet_ws_wrap(minnet_server.cb_connect.ctx, user->req);

        // printf("ws %s wsi=%p, ws=%p, req=%p, url=%s, serv=%p, user=%p\n", lws_callback_name(reason) + 13, wsi, user->ws, user->req, url, serv, lws_get_opaque_user_data(wsi));

        minnet_emit_this(&minnet_server.cb_connect, serv->args[0], 2, serv->args);
      }

      return 0;
    }

    case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
    case LWS_CALLBACK_CLOSED: {
      if(!serv->closed) {
        JSValue why = JS_UNDEFINED;
        int code = -1;

        if(in) {
          uint8_t* codep = in;
          code = (codep[0] << 8) + codep[1];
          if(len - 2 > 0)
            why = JS_NewStringLen(minnet_server.ctx, in + 2, len - 2);
        }

        printf("ws %s fd=%d\n", lws_callback_name(reason), lws_get_socket_fd(wsi));

        if(minnet_server.cb_close.ctx) {
          JSValue cb_argv[3] = {JS_DupValue(minnet_server.cb_close.ctx, serv->ws_obj), code != -1 ? JS_NewInt32(minnet_server.cb_close.ctx, code) : JS_UNDEFINED, why};
          minnet_emit(&minnet_server.cb_close, code != -1 ? 3 : 1, cb_argv);
          JS_FreeValue(minnet_server.cb_close.ctx, cb_argv[0]);
          JS_FreeValue(minnet_server.cb_close.ctx, cb_argv[1]);
        }
        JS_FreeValue(minnet_server.ctx, why);
        JS_FreeValue(minnet_server.ctx, serv->ws_obj);
        serv->ws_obj = JS_NULL;
        serv->closed = 1;
      }
      return 0;
    }

    case LWS_CALLBACK_SERVER_WRITEABLE: {
      /*   printf("ws %s fd=%d\n", lws_callback_name(reason), lws_get_socket_fd(wsi));
         lws_callback_on_writable(wsi);*/
      return 0;
    }
    case LWS_CALLBACK_RECEIVE: {
      if(minnet_server.cb_message.ctx) {
        //  ws_obj = minnet_ws_wrap(minnet_server.cb_message.ctx, wsi);
        JSValue msg = JS_NewStringLen(minnet_server.cb_message.ctx, in, len);
        JSValue cb_argv[2] = {JS_DupValue(minnet_server.cb_message.ctx, serv->ws_obj), msg};
        minnet_emit(&minnet_server.cb_message, 2, cb_argv);
        JS_FreeValue(minnet_server.cb_message.ctx, cb_argv[0]);
        JS_FreeValue(minnet_server.cb_message.ctx, cb_argv[1]);
      }
      return 0;
    }
    case LWS_CALLBACK_RECEIVE_PONG: {
      if(minnet_server.cb_pong.ctx) {
        // ws_obj = minnet_ws_wrap(minnet_server.cb_pong.ctx, wsi);
        JSValue msg = JS_NewArrayBufferCopy(minnet_server.cb_pong.ctx, in, len);
        JSValue cb_argv[2] = {JS_DupValue(minnet_server.cb_pong.ctx, serv->ws_obj), msg};
        minnet_emit(&minnet_server.cb_pong, 2, cb_argv);
        JS_FreeValue(minnet_server.cb_pong.ctx, cb_argv[0]);
        JS_FreeValue(minnet_server.cb_pong.ctx, cb_argv[1]);
      }
      return 0;
    }

    case LWS_CALLBACK_LOCK_POLL:
    case LWS_CALLBACK_UNLOCK_POLL: return 0;

    case LWS_CALLBACK_ADD_POLL_FD: {
      struct lws_pollargs* args = in;

      if(minnet_server.cb_fd.ctx) {
        JSValue argv[3] = {JS_NewInt32(minnet_server.cb_fd.ctx, args->fd)};
        minnet_handlers(minnet_server.cb_fd.ctx, wsi, args, &argv[1]);

        minnet_emit(&minnet_server.cb_fd, 3, argv);

        JS_FreeValue(minnet_server.cb_fd.ctx, argv[0]);
        JS_FreeValue(minnet_server.cb_fd.ctx, argv[1]);
        JS_FreeValue(minnet_server.cb_fd.ctx, argv[2]);
      }
      return 0;
    }
    case LWS_CALLBACK_DEL_POLL_FD: {
      struct lws_pollargs* args = in;

      if(minnet_server.cb_fd.ctx) {
        JSValue argv[3] = {
            JS_NewInt32(minnet_server.cb_fd.ctx, args->fd),
        };
        minnet_handlers(minnet_server.cb_fd.ctx, wsi, args, &argv[1]);
        minnet_emit(&minnet_server.cb_fd, 3, argv);
        JS_FreeValue(minnet_server.cb_fd.ctx, argv[0]);
        JS_FreeValue(minnet_server.cb_fd.ctx, argv[1]);
        JS_FreeValue(minnet_server.cb_fd.ctx, argv[2]);
      }
      return 0;
    }
    case LWS_CALLBACK_CHANGE_MODE_POLL_FD: {
      struct lws_pollargs* args = in;

      if(minnet_server.cb_fd.ctx) {
        if(args->events != args->prev_events) {
          JSValue argv[3] = {JS_NewInt32(minnet_server.cb_fd.ctx, args->fd)};
          minnet_handlers(minnet_server.cb_fd.ctx, wsi, args, &argv[1]);

          minnet_emit(&minnet_server.cb_fd, 3, argv);
          JS_FreeValue(minnet_server.cb_fd.ctx, argv[0]);
          JS_FreeValue(minnet_server.cb_fd.ctx, argv[1]);
          JS_FreeValue(minnet_server.cb_fd.ctx, argv[2]);
        }
      }
      return 0;
    }

    case LWS_CALLBACK_WSI_CREATE: {
      return 0;
    }
    case LWS_CALLBACK_WSI_DESTROY:
    case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:
    case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
    case LWS_CALLBACK_ADD_HEADERS:
    case LWS_CALLBACK_WS_SERVER_DROP_PROTOCOL: {
      return 0;
    }

    case LWS_CALLBACK_HTTP:
    case LWS_CALLBACK_HTTP_BODY:
    case LWS_CALLBACK_HTTP_BODY_COMPLETION:
    case LWS_CALLBACK_HTTP_BIND_PROTOCOL:
    case LWS_CALLBACK_HTTP_CONFIRM_UPGRADE:
    case LWS_CALLBACK_CLOSED_HTTP:
    case LWS_CALLBACK_FILTER_HTTP_CONNECTION:
    case LWS_CALLBACK_HTTP_DROP_PROTOCOL: {
      return minnet_http_callback(wsi, reason, user, in, len);
    }
      /*
          default: {
            minnet_lws_unhandled("WS", reason);
            return 0;
          }*/
  }
  minnet_lws_unhandled("WS", reason);
  return 0;
  //  return lws_callback_http_dummy(wsi, reason, user, in, len);
}
