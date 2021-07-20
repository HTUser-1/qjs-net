//import inspect from 'inspect';
//import * as bjson from 'bjson';
import { SyscallError } from '../../lib/misc.js';
import extendArray from '../modules/lib/extendArray.js';

let sockId;

extendArray(Array.prototype);

globalThis.GetClasses = function* GetClasses(obj) {
  let keys = getKeys(obj);

  for(let name of keys) {
    try {
      if(Util.isConstructor(obj[name])) yield [name, obj[name]];
    } catch(e) {}
  }

  // console.log("desc:", desc);
  return desc;
};

export function Mapper(map = new WeakMap()) {
  let self;
  self = function(key, value) {
    if(value === undefined) value = map.get(key);
    else map.set(key, value);
    return value;
  };
  return Object.setPrototypeOf(self, Mapper.prototype);
}
Mapper.prototype = new Function();
Mapper.prototype.constructor = Mapper;

export function DefaultConstructor(mapper, fn = (...args) => new Object(...args)) {
  let self;
  self = function(...args) {
    let [key, value] = args;
    if(args.length <= 1) {
      if(!(value = map.get(key))) {
        value = fn(...args.slice(1));
        map.set(key, value);
      }
    } else {
      map.set(key, value);
    }
    return value;
  };
  return Object.setPrototypeOf(self, DefaultConstructor.prototype);
}
DefaultConstructor.prototype = new Function();
DefaultConstructor.prototype.constructor = DefaultConstructor;

export function EventProxy(instance = {}, callback = (name, event, thisObj) => console.log('EventProxy', { name, event, thisObj })) {
  function WrapEvent(handler, name) {
    return function(e) {
      return callback(name, e, this);
    };
  }

  return new Proxy(instance, {
    get(obj, prop) {
      if(prop.startsWith('on')) {
        return WrapEvent(obj[prop], prop.slice(2));
      }
      return obj[prop];
    }
  });
}

/** @interface MessageReceiver */
export class MessageReceiver {
  static [Symbol.hasInstance](instance) {
    return 'onmessage' in instance;
  }

  /** @abstract */
  onmessage(msg) {
    throw new Error(`MessageReceiver.onmessage unimplemented`);
  }
}

/** @interface MessageTransmitter */
export class MessageTransmitter {
  static [Symbol.hasInstance](instance) {
    return typeof sendMessage == 'function';
  }
  /** @abstract */
  sendMessage() {
    throw new Error(`MessageReceiver.sendMessage unimplemented`);
  }
}

/**
 * @interface MessageTransceiver
 * @mixes MessageReceiver
 * @mixes MessageTransmitter
 */
export function MessageTransceiver() {}

Object.assign(MessageTransceiver.prototype, MessageReceiver.prototype, MessageTransmitter.prototype);

Object.defineProperty(MessageTransceiver, Symbol.hasInstance, {
  value: instance => [MessageReceiver, MessageTransmitter].every(ctor => ctor[Symbol.hasInstance](instance))
});

const codecs = {
  none() {
    return {
      name: 'none',
      encode: v => v,
      decode: v => v
    };
  },
  json(verbose = false) {
    return {
      name: 'json',
      encode: v => JSON.stringify(v, ...(verbose ? [null, 2] : [])),
      decode: v => JSON.parse(v)
    };
  }
};

if(globalThis.inspect) {
  codecs.js = function js(verbose = false) {
    return {
      name: 'js',
      encode: v => inspect(v, { colors: false, compact: verbose ? false : -2 }),
      decode: v => eval(`(${v})`)
    };
  };
}

if(globalThis.bjson) {
  codecs.bjson = function bjson() {
    return {
      name: 'bjson',
      encode: v => bjson.write(v),
      decode: v => bjson.read(v)
    };
  };
}

/**
 * @interface Connection
 */
export class Connection extends MessageTransceiver {
  static fromSocket = new WeakMap();

