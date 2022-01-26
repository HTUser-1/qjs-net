import * as std from 'std';
import * as os from 'os';
import net, { URL } from 'net';
import { Init } from './log.js';

const connections = new Set();

const escape = s =>
  [
    [/\r/g, '\\r'],
    [/\n/g, '\\n']
  ].reduce((a, [exp, rpl]) => a.replace(exp, rpl), s);

const abbreviate = s => (s.length > 100 ? s.substring(0, 45) + ' ... ' + s.substring(-45) : s);

export default function Client(url, options) {
  Init('CLIENT');

  return net.client(url, {
    ...options,
    onConnect(ws, req) {
      connections.add(ws);
      console.log('onConnect', ws, req);
      try {
        console.log(`Connected to: ${req.url}`);
      } catch(err) {
        console.log('error:', err.message);
      }
    },
    onClose(ws, status) {
      connections.delete(ws);
      console.log('onClose', { ws, status });
      std.exit(status != 1000 ? 1 : 0);
    },
    onError(ws, error) {
      connections.delete(ws);
      console.log('onError', { ws, error });
      std.exit(error);
    },
    onHttp(req, rsp) {
      const { url, method, headers } = req;
      console.log('\x1b[38;5;82monHttp\x1b[0m',  { url, method, headers });
      return rsp;
    },
    onFd(fd, rd, wr) {
      // console.log('onFd', fd, rd, wr);
      os.setReadHandler(fd, rd);
      os.setWriteHandler(fd, wr);
    },
    onMessage(ws, msg) {
      console.log('onMessage', console.config({ maxStringLen: 100 }), { ws, msg });
      std.puts(escape(abbreviate(msg)) + '\n');
    }
  });
}

Object.defineProperty(Client, 'connections', {
  get() {
    return [...connections];
  }
});
