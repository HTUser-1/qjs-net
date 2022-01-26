import net, { URL, LLL_ERR, LLL_WARN, LLL_NOTICE, LLL_INFO, LLL_DEBUG, LLL_PARSER, LLL_HEADER, LLL_EXT, LLL_CLIENT, LLL_LATENCY, LLL_USER, LLL_THREAD } from 'net';
import * as std from 'std';

export const Levels = Object.keys({ LLL_ERR, LLL_WARN, LLL_NOTICE, LLL_INFO, LLL_DEBUG, LLL_PARSER, LLL_HEADER, LLL_EXT, LLL_CLIENT, LLL_LATENCY, LLL_USER, LLL_THREAD }).reduce((acc, n) => {
  let v = Math.log2(net[n]);
  if(Math.floor(v) === v) acc[net[n]] = n.substring(4);
  return acc;
}, {});

export const DefaultLevels = LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO | LLL_CLIENT | LLL_LATENCY | LLL_USER | LLL_THREAD;

export const Init = (name, mask = net.LLL_USER | ((net.LLL_CLIENT << 1) - 1)) =>
  net.setLog(mask, (level, msg) => {
    let l = Levels[level];
    if(!(level & mask)) return;
    if(level >= LLL_NOTICE && level <= LLL_EXT) return;
    if(l == 'USER') l = name ?? l;
    std.err.puts(`${l.padEnd(10)} ${msg}\n`);
  });

export const SetLog = (name, maxLevel = net.LLL_CLIENT) =>
  net.setLog(net.LLL_USER | ((maxLevel << 1) - 1), (level, msg) => {
    let l = Levels[level] ?? 'UNKNOWN';
    if(l == 'USER') l = name ?? l;
    std.err.puts(('X', l).padEnd(9) + msg.replace(/\r/g, '\\r').replace(/\n/g, '\\n'));
  });

import('console').then(({ Console }) => {
  const out = std.err;
  globalThis.console = new Console(out, { inspectOptions: { compact: 2, customInspect: true, maxStringLength: 100 } });
});