  static equal(a, b) {
    return (a.socket != null && a.socket === b.socket) || (typeof a.fd == 'number' && a.fd === b.fd);
  }
  static get last() {
    return this.list.last;
  }

  constructor(socket, instance, log, codec = 'none') {
    super();
    this.fd = socket.fd;
    define(this, {
      socket,
      exception: null,
      log: (...args) => log(this[Symbol.toStringTag], `(fd ${this.socket.fd})`, ...args)
    });

    define(this, typeof codec == 'string' && codecs[codec] ? { codecName: codec, codec: codecs[codec]() } : {});
    define(this, typeof codec == 'object' && codec.name ? { codecName: codec.name, codec } : {});

    //this.log('Connection.constructor', { socket, instance, log, codec });
    Connection.set.add(this);
    /*if(this.constructor != Connection && this.constructor && this.constructor.set)
      this.constructor.set.add(this);
*/
    Connection.fromSocket.set(socket, this);
  }

  error(message) {
    const { socket } = this;
    this.log(`ERROR: ${message}`);
    this.exception = new Error(message);
    this.close(socket.CLOSE_STATUS_PROTOCOL_ERR || 1000, message.slice(0, 128));
    return this.exception;
  }

  close(...args) {
    const { socket } = this;
    this.log('close(', ...args, ')');

    socket.close();
    delete this.socket;
    delete this.fd;
    this.connected = false;
  }

  onmessage(msg) {
    let { codec, codecName } = this;

    if(!msg) return;
    if(typeof msg == 'string' && msg.trim() == '') return;
    this.log('Connection.onmessage', { msg, codec, codecName });

    let data;
    try {
      data = codec.decode((msg && msg.data) || msg);
    } catch(err) {
      throw this.error(`${this.codec.name} parse error: '${(err && err.message) || msg}'` + err.stack);
      return this.exception;
    }
    let response = this.processMessage(data);
    this.log('Connection.onmessage', { data, response });
    if(isThenable(response)) response.then(r => this.sendMessage(r));
    else if(response !== undefined) this.sendMessage(response);
  }

  processMessage(data) {
    this.log('Connection.processMessage', { data });
    throw new Error('Virtual method');
  }

  onconnect() {
    this.log('Connection.onconnect');
  }

  onopen() {
    this.log('Connection.onopen');
  }

  onpong(data) {
    this.log('Connection.onpong:', data);
  }

  onerror(error) {
    this.log('Connection.onerror', error ? ` (${error})` : '');
    this.connected = false;
    this.cleanup();
  }

  onclose(reason) {
    this.log('Connection.onclose', reason ? ` (${reason})` : '');
    this.connected = false;
    this.cleanup();
  }

  cleanup() {
    if(this.instances) for(let id in this.instances) delete this.instances[id];
  }

  sendMessage(obj) {
    if(typeof obj == 'object') if (typeof obj.seq == 'number') this.messages.responses[obj.seq] = obj;
    let msg = typeof obj != 'string' ? this.codec.encode(obj) : obj;
    this.log('Connection.sendMessage', msg);
    this.socket.send(msg);
  }

  sendCommand(command, params = { seq: this }) {
    let message = { command, ...params };
    if(typeof params.seq == 'number') this.messages.responses[params.seq] = obj;

    this.sendMessage(message);

    this.log('Connection.sendCommand', { message });
  }

