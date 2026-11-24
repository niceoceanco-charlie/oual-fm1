// The uploader page keeps the chosen MIDI port when the port list is rebuilt (a state change used to reset it to the first entry).
const fs = require('fs'), vm = require('vm'), path = require('path');
const html = fs.readFileSync(path.join(__dirname, '../web/uploader.template.html'), 'utf8');
const m = html.match(/let chosen = '';[\s\S]*?\$\('#out'\)\.onchange = [^\n]*/); if (!m) { console.log('FAIL: port code not found'); process.exit(1); }
class Option { constructor(t, v) { this.text = t; this.value = v; } }
const sel = { options: [], value: '', replaceChildren() { this.options = []; this.value = ''; }, append(o) { if (!this.options.length) this.value = o.value; this.options.push(o); } };
let outs = [{ id: 'a', name: 'Quantum 2626' }, { id: 'b', name: 'FM1 Community' }];
const ctx = { $: s => sel, midi: {}, Option, LF: { midi: { list: () => ({ outputs: outs }) } } }; vm.createContext(ctx); vm.runInContext(m[0], ctx);
let bad = 0; const check = (c, t) => { console.log((c ? 'PASS  ' : 'FAIL  ') + t); if (!c) bad++; };
ctx.fill(); check(sel.value === 'b', 'with no choice yet, the FM1 output is picked (' + sel.value + ')');
sel.value = 'a'; sel.onchange({ target: sel }); ctx.fill(); check(sel.value === 'a', 'a port the user picked survives a rebuilt list (' + sel.value + ')');
outs = [{ id: 'a', name: 'Quantum 2626' }]; ctx.fill(); outs = [{ id: 'a', name: 'Quantum 2626' }, { id: 'b', name: 'FM1 Community' }]; ctx.fill();
check(sel.value === 'a', 'it stays after the other port disappears and returns (' + sel.value + ')');
process.exit(bad ? 1 : 0);
