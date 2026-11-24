// The JavaScript Uploader (web/proto.js, the one the uploader page uses) against the real C device code (the firmware's lofi_proto.h) over a lossy link,
// then a "reboot" of the device: the version 2 pack must arrive intact and survive. Usage: node test/test_upload.js out/ci_test.lfp
const fs = require('fs'), path = require('path'), cp = require('child_process');
const LF = {}; Object.assign(LF, require('../web/proto.js'));
const root = path.join(__dirname, '..'), packPath = process.argv[2] || path.join(root, 'out/ci_test.lfp'), img = new Uint8Array(fs.readFileSync(packPath));
const bin = '/tmp/oual_dev_server'; cp.execFileSync('cc', ['-O2', '-o', bin, path.join(__dirname, 'dev_server.c')]);
function device() {
  const proc = cp.spawn(bin), lines = []; let waiter = null, buf = '';
  proc.stdout.on('data', d => { buf += d; let i; while ((i = buf.indexOf('\n')) >= 0) { const l = buf.slice(0, i); buf = buf.slice(i + 1); if (waiter) { const w = waiter; waiter = null; w(l); } else lines.push(l); } });
  const ask = line => new Promise(res => { if (lines.length) return res(lines.shift()); waiter = res; proc.stdin.write(line + '\n'); });
  return { ask, kill: () => proc.kill() };
}
const hex = u8 => Buffer.from(u8).toString('hex'), unhex = s => new Uint8Array(Buffer.from(s, 'hex'));
function lossy(dev, loss, seed) {
  let x = seed; const rnd = () => (x = (x * 1664525 + 1013904223) >>> 0) / 4294967296; const replies = []; let chain = Promise.resolve();
  return { send(m) { chain = chain.then(async () => { if (rnd() * 100 < loss) return; const r = await dev.ask(hex(m)); if (r && rnd() * 100 >= loss) replies.push(unhex(r)); }); },
    async receive(ms) { await chain; return replies.length ? replies.shift() : null; }, drain() { replies.length = 0; } };
}
(async () => {
  if ((img[4] | img[5] << 8) !== 2) { console.log('FAIL: not a version 2 pack'); process.exit(1); }
  let bad = 0;
  for (const loss of [0, 10, 25]) for (const seed of [1, 2, 3]) {
    const dev = device(), up = new LF.proto.Uploader(lossy(dev, loss, seed), { pacingMs: 0, replyTimeoutMs: 20 });
    await up.send(img); const want = 'state=1 size=' + img.length + ' crc=' + LF.proto.crc32(img).toString(16).padStart(8, '0');
    const d1 = await dev.ask('dump'); await dev.ask('reboot'); const d2 = await dev.ask('dump'); dev.kill();
    const ok = d1 === want && d2 === want; if (!ok) bad++;
    if (seed === 1 || !ok) console.log((ok ? 'PASS  ' : 'FAIL  ') + 'loss ' + loss + '% seed ' + seed + ': ' + d1 + ' | after reboot: ' + d2 + ' | blocks sent ' + up.blocksSent + ', re-sent ' + up.blocksResent);
  }
  console.log(bad ? 'FAILED ' + bad : 'ALL PASSED (9 uploads of ' + img.length + ' bytes)'); process.exit(bad ? 1 : 0);
})();