  static getCallbacks(instance, verbosity = 0) {
    const { classes, fdlist, log } = instance;
    const ctor = this;
    const verbose = verbosity > 1 ? (...args) => log('VERBOSE', ...args) : () => {};

    log(`${ctor.name}.getCallbacks`, { instance, log, verbosity });

    const handle = (sock, event, ...args) => {
      let conn, obj;
      if((conn = fdlist[sock.fd])) {
        verbose(`Handle fd #${sock.fd} ${event}`);
        callHandler(conn, event, ...args);
      } else {
        throw new Error(`No connection for fd #${sock.fd}!`);
      }
      obj = { then: fn => (fn(sock.fd), obj) };
      return obj;
    };

    const remove = sock => {
      const { fd } = sock;
      delete fdlist[fd];
    };

    return {
      onConnect(sock) {
        verbose(`Connected`, { fd: sock.fd }, ctor.name);

        let connection = fdlist[sock.fd];
        if(!connection) connection = new ctor(sock, instance, log, 'json', classes);
        verbose(`Connected`, { connection });
        fdlist[sock.fd] = connection;
        handle(sock, 'connect');
      },
      onOpen(sock) {
        verbose(`Opened`, { fd: sock.fd }, ctor.name);
        fdlist[sock.fd] = new ctor(sock, instance, log, 'json', classes);
        handle(sock, 'open');
      },
      onMessage(sock, msg) {
        verbose(`Message`, { fd: sock.fd }, msg);
        handle(sock, 'message', msg);
      },
      onError(sock, error) {
        verbose(`Error`, { fd: sock.fd }, error);
        callHandler(instance, 'error', error);
        handle(sock, 'error', error);
        remove(sock);
      },
      onClose(sock, why) {
        verbose(`Closed`, { fd: sock.fd }, why);
        handle(sock, 'close', why);
        remove(sock);
      },
      onPong(sock, data) {
        verbose(`Pong`, { fd: sock.fd }, data);
        handle(sock, 'pong', data);
      }
    };
  }
}

define(Connection.prototype, { [Symbol.toStringTag]: 'Connection' });

Connection.list = [];

function RPCCommands(classes = {}) {
  return {
    new({ class: name, args = [] }) {
      let obj, ret, id;
      try {
        obj = new this.classes[name](...args);
        id = this.makeId();
        this.instances[id] = obj;
      } catch(e) {
        return statusResponse(false, e.message);
      }
      return { success: true, id, name };
    },
    list() {
      return { success: true, classes: Object.keys({ ...classes, ...this.classes }) };
    },
    delete: objectCommand(({ id }, respond) => {
      delete this.instances[id];
      return respond(true);
    }),
    call: objectCommand(({ obj, method, args = [] }, respond) => {
      if(method in obj && typeof obj[method] == 'function') {
        const result = obj[method](...args);
        if(isThenable(result)) return result.then(result => respond(true, result)).catch(error => respond(false, error));
        return respond(true, result);
      }
      return respond(false, `No such method on object #${id}: ${method}`);
    }),
    keys: objectCommand(({ obj, enumerable = true }, respond) => {
      return respond(true, getPropertyNames(obj, enumerable ? obj => Object.keys(obj) : obj => Object.getOwnPropertyNames(obj)));
    }),
    properties: makeListPropertiesCmd(v => typeof v != 'function'),
    methods: makeListPropertiesCmd(v => typeof v == 'function', { enumerable: false }),
    get: objectCommand(({ obj, property }, respond) => {
      if(property in obj && typeof obj[property] != 'function') {
        const result = obj[property];
        return respond(true, result);
      }
      return respond(false, `No such property on object #${id}: ${property}`);
    }),
    set: objectCommand(({ obj, property, value }, respond) => {
      return respond(true, (obj[property] = value));
    })
  };
}

export class RPCServerConnection extends Connection {
  constructor(socket, instance, log, codec = codecs.json(false), classes) {
    log('RPCServerConnection.constructor', { socket, classes, instance, log });

    super(socket, instance, log, codec);

    let connection = this;
    define(connection, {
      classes,
      instances: {},
      lastId: 0,
      connected: true,
      messages: { requests: {}, responses: {} },
      commands: RPCCommands(classes)
    });

    RPCServerConnection.set.add(connection);
  }

  makeId() {
    return ++this.lastId;
  }

