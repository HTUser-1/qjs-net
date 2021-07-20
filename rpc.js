//import inspect from 'inspect';
//import * as bjson from 'bjson';
import { SyscallError } from '../../lib/misc.js';

let sockId;

export function Mapper(map = new WeakMap()) {   
  let self;
  self = function(key,value) {
    if(value === undefined) value = map.get(key);
  else map.set(key,value);
  return value;
  }
  return Object.setPrototypeOf(self,Mapper.prototype);
}
Mapper.prototype = new Function;
Mapper.prototype.constructor = Mapper;


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

  static fromSocket = new  WeakMap();

  constructor(socket, instance, log, codec = 'none') {
    super();
    this.socket = socket;
    this.fd = socket.fd;
    this.exception = null;
    this.codec = typeof codec == 'string' ? codecs[codec]() : codec;
    this.log = (...args) => log(this[Symbol.toStringTag], `(fd ${this.socket.fd})`, ...args);
    this.log('new Connection');
    Connection.list.add(this);
    Connection.fromSocket.set(socket, this);
  }

  error(message) {
    const { socket } = this;
    this.log(`ERROR: ${message}`);
    this.exception = new Error(message);
    this.close(socket.CLOSE_STATUS_PROTOCOL_ERR || 1000, message);
    return this.exception;
  }

  close(...args) {
    const { socket } = this;
    this.log('close(', ...args, ')');

    socket.close(...args);
    delete this.socket;
    delete this.fd;
    this.connected = false;
  }

  onmessage(msg) {
    if(!msg) return;
    if(typeof msg == 'string' && msg.trim() == '') return;

     this.log('onmessage', { msg });
     let data;
    try {
      data = this.codec.decode(msg && msg.data || msg);
    } catch(err) {
      throw this.error(`${this.codec.name} parse error: '${(err && err.message) || msg}'`);
      return this.exception;
    }
    let response = this.processMessage(data);
    this.log('onmessage', { data, response });
    if(isThenable(response)) response.then(r => this.sendMessage(r));
    else if(response !== undefined) this.sendMessage(response);
  }

  processMessage(data) {
    this.log('message:', data);
  }

  onconnect() {
    this.log('connect');
  }

  onopen() {
    this.log('open');
  }

  onpong(data) {
    this.log('pong:', data);
  }

  onerror(error) {
    this.log('error', error ? ` (${error})` : '');
    this.connected = false;
    this.cleanup();
  }

  onclose(reason) {
    this.log('closed', reason ? ` (${reason})` : '');
    this.connected = false;
    this.cleanup();
  }

  cleanup() {
    if(this.instances) for(let id in this.instances) delete this.instances[id];
  }

  sendMessage(obj) {
    if(typeof obj == 'object') if (typeof obj.seq == 'number') this.messages.responses[obj.seq] = obj;
    let msg = typeof obj != 'string' ? this.codec.encode(obj) : obj;
    this.log('sending', msg);
    this.socket.send(msg);
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
        const connection = new ctor(sock, instance, log, 'json');

        verbose(`Connected`, { connection });
        fdlist[sock.fd] = connection;
        handle(sock, 'connect');
      },
      onOpen(sock) {
        verbose(`Opened`, { fd: sock.fd }, ctor.name);
        fdlist[sock.fd] = new ctor(sock, instance, log, 'json');
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

Connection.list = new /*Weak*/Set();

export class RPCServerConnection extends Connection {
  constructor(socket, instance, log, classes, codec = codecs.json(false)) {
    log('RPCServerConnection', { socket, classes, instance, log });

    super(socket, instance, log, codec);

    this.classes = classes;
    this.instances = {};
    this.lastId = 0;
    this.connected = true;
    this.messages = { requests: {}, responses: {} };
RPCServerConnection.list.add(this);
  }

  makeId() {
    return ++this.lastId;
  }

  static commands = {
    new({ name, args = [] }) {
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
 return {success: true, classes: Object.keys(this.classes) };
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

  processMessage(data) {
    let ret = null;
    if(!('command' in data)) return statusResponse(false, `No command specified`);
    const { command, seq } = data;
    const { commands } = this.constructor;
    this.log('message:', data);
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

RPCServerConnection.list = new Set();

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
  constructor(socket, instance, log, classes, codec = codecs.json(false)) {
    super(socket, instance, log, codec);
    this.instances = {};
    this.classes = classes;
    this.connected = true;
    RPCClientConnection.list.add(this);
  }

  processMessage(response) {
    const { success, error, result } = response;
    this.log('message:', response);
  }
}

define(RPCClientConnection.prototype, { [Symbol.toStringTag]: 'RPCClientConnection' });
RPCClientConnection.list = new Set();

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

  define(instance, {
    get fd() {
      return Object.keys(this.fdlist)[0] ?? -1;
    },
    get socket() {
      return this.fdlist[this.fd]?.socket;
    },
    fdlist: {},
    classes: {},
    log: console.config
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
      : (...args) => console.log(...args)
  });

  const callbacks = service.getCallbacks(instance, verbosity);

  if(!url) url = globalThis.location?.href;
  if(typeof url != 'object') url = parseURL(url);

  define(instance, {
    service,
    callbacks,
    url,
    register(ctor) {
      if(typeof ctor == 'object' && ctor !== null) {
        for(let name in ctor) classes[name] = ctor[name];
      } else {
        this.classes[ctor.name] = ctor;
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
    connect(new_ws, os = globalThis.os) {
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

  return instance;
}

export function RPCFactory(clientConnection) {
  
  return function(className, ...args) {

sendMessage
  } 
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

function isThenable(value) {
  return typeof value == 'object' && value != null && typeof value.then == 'function';
}

function hasHandler(obj, eventName) {
  if(typeof obj == 'object' && obj != null) {
    const handler = obj['on' + eventName];
    if(typeof handler == 'function') return handler;
  }
}

function callHandler(obj, eventName, ...args) {
  let ret,
    fn = hasHandler(obj, eventName);
  if(fn) return fn.call(obj, ...args);
}

function parseURL(url_or_port) {
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

function getPropertyNames(obj, method = obj => Object.getOwnPropertyNames(obj)) {
  let names = new Set();
  do {
    for(let name of method(obj)) names.add(name);
    let proto = Object.getPrototypeOf(obj);
    if(proto === obj) break;
    obj = proto;
  } while(typeof obj == 'object' && obj != null);
  return [...names];
}

function getKeys(obj) {
  let keys = new Set();
  for(let key of getPropertyNames(obj)) keys.add(key);
  for(let key of getPropertyNames(obj, obj => Object.getOwnPropertySymbols(obj))) keys.add(key);
  return [...keys];
}

function getPropertyDescriptors(obj, merge = true) {
  let descriptors = [];
  do {
    let desc = Object.getOwnPropertyDescriptors(obj);
    descriptors.push(desc);
    //for(let name in desc) if(!(name in descriptors)) descriptors[name] = desc[name];
    let proto = Object.getPrototypeOf(obj);
    if(proto === obj) break;
    obj = proto;
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
      if('value' in propdesc[prop]) propdesc[prop].writable = false;
    }
  }
  Object.defineProperties(obj, propdesc);
  return obj;
}

function setHandlers(os, handlers) {
  handlers.onFd = function(fd, readable, writable) {
    os.setReadHandler(fd, readable);
    os.setWriteHandler(fd, writable);
  };
}

function statusResponse(success, result_or_error, data) {
  let r = { success };
  if(result_or_error !== undefined) r[success ? 'result' : 'error'] = result_or_error;
  if(typeof data == 'object' && data != null && typeof data.seq == 'number') r.seq = data.seq;
  return r;
}

function objectCommand(fn) {
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

function makeListPropertiesCmd(pred = v => typeof v != 'function', defaults = {}) {
  return objectCommand((data, respond) => {
    const { obj, enumerable = true, source = false, keyDescriptor = true, valueDescriptor = true } = data;
    defaults = { enumerable: true, writable: true, configurable: true, ...defaults };
    let propDesc = getPropertyDescriptors(obj);
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

function getPrototypeName(proto) {
  return proto.constructor?.name ?? proto[Symbol.toStringTag];
}

function makeValueDescriptor(value, source = false) {
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
  SyscallError
};
