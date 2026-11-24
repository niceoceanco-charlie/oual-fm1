// wasm_manifest: writes manifest.json and SHA256SUMS for a build of the oual WebAssembly module.  usage: node tools/wasm_manifest.mjs outdir version commit
import { readFileSync, writeFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
const [dir, version, commit] = process.argv.slice(2);
const files = ['oual.wasm', 'oual-core.mjs', 'oual.mjs', 'oual-worklet.js'];
const sums = {}; for (const f of files) sums[f] = createHash('sha256').update(readFileSync(`${dir}/${f}`)).digest('hex');
const mod = new WebAssembly.Module(readFileSync(`${dir}/oual.wasm`));
const manifest = { name: 'oual-wasm', version, api: 1, commit, license: 'GPL-3.0-only', engine: 'core/oual_engine.c (the FM-1 engine)', imports: WebAssembly.Module.imports(mod).length,
  exports: WebAssembly.Module.exports(mod).map((e) => e.name).sort(), bytes: readFileSync(`${dir}/oual.wasm`).length, sha256: sums };
writeFileSync(`${dir}/manifest.json`, JSON.stringify(manifest, null, 2) + '\n');
writeFileSync(`${dir}/SHA256SUMS`, files.map((f) => `${sums[f]}  ${f}`).join('\n') + '\n');
console.log(`oual-wasm ${version} (${manifest.bytes} bytes): ${sums['oual.wasm']}`);