  processMessage(data) {
    let ret = null;
    if(!('command' in data)) return statusResponse(false, `No command specified`);
    const { command, seq } = data;
    const { commands } = this;
    this.log('RPCServerConnection.processMessage', { data, command, seq });
    if(typeof seq == 'number') this.messages.requests[seq] = data;
    if(commands[command]) return commands[command].call(this, data);
    switch (command) {
      default: {
        ret = statusResponse(false, `No such command '${command}'`);
        break;
      }
    }
    return ret;
  }
}

define(RPCServerConnection.prototype, { [Symbol.toStringTag]: 'RPCServerConnection' });

RPCServerConnection.list = [];

/**
 * @class This class describes a client connection.
 *
 * @class      RPCClientConnection
 * @param      {Object} socket
 * @param      {Object} classes
 * @param      {Object} instance
 * @param      {Function} instance
 *
 */
export class RPCClientConnection extends Connection {
  constructor(socket, instance, log, codec = codecs.json(false), classes) {
    super(socket, instance, log, codec);
    this.instances = {};
    this.classes = classes;
    this.connected = true;
    RPCClientConnection.set.add(this);
  }

  processMessage(response) {
    const { success, error, result } = response;
    this.log('message:', response);
  }
}

define(RPCClientConnection.prototype, { [Symbol.toStringTag]: 'RPCClientConnection' });

/**
 * @class Creates new RPC socket
 *
 * @param      {string}     [url=window.location.href]     URL (ws://127.0.0.1) or Port
 * @param      {function}   [service=RPCServerConnection]  The service constructor
 * @return     {RPCSocket}  The RPC socket.
 */
export function RPCSocket(url, service = RPCServerConnection, verbosity = 1) {
  if(!new.target) return new RPCSocket(url, service, verbosity);

  const instance = new.target ? this : new RPCSocket(url, service, verbosity);
  const log = console.config
    ? (msg, ...args) => {
        const { console } = globalThis;
        console /*instance.log ??*/.log
          .call(
            console,
            msg,
            console.config({
              multiline: false,
              compact: false,
              maxStringLength: 100,
              stringBreakNewline: false,
              hideKeys: ['obj']
            }),
            ...args
          );
      }
    : (...args) => console.log(...args);

  define(instance, {
    get fd() {
      return Object.keys(this.fdlist)[0] ?? -1;
    },
    get socket() {
      return this.fdlist[this.fd]?.socket;
    },
    fdlist: {},
    classes: {},
    log
  });

  const callbacks = service.getCallbacks(instance, verbosity);

  if(!url) url = globalThis.location?.href;
  if(typeof url != 'object') url = parseURL(url);

  define(instance, {
    service,
    callbacks,
    url,
    log,
    register(ctor) {
      if(typeof ctor == 'object' && ctor !== null) {
        for(let name in ctor) instance.classes[name] = ctor[name];
      } else {
        instance.classes[ctor.name] = ctor;
      }
      return this;
    },
    listen(new_ws, os = globalThis.os) {
      if(!new_ws) new_ws = MakeWebSocket;
      this.log(`${service.name} listening on ${this.url}`);
      if(os) setHandlers(os, callbacks);
      this.listening = true;
      this.ws = new_ws(this.url, callbacks, true);
      if(new_ws !== MakeWebSocket)
        if(this.ws.then) this.ws.then(() => (this.listening = false));
        else this.listening = false;
      return this;
    },
    async connect(new_ws, os = globalThis.os) {
      if(!new_ws) new_ws = MakeWebSocket;
      this.log(`${service.name} connecting to ${this.url}`);
      if(os) setHandlers(os, callbacks);
      this.ws = new_ws(this.url, callbacks, false);
      console.log('connect()', this.ws);
      return this;
    },
    /* prettier-ignore */ get connected() {
      const ws = this.ws;
      console.log("ws", ws);
      if(ws)
      return typeof ws.readyState == 'number' ? ws.readyState == ws.OPEN : false;
    const {fdlist} = instance;
      console.log("fdlist", fdlist);

    return  fdlist[Object.keys( fdlist)[0]].connected;
    }
  });

  RPCSocket.set.add(instance);

  return instance;
}
for(let ctor of [RPCSocket, Connection, RPCClientConnection, RPCServerConnection]) {
  let set = new Set();

  define(ctor, {
    set,
    get list() {
      return [...set];
    },
    get last() {
      return this.list.last;
    }
  });
}

