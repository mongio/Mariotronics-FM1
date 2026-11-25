/* From Mario Texture VIRGO, GPL-3.0-only. */
class Mariotronics extends AudioWorkletProcessor {
  constructor() { super(); this.e = null; this.pos = 256; this.frames = 0; this.buf = null; this.last = -1; this.gen = -1; this.saves = -1; this.port.onmessage = (ev) => this.msg(ev.data); }
  async msg(d) {
    if (d.type === 'wasm') {
      const { instance } = await WebAssembly.instantiate(d.bytes, {});
      const e = instance.exports;
      if (d.bank?.length === e.store_len()) new Uint8Array(e.memory.buffer,e.store(),e.store_len()).set(d.bank);
      if (!d.bank && [0,1,2].includes(d.light)) new Uint8Array(e.memory.buffer,e.store(),5).set([1,1,0,0,d.light]);
      const u = new Uint8Array(e.memory.buffer, e.ubank_buf(), e.ubank_size()); u.fill(0xFF); if (d.ubank) u.set(d.ubank.subarray(0, u.length));
      e.init();
      this.saves = new Uint8Array(e.memory.buffer, e.store(), 4)[3];
      this.e = e;
      this.port.postMessage({ type: 'ready' });
      return;
    }
    const e = this.e;
    if (!e) return;
    if(d.type==='param' && d.page===e.page())e.set_param(d.k,d.v);
    else if (d.type === 'keys') e.keys(d.mask >>> 0);
    else if (d.type === 'button') e.button(d.id, d.down ? 1 : 0);
    else if (d.type === 'enc') e.enc(d.id, d.d);
    else if (d.type === 'midi') e.midi(d.st, d.d1, d.d2);
    else if (d.type === 'master') e.master(d.q15);
    else if (d.type === 'ubank') { const u = new Uint8Array(e.memory.buffer, e.ubank_buf(), e.ubank_size()); u.fill(0xFF); u.set(d.ubank.subarray(0, u.length)); e.bank_changed(); }
  }
  process(_in, outs) {
    const o = outs[0], L = o[0], R = o[1] || o[0], e = this.e;
    if (!e) return true;
    for (let i = 0; i < L.length; i++) {
      if (this.pos >= 256) { this.frames += 256; e.step(Math.floor(this.frames * 1000 / 44100) >>> 0); let packet;while((packet=e.take_note_out()))this.port.postMessage({type:'noteOut',packet}); this.buf = new Int32Array(e.memory.buffer, e.audio(), 512); this.pos = 0; }
      L[i] = this.buf[2 * this.pos] / 32768; R[i] = this.buf[2 * this.pos + 1] / 32768; this.pos++;
    }
    if (currentTime - this.last > 0.033) {
      this.last = currentTime;
      const g = e.fb_gen(), msg = { type: 'frame', leds: new Uint8Array(e.memory.buffer, e.leds(), 64).slice(), state: e.state() >>> 0, page: e.page(), voice: e.voice(), params: [0,1,2,3].map(k=>e.param(k)) };
      if (g !== this.gen) { this.gen = g; msg.fb = new Uint16Array(e.memory.buffer, e.fb(), 240 * 240).slice(); }
      const st = new Uint8Array(e.memory.buffer, e.store(), e.store_len());
      if (st[3] !== this.saves) { this.saves = st[3]; msg.light = st[4]; msg.bank=st.slice(); }
      this.port.postMessage(msg, msg.fb ? [msg.fb.buffer] : []);
    }
    return true;
  }
}
registerProcessor('mariotronics', Mariotronics);
