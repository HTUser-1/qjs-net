import * as std from 'std';
import * as os from 'os';
import * as path from 'path';
import { Console } from 'console';
import REPL from 'repl';
import { define } from 'util';
import inspect from 'inspect';
import * as net from 'net';
Error.stackTraceLimit = 100;
function GetOpt(options = {}, args) {
  let s, l;
  let r = {};
  let positional = (r['@'] = []);
  if(!(options instanceof Array)) options = Object.entries(options);
  const findOpt = a => options.find(([optname, option]) => (Array.isArray(option) ? option.indexOf(a) != -1 : false) || a == optname);
  let [, params] = options.find(o => o[0] == '@') || [];
  if(typeof params == 'string') params = params.split(',');
  for(let i = 0; i < args.length; i++) {
    const a = args[i];
    let o;
    if(a[0] == '-') {
      let n, v, x, y;
      if(a[1] == '-') l = true;
      else s = true;
      x = s ? 1 : 2;
      if(s) y = 2;
      else if((y = a.indexOf('=')) == -1) y = a.length;
      n = a.substring(x, y);
      if((o = findOpt(n))) {
        const [has_arg, handler] = o[1];
        if(has_arg) {
          if(a.length > y) v = a.substring(y + (a[y] == '='));
          else v = args[++i];
        } else {
          v = true;
        }
        try {
          handler(v, r[o[0]], options, r);
        } catch(err) {}
        r[o[0]] = v;
        continue;
      }
    }
    if(params.length) {
      const p = params.shift();
      if((o = findOpt(p))) {
        const [, [, handler]] = o;
        let v = a;
        if(typeof handler == 'function')
          try {
            v= handler(v, r[o[0]], options, r);
          }catch(err) { }
        const n = o[0];
        r[o[0]] = v;
        continue;
      }
    }
    r['@'] = [...(r['@'] ?? []), a];
  }
  return r;
}
/*function ReadJSON(filename) {
  let data = fs.readFileSync(filename, 'utf-8');
  if(data) console.debug(`${data.length} bytes read from '${filename}'`);
  return data ? JSON.parse(data) : null;
}
function WriteFile(name, data, verbose = true) {
  if(Util.isGenerator(data)) {
    let fd = fs.openSync(name, os.O_WRONLY | os.O_TRUNC | os.O_CREAT, 0x1a4);
    let r = 0;
    for(let item of data) {
      r += fs.writeSync(fd, toArrayBuffer(item + ''));
    }
    fs.closeSync(fd);
    let stat = fs.statSync(name);
    return stat?.size;
  }
  if(Util.isIterator(data)) data = [...data];
  if(Util.isArray(data)) data = data.join('\n');
  if(typeof data == 'string' && !data.endsWith('\n')) data += '\n';
  let ret = fs.writeFileSync(name, data);
  if(verbose) console.log(`Wrote ${name}: ${ret} bytes`);
}*/
function WriteJSON(name, data) {
  WriteFile(name, JSON.stringify(data, null, 2));
}
function main(...args) {
  const base = scriptArgs[0].replace(/.*\//g, '').replace(/\.[a-z]*$/, '');
  globalThis.console = new Console({
    inspectOptions: {
      colors: true,
      depth: Infinity,
      compact: 2,
      customInspect: true
    }
  });
  let params = GetOpt(
    {
      verbose: [false, (a, v) => (v | 0) + 1, 'v'],
      listen: [false, null, 'l'],
      connect: [false, null, 'c'],
      client: [false, null, 'C'],
      server: [false, null, 'S'],
      debug: [false, null, 'x'],
      address: [true, null, 'a'],
      port: [true, null, 'p'],
      'ssl-cert': [true, null],
      'ssl-private-key': [true, null],
      '@': 'url,'
    },
    args
  );
  const { 'ssl-cert': sslCert = 'localhost.crt', 'ssl-private-key': sslPrivateKey = 'localhost.key' } = params;
  const url = params['@'][0] ?? 'ws://127.0.0.1:8999';
  const listen = params.connect && !params.listen ? false : true;
  const server = !params.client || params.server;
  console.log('params', params);
  let connections = new Set();
  let repl;
  function createWS(url, callbacks, listen = 0) {
    let [protocol, host, port, ...location] = [...url.matchAll(/[^:\/]+/g)].map(a => a[0]);
    if(!isNaN(+port)) port = +port;
    const path = location.reduce((acc, part) => acc + '/' + part, '');
    console.log('createWS', { protocol, host, port, path });
    net.setLog(net.LLL_DEBUG - 1, (level, ...args) => {
      if(level == net.LLL_USER)
        console.log((['ERR', 'WARN', 'NOTICE', 'INFO', 'DEBUG', 'PARSER', 'HEADER', 'EXT', 'CLIENT', 'LATENCY', 'MINNET', 'THREAD'][Math.log2(level)] ?? level + '').padEnd(8), ...args);
    });
    const fn = [net.client, net.server][+listen];
    return fn({
      sslCert,
      sslPrivateKey,
      ssl: protocol == 'wss',
      host,
      port,
      path,
      ...callbacks,
      onConnect(ws, req) {
        connections.add(ws);
        console.log('onConnect', ws, req);
        try {
          repl = CreateREPL(`${host}:${port}`);
          repl.printStatus(`Connected to ${protocol}://${host}:${port}${path}`, true);
        } catch(err) {
          console.log('error:', err.message);
        }
      },
      onClose(ws, status, reason) {
        connections.delete(ws);
        console.log('onClose', ws, status, reason);
        repl.exit(status != 1000 ? 1 : 0);
      },
      onHttp(req, rsp) {
        const { url, method, headers } = req;
        console.log('\x1b[38;5;82monHttp\x1b[0m(\n\t', Object.setPrototypeOf({ url, method, headers }, Object.getPrototypeOf(req)), ',\n\t', rsp, '\n)');
        return rsp;
      },
      onFd(fd, rd, wr) {
        //console.log('onFd', fd, rd, wr);
        os.setReadHandler(fd, rd);
        os.setWriteHandler(fd, wr);
      },
      onMessage(ws, msg) {
        repl.printStatus(msg, true);
      },
      onError(ws, error) {
        console.log('onError', ws, error);
      }
    });
  }
  define(globalThis, {
    get connections() {
      return [...connections];
    }
  });
  createWS(url, {});
  function quit(why) {
    console.log(`quit('${why}')`);
    repl.cleanup(why);
  }
}
try {
  main(...scriptArgs.slice(1));
} catch(error) {
  console.log(`FAIL: ${error.message}\n${error.stack}`);
  std.exit(1);
} finally {
  //console.log('SUCCESS');
}

function CreateREPL(prompt2) {
  let name = scriptArgs[0];
  name = name
    .replace(/.*\//, '')
    .replace(/-/g, ' ')
    .replace(/\.[^\/.]*$/, '');
  let [prefix, suffix] = [name, prompt2];
  let repl = new REPL(`\x1b[38;5;40m${prefix} \x1b[38;5;226m${suffix}\x1b[0m`, false);
  repl.historyLoad(null, false);
  repl.help = () => {};
  let { log } = console;
  repl.show = arg => std.puts((typeof arg == 'string' ? arg : inspect(arg, globalThis.console.options)) + '\n');
  repl.handleCmd = data => {
    if(typeof data == 'string' && data.length > 0) {
      for(let connection of connections) {
        repl.printStatus(`Sending '${data}'`, false);
        connection.send(data);
      }
    }
  };
  repl.addCleanupHandler(() => {
    repl.readlineRemovePrompt();
    Terminal.mousetrackingDisable();
    let numLines = repl.historySave();
    repl.printStatus(`EXIT (wrote ${numLines} history entries)`, false);
    std.exit(0);
  });
  console.log = repl.printFunction(log);
  repl.runSync();
  return repl;
}