export function RPCFactory(clientConnection) {
  return function(className, ...args) {
    sendMessage;
  };
}

Object.defineProperty(RPCSocket.prototype, Symbol.toStringTag, { value: 'RPCSocket' });

function MakeWebSocket(url, callbacks) {
  let ws;
  try {
    ws = new WebSocket(url + '');
  } catch(error) {
    callbacks.onError(ws, error);
    return null;
  }
  ws.onconnect = () => callbacks.onConnect(ws);
  ws.onopen = () => callbacks.onOpen(ws);
  ws.onerror = error => callbacks.onError(ws, error);
  ws.onmessage = msg => callbacks.onMessage(ws, msg);
  ws.onpong = pong => callbacks.onPong(ws, pong);
  ws.onclose = reason => callbacks.onClose(ws, reason);
  ws.fd = sockId = (sockId | 0) + 1;

  return ws;
}

export function isThenable(value) {
  return typeof value == 'object' && value != null && typeof value.then == 'function';
}

export function hasHandler(obj, eventName) {
  if(typeof obj == 'object' && obj != null) {
    const handler = obj['on' + eventName];
    if(typeof handler == 'function') return handler;
  }
}

export function callHandler(obj, eventName, ...args) {
  let ret,
    fn = hasHandler(obj, eventName);
  if(fn) return fn.call(obj, ...args);
}

export function parseURL(url_or_port) {
  let protocol, host, port;
  if(!isNaN(+url_or_port)) [protocol, host, port] = ['ws', '0.0.0.0', url_or_port];
  else {
    [protocol = 'ws', host, port = 80] = [.../(.*:\/\/|)([^:/]*)(:[0-9]+|).*/.exec(url_or_port)].slice(1);
    if(typeof port == 'string') port = port.slice(1);
  }
  port = +port;
  if(protocol) {
    protocol = protocol.slice(0, -3);
    if(protocol.startsWith('http')) protocol = protocol.replace('http', 'ws');
  } else {
    protocol = 'ws';
  }

  return define(
    {
      protocol,
      host,
      port
    },
    {
      toString() {
        const { protocol, host, port } = this;
        return `${protocol || 'ws'}://${host}:${port}`;
      }
    }
  );
}

export function getPropertyNames(obj, method = (obj, depth) => (depth <= 1 ? Object.getOwnPropertyNames(obj) : [])) {
  let names = new Set();
  let depth = 0;
  do {
    for(let name of method(obj, depth)) names.add(name);
    let proto = Object.getPrototypeOf(obj);
    if(proto === obj) break;
    obj = proto;
  } while(typeof obj == 'object' && obj != null);
  return [...names];
}

export function getKeys(obj) {
  let keys = new Set();
  for(let key of getPropertyNames(obj)) keys.add(key);
  for(let key of getPropertyNames(obj, obj => Object.getOwnPropertySymbols(obj))) keys.add(key);
  return [...keys];
}

export function getPropertyDescriptors(obj, merge = true, pred = (proto, depth) => true) {
  let descriptors = [];
  let depth = 0,
    desc,
    ok;
  do {
    desc = Object.getOwnPropertyDescriptors(obj);
    try {
      ok = pred(obj, depth);
    } catch(e) {}

    if(ok) descriptors.push(desc);

    //for(let name in desc) if(!(name in descriptors)) descriptors[name] = desc[name];
    let proto = Object.getPrototypeOf(obj);
    if(proto === obj) break;
    obj = proto;
    ++depth;
  } while(typeof obj == 'object' && obj != null);

  if(merge) {
    let i = 0;
    let result = {};
    for(let desc of descriptors) for (let prop of getKeys(desc)) if(!(prop in result)) result[prop] = desc[prop];

    /*console.log(`desc[${i++}]:`, getKeys(desc));
      Object.assign(result, desc);*/
    return result;
  }
  return descriptors;
}

function define(obj, ...args) {
  let propdesc = {};
  for(let props of args) {
    let desc = Object.getOwnPropertyDescriptors(props);
    for(let prop of getKeys(desc)) {
      propdesc[prop] = { ...desc[prop], enumerable: false, configurable: true };
      if('value' in propdesc[prop]) propdesc[prop].writable = true;
    }
  }
  Object.defineProperties(obj, propdesc);
  return obj;
}

export function setHandlers(os, handlers) {
  handlers.onFd = function(fd, readable, writable) {
    os.setReadHandler(fd, readable);
    os.setWriteHandler(fd, writable);
  };
}

export function statusResponse(success, result_or_error, data) {
  let r = { success };
  if(result_or_error !== undefined) r[success ? 'result' : 'error'] = result_or_error;
  if(typeof data == 'object' && data != null && typeof data.seq == 'number') r.seq = data.seq;
  return r;
}

export function objectCommand(fn) {
  return function(data) {
    const respond = (success, result) => statusResponse(success, result, data);
    const { id, ...rest } = data;
    if(id in this.instances) {
      data.obj = this.instances[id];
      return fn.call(this, data, respond);
    }
    return respond(false, `No such object #${id}`);
  };
}

export function makeListPropertiesCmd(pred = v => typeof v != 'function', defaults = { maxDepth: Infinity }) {
  return objectCommand((data, respond) => {
    const { obj, enumerable = true, source = false, keyDescriptor = true, valueDescriptor = true } = data;
    defaults = { enumerable: true, writable: true, configurable: true, ...defaults };
    let propDesc = getPropertyDescriptors(obj, true, (proto, depth) => depth < (defaults.maxDepth ?? Infinity));
    let keys = getKeys(propDesc);
    let props = keys.reduce((acc, key) => {
      const desc = propDesc[key];
      let value = desc?.value || obj[key];
      if(pred(value)) {
        if(valueDescriptor) {
          value = makeValueDescriptor(value, source);
          for(let flag of ['enumerable', 'writable', 'configurable']) if(desc[flag] !== undefined) if (desc[flag] != defaults[flag]) value[flag] = desc[flag];
        } else if(typeof value == 'function') {
          value = value + '';
        }
        acc.push([keyDescriptor ? makeValueDescriptor(key) : key, value]);
      }
      return acc;
    }, []);
    return respond(true, props);
  });
}

export function getPrototypeName(proto) {
  return proto.constructor?.name ?? proto[Symbol.toStringTag];
}

export function makeValueDescriptor(value, source = false) {
  const type = typeof value;
  let desc = { type };
  if(type == 'object' && value != null) {
    desc['class'] = getPrototypeName(value) ?? getPrototypeName(Object.getPrototypeOf(value));
    desc['chain'] = Util.getPrototypeChain(value).map(getPrototypeName);
  } else if(type == 'symbol') {
    desc['description'] = value.description;
    desc['symbol'] = value.toString();
  } else if(type == 'function') {
    if(value.length !== undefined) desc['length'] = value.length;
  }
  if(value instanceof ArrayBuffer) {
    let array = new Uint8Array(value);
    value = [...array];
    desc['class'] = 'ArrayBuffer';
    delete desc['chain'];
  }
  if(typeof value == 'function') {
    if(source) desc.source = value + '';
  } else if(typeof value != 'symbol') {
    desc.value = value;
  }
  return desc;
}

export default {
  ServerConnection: RPCServerConnection,
  ClientConnection: RPCClientConnection,
  Socket: RPCSocket,
  MessageReceiver,
  MessageTransmitter,
  MessageTransceiver,
  EventProxy,
  SyscallError,
  define
};
