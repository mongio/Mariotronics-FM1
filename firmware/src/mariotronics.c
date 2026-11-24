#include <stdint.h>
/* SPDX-License-Identifier: GPL-3.0-only
 * Mario Texture's Mariotronics. FM-1 host/platform from VIRGO, GRANA and
 * Felucca. Guitar: fixed-point adaptation of De-Pattern/Ravel_FM1 GUITAR
 * (rv_synth.c). Triangle pluck, lossy fractional delay and a second pickup tap.
 */
#define APP_NAME "MARIOTRONICS"
#define CV_MAX (240u * 146u)
#include "mt_art.h"
#include "platform.c"
#include "mt_ble.h"
#include "mt_intro.h"

#ifndef RING_PUBLISH
#define RING_PUBLISH()                                                         \
  do {                                                                         \
  } while (0)
#endif
#define TAPE_RATE 14700u
#define CAPACITY (8u * TAPE_RATE)
#define STRING_LEN 2048
#if defined(GRANA_WEB) || defined(DINGHY_HOST)
#define POOLMEM
#else
#define POOLMEM __attribute__((section(".pool")))
#endif
static int16_t tape[CAPACITY] POOLMEM;
enum { STOP, RECORD, DUB, HOLD };
enum { SINGING = 4, HOLLOW = 5 };
enum { HOME, SOUND, ART, FUZZ, SCALE, WAH, AMP, PLATE, ENV, COMP, LFO, MIDI, BT, ARP, OFF_TAPE, LIGHTS, CHARACTER, PAGE_COUNT };
static const char *const pages[] = {"TAPE", "GUITAR", "PHRASING", "FUZZ", "SCALES", "WAH", "AMP", "PLATE", "ENV","COMP","LFO","MIDI","BLE MIDI","ARP","OFF-TAPE","LIGHTS","TAPE CHAR"};
static const char *const names[][4] = {{"SWELL", "FDBK", "SEND", "RETURN"},
                                       {"VOICE", "DRIVE", "TAPE", "MODE"},
                                       {"GLISS", "MS", "CENT", "HZ"}, {"DRIVE","TONE","BIAS","MIX"}, {"MODE","ROOT"}, {"SWEEP","RES","TONE","MIX"}, {"GAIN","BASS","TREBLE","VOL"}, {"MIX","DECAY","DAMP","PRE ms"}, {"A ms","D ms","S %","R ms"},{"THRESH","RATIO","REL ms","GAIN"},{"HZ","CENT","WAIT ms","RISE ms"},{"RX CH","CC IN","SYNC","BEATS"},{"RADIO","DEVICE"},{"PLAY","HZ","GATE %","ACCENT"},{"LEVEL %"},{"KEYS"},{"HF LOSS","SAT","WOW","FLUTTER"}};
static volatile int knob[][4] = {
    {38, 81, 80, 72}, {0, 55, 3, 0}, {1, 24, 22, 5}, {54,45,18,42}, {0,6}, {40,52,60,0}, {32,48,42,85}, {19,72,52,18}, {38,32,84,36},{46,2,40,12},{5,22,12,20},{0,1,0,8},{0,0},{0,9,72,38},{25},{2},{32,0,0,0}};
static const int maximum[][4] = {{100, 100, 100, 100},
                                 {1, 100, 7, 1},
                                 
                                 {1, 100, 80, 9}, {100, 100, 100, 100}, {2, 11},{100,100,100,100},{100,100,100,100},{100,100,100,80},{100,100,100,100},{100,8,100,100},{9,80,100,100},{16,1,1,16},{1,8},{1,16,95,100},{100},{2},{100,100,100,100}};
static const int minimum[][4] = {{0}, {0}, {0, 1, 0, 2}, {0}, {0},{0},{0},{0},{0},{0,1,0,0},{2,0,0,0},{0,0,0,1},{0},{0,3,20,0},{0},{0},{0}};
static const char *const voices[] = {"SING", "HOLLOW"};
static const char *const mode_names[] = {"CHROM", "DORIAN", "MAJOR"};
static const char *const tonic_names[] = {"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
static int scale_note(int note) {
  static const unsigned masks[] = {0xfff, 0x6ad, 0xab5};
  unsigned mask = masks[knob[SCALE][0]];
  int root = knob[SCALE][1];
  /* Nearest scale tone, preferring the lower neighbour in a tie. */
  for (int delta = 0; delta <= 2; delta++) {
    int low = note - delta, high = note + delta;
    if (low >= 0 && (mask & (1u << ((low + 12 - root) % 12)))) return low;
    if (high <= 127 && (mask & (1u << ((high + 12 - root) % 12)))) return high;
  }
  return note;
}
static int page, octave;
static volatile int vib_on = 1;
static volatile unsigned transport, position, length = TAPE_RATE * 4, has_loop,
                                              turnclock;
static volatile int vu_in, vu_out;
static unsigned valid_samples;
static unsigned random_state = 14701;
static int bound(int x, int lo, int hi) {
  return x < lo ? lo : x > hi ? hi : x;
}
static int absolute(int x) { return x < 0 ? -x : x; }
static int rnd(void) {
  random_state = random_state * 1664525u + 1013904223u;
  return (int)(random_state >> 16) - 32768;
}

typedef struct {
  unsigned phase, increment, vibrato;
  int bloom, strength, neck, pitch, start_pitch, target_pitch, glide_ticks, glide_at, pitch_clock;
  int16_t line[STRING_LEN];
  int note, gate, active, kind, at, delay, lp, level, attack, age;
  unsigned lfo_age;
  int env_q8, decay_step, sustain, release_step, release_length, peaked;
} String;
static String strings[4];
static unsigned char held_notes[128], held_counts[128];
static int held_size;
static unsigned serial;
static void strike(int slot, int note, int velocity, int kind) {
  String *s = &strings[slot];
  int i, lp = 0, lp2 = 0;
  int delay = (int)period256[bound(note, 0, 127)], n = delay >> 8;
  int corner = bound(n * (kind == 0 ? 19 : 30) / 100, 1, n - 1);
  s->phase = s->vibrato = s->lfo_age = 0;
  s->increment = (0xffffffffu / ((unsigned)delay + 64u)) * 256u;
  s->strength = 2800 + velocity * 36;
  s->bloom = s->neck = 0;
  s->pitch = s->target_pitch = note * 4096;
  s->glide_ticks = s->glide_at = s->pitch_clock = 0;
  s->at = n;
  s->delay = delay;
  s->lp = 0;
  s->kind = kind;
  s->note = note;
  s->age = ++serial;
  s->active = s->gate = 1;
  int attack_ms=2+knob[ENV][0]*knob[ENV][0]/8;
  int decay_ms=5+knob[ENV][1]*knob[ENV][1]/5;
  s->sustain=32767*knob[ENV][2]/100*256;
  s->attack=(32767*256)/(attack_ms*44);
  s->decay_step=1+(32767*256-s->sustain)/(decay_ms*44);
  s->release_length=(5+knob[ENV][3]*knob[ENV][3]/4)*44;
  s->env_q8=s->peaked=s->release_step=s->level=0;
  for (i = 0; i <= n; i++) {
    int tri = i < corner ? i * 20000 / corner : (n - i) * 20000 / (n - corner);
    int sample = (tri - 10000 + rnd() / 36) * velocity / 127;
    lp += (sample - lp) * 3 / 4;
    lp2 += (lp - lp2) * 3 / 4;
    s->line[i] = lp2;
  }
  s->line[STRING_LEN - 1] = s->line[0];
}
static void glide_to(String *s, int note) {
  s->note = note;
  s->target_pitch = note * 4096;
  s->start_pitch = s->pitch;
  /* Control ticks run once per 16 audio samples. One knob step is 5 ms. */
  s->glide_ticks = knob[ART][1] * 441 / 32;
  s->glide_at = 0;
}
static void forget_note(int note) {
  for (int k = 0; k < held_size; k++)
    if (held_notes[k] == note) {
      for (int j = k + 1; j < held_size; j++)
        held_notes[j - 1] = held_notes[j];
      held_size--;
      break;
    }
}
static void note_on(int note, int vel) {
  if (knob[ART][0]) {
    forget_note(note);
    held_notes[held_size++] = note;
    if (held_counts[note] < 255) held_counts[note]++;
    if (strings[0].active && strings[0].gate)
      glide_to(&strings[0], note);
    else
      strike(0, note, vel, SINGING+knob[SOUND][0]);
    return;
  }
  int slot = 0;
  for (int i = 0; i < 4; i++) {
    if (!strings[i].active) {
      slot = i;
      break;
    }
    if (strings[i].age < strings[slot].age)
      slot = i;
  }
  strike(slot, note, vel, SINGING+knob[SOUND][0]);
}
static void note_off(int note) {
  if (knob[ART][0]) {
    if (!held_counts[note] || --held_counts[note]) return;
    forget_note(note);
    if (strings[0].note == note) {
      if (held_size) glide_to(&strings[0], held_notes[held_size-1]);
      else strings[0].gate = 0;
    }
    return;
  }
  for (int i = 0; i < 4; i++)
    if (strings[i].active && strings[i].gate && strings[i].note == note) {
      strings[i].gate = 0;
      return;
    }
}
static int saturate(int x);
/* Interpolated harmonic excitation supplies the energy of the held lead
 * after the picked waveguide transient has decayed. Phase wraps unsigned. */
static int sinus(unsigned phase) {
  unsigned index = phase >> 24;
  int a = sine256[index], b = sine256[(index + 1u) & 255u];
  return a + (b - a) * (int)((phase >> 16) & 255u) / 256;
}
static int singing_pickup(String *s, int pluck, int drive) {
  if (s->gate && s->bloom < 24576)
    s->bloom++;
  s->phase += s->increment;
  int tone = sinus(s->phase) * 96 + sinus(s->phase * 2u) * 21
           + sinus(s->phase * 3u) * (6 + (s->bloom >> 11));
  if (s->kind==HOLLOW) tone=sinus(s->phase)*74+sinus(s->phase*3u)*36+sinus(s->phase*5u)*17;
  int held = ((tone * s->strength) >> 14) * s->bloom / 24576;
  int input = (s->kind==HOLLOW ? pluck : pluck/2) + held;
  int pre = input * (3 + drive / 9);
  int clipped = saturate(pre + absolute(pre) / 12);
  s->neck += (clipped - s->neck) / 4;
  return s->neck * 2;
}
static void update_pitch(String *s) {
  if (s->glide_at < s->glide_ticks) {
    s->glide_at++;
    s->pitch = s->start_pitch + (s->target_pitch - s->start_pitch) * s->glide_at / s->glide_ticks;
  }
  s->vibrato += (unsigned)knob[ART][3] * 1558265u;
  unsigned wait=knob[LFO][2]*441u/16u, rise=knob[LFO][3]*441u/16u;
  if (s->lfo_age < 65535) s->lfo_age++;
  unsigned elapsed=s->lfo_age>wait?s->lfo_age-wait:0;
  int depth=!elapsed?0:rise && elapsed<rise?(int)(knob[LFO][1]*elapsed/rise):knob[LFO][1];
  int offset = vib_on ? sinus(s->vibrato) * depth * 4096 / 12700 : 0;
  int pitch = bound(s->pitch + offset, 0, 127 * 4096);
  int note = pitch >> 12, fraction = pitch & 4095;
  int delay = period256[note];
  if (fraction) delay += ((int)period256[note+1] - delay) * fraction / 4096;
  s->delay = delay;
  s->increment = (0xffffffffu / ((unsigned)delay + 64u)) * 256u;
}
static int string_tick(String *s) {
  if (!s->active)
    return 0;
  if ((s->pitch_clock++ & 15) == 0) update_pitch(s);
  int len = s->delay >> 8, fr = s->delay & 255, r = (s->at - len) & 2047;
  int a = s->line[r], v = a + ((s->line[(r - 1) & 2047] - a) * fr >> 8);
  s->lp += (v - s->lp) * 3 / 4;
  int y = s->lp;
  if (!s->gate)
    y = y * 996 / 1024;
  else if (s->kind == 1 || s->kind == 2)
    y += bound(y / 1024, -4, 4);
  else
    y = y * 1022 / 1024;
  s->line[s->at] = (int16_t)bound(y, -26000, 26000);
  int pickup =
      s->line[(s->at - (len * (s->kind == 0 ? 9 : 22) / 100 + 1)) & 2047];
  s->at = (s->at + 1) & 2047;
  if (!s->gate) {
    if (!s->release_step) s->release_step=1+s->env_q8/s->release_length;
    s->env_q8-=s->release_step;
    if(s->env_q8<=0) { s->active=0; return 0; }
  } else if(!s->peaked) {
    s->env_q8+=s->attack;
    if(s->env_q8>=32767*256) { s->env_q8=32767*256; s->peaked=1; }
  } else if(s->env_q8>s->sustain) {
    s->env_q8=bound(s->env_q8-s->decay_step,s->sustain,32767*256);
  }
  s->level=s->env_q8>>8;
  int picked = y - pickup * 3 / 5;
  if (s->kind >= SINGING)
    picked = singing_pickup(s, picked, knob[SOUND][1]);
  return (picked * s->level) >> 15;
}
static int dec_sum, dec_n, prev_return, next_return, tape_lp, amp_lp;
/* First A77 playback head, 180 ms downstream of the record head. */
#define HEAD_SAMPLES (TAPE_RATE * 180u / 1000u)
static int16_t head_history[HEAD_SAMPLES] POOLMEM;
static unsigned head_index, head_count;
static int head_previous, head_filtered;

/* Short fractional delay on playback models speed variations without moving
 * the record cursor. Its history is valid even when modulation is bypassed. */
static int16_t play_history[128];
static unsigned play_head, play_valid, wow_phase, flutter_phase;
static int wow_depth, flutter_depth;

static int read_transport(int sample, int wow, int flutter) {
  play_history[play_head]=sample;
  if (play_valid<128) play_valid++;
  wow_phase+=201600u;       /* 0.69 Hz at tape rate */
  flutter_phase+=2377600u;  /* 8.14 Hz at tape rate */
  /* Slew depth when turning an encoder, including transitions to bypass. */
  wow_depth+=bound(wow*64-wow_depth,-4,4);
  flutter_depth+=bound(flutter*64-flutter_depth,-4,4);
  if(wow_depth || flutter_depth) {
    int delay_q8=((128+sinus(wow_phase))*wow_depth*3)/256
                +((128+sinus(flutter_phase))*flutter_depth)/768;
    unsigned whole=(unsigned)delay_q8>>8;
    int a=whole<play_valid?play_history[(play_head-whole)&127u]:0;
    int b=whole+1<play_valid?play_history[(play_head-whole-1)&127u]:0;
    sample=a+(b-a)*(delay_q8&255)/256;
  }
  play_head=(play_head+1)&127u;
  return sample;
}
static int tape_record_level(int sample,int saturation) {
  int knee=30000-saturation*250;
  int magnitude=absolute(sample);
  if(magnitude<=knee) return sample;
  int room=30000-knee;
  int excess=magnitude-knee;
  int shaped=knee+room*excess/(room+excess);
  return sample<0?-shaped:shaped;
}
static void reset_head(void) {
  play_valid=0;
  head_index = head_count = 0;
  head_previous = head_filtered = 0;
}

static void advance_head(int recorded) {
  int sample = head_count == HEAD_SAMPLES ? head_history[head_index] : 0;
  if (head_count < HEAD_SAMPLES) head_count++;
  head_history[head_index] = recorded;
  if (++head_index == HEAD_SAMPLES) head_index = 0;
  head_previous = head_filtered;
  head_filtered += (sample - head_filtered) * 3 / 4;
}
static volatile unsigned reverse, loop_mode, beat_ms;
static unsigned beat_length(void) {return beat_ms && knob[MIDI][2] ? (unsigned)bound((int)(beat_ms*knob[MIDI][3]*147u/10u),TAPE_RATE/4,CAPACITY) : (knob[SOUND][2]+1)*TAPE_RATE;}
static void rec_command(void) {
  reverse = 0;
  if (transport == STOP && !has_loop) {
    position = 0;
    dec_n = dec_sum = prev_return = next_return = tape_lp = 0;
    loop_mode = knob[SOUND][3];
    length = loop_mode ? CAPACITY : (knob[SOUND][2] + 1) * TAPE_RATE;
    transport = RECORD;
  } else if (transport == RECORD) {
    if (loop_mode) {
      if (position < TAPE_RATE / 12)
        return;
      length = position;
      position = 0;
    }
    has_loop = 1;
    transport = DUB;
  } else {
    transport = transport == DUB ? HOLD : DUB;
  }
}
static void play_command(void) {
  if (transport == RECORD) {
    rec_command();
    if (transport == DUB)
      transport = HOLD;
  } else if (transport != STOP) {
    transport = STOP;
    reset_head();
  }
  else if (has_loop)
    transport = HOLD;
}
static void erase_loop(void) {
  reset_head();
  reverse = loop_mode = 0;
  valid_samples = 0;
  transport = STOP;
  has_loop = 0;
  position = 0;
  next_return = prev_return = tape_lp = dec_sum = dec_n = 0;
}
static int saturate(int x) {
  int a = absolute(x);
  int y = 8192 - 67108864 / (8192 + a);
  return x < 0 ? -y : y;
}
#include "mt_plate.h"
static int wah_low,wah_band,wah_tone,fuzz_tone,amp_bass,amp_cab;
static int comp_envelope,comp_gain=4096;
static unsigned comp_clock;
static int guitar_fx(int sample) {
  int peak=absolute(sample);
  if(peak>=comp_envelope) comp_envelope=peak;
  else if(!(comp_clock&15u)) comp_envelope-=1+(comp_envelope-peak)/(14+knob[COMP][2]*3);
  if(!(comp_clock++&15u)) {
    int threshold=1500+knob[COMP][0]*175;
    int ratio=knob[COMP][1];
    comp_gain=comp_envelope>threshold ?
      (threshold+(comp_envelope-threshold)/ratio)*4096/comp_envelope : 4096;
  }
  sample=((sample*comp_gain)/4096)*(100+knob[COMP][3])/100;
  int gain=2+knob[FUZZ][0]/4;
  int bias=knob[FUZZ][2]*32;
  int clipped=(saturate(sample*gain+bias)-saturate(bias))*2;
  fuzz_tone+=(clipped-fuzz_tone)*(9+knob[FUZZ][1])/128;
  sample+=(fuzz_tone-sample)*knob[FUZZ][3]/100;
  int pos=knob[WAH][0],freq=700+41*pos*pos/100;
  wah_low=bound(wah_low+freq*wah_band/16384,-48000,48000);
  int high=sample-wah_low-wah_band*(125-knob[WAH][1])/128;
  wah_band=bound(wah_band+freq*high/16384,-48000,48000);
  wah_tone+=(wah_band-wah_tone)*(25+knob[WAH][2])/128;
  sample+=(wah_tone-sample)*knob[WAH][3]/100;
  amp_bass+=(sample-amp_bass)/36;
  sample+=amp_bass*(knob[AMP][1]-50)/70;
  int driven=2*saturate(sample*(2+knob[AMP][0]/12));
  sample+=(driven-sample)*knob[AMP][0]/100;
  amp_cab+=(sample-amp_cab)*(16+knob[AMP][2])/128;
  return amp_cab*knob[AMP][3]/100;
}
/* Keep the played notes separate from the voice currently picked by the phrase. */
static unsigned char phrase_keys[128], phrase_count[128], phrase_velocity[128];
static int phrase_size, phrase_enabled, phrase_step, phrase_left, phrase_gate;
static void phrase_reset(void) {
  for(int i=0;i<4;i++) strings[i].gate=0;
  held_size=0;
  for(int i=0;i<128;i++) held_counts[i]=0;
  phrase_step=phrase_left=phrase_gate=0;
}
static void phrase_event(int note,int velocity) {
  if(velocity) {
    if(!phrase_count[note])phrase_keys[phrase_size++]=note;
    if(phrase_count[note]<255)phrase_count[note]++;
    phrase_velocity[note]=velocity;
  } else {
    if(!phrase_count[note])return;
    if(!--phrase_count[note]){
      int i=0;while(i<phrase_size && phrase_keys[i]!=note)i++;
      if(i<phrase_size){for(;i+1<phrase_size;i++)phrase_keys[i]=phrase_keys[i+1];phrase_size--;}
    }
  }
  if(!phrase_enabled){
    if(velocity)note_on(note,velocity);else note_off(note);
  } else {
    phrase_reset();
    if(phrase_size==1){
      int n=phrase_keys[0];note_on(n,phrase_velocity[n]);
    }
  }
}
static void phrase_tick(void) {
  if(phrase_size<2)return;
  if(!phrase_left){
    int n=phrase_keys[phrase_size-2+(phrase_step&1)];
    int accented=phrase_step==0 || phrase_step==3 || phrase_step==6;
    int v=phrase_velocity[n]*(accented?100:100-knob[ARP][3]*3/5)/100;
    strike(0,n,v?v:1,SINGING+knob[SOUND][0]);
    /* A picked transient must fit inside a fast gate, even with long manual swells. */
    if(strings[0].attack<8388352/(44*6))strings[0].attack=8388352/(44*6);
    if(strings[0].release_length>1323)strings[0].release_length=1323;
    phrase_left=44100/knob[ARP][1];
    phrase_gate=phrase_left*knob[ARP][2]/100;
    phrase_step=(phrase_step+1)&7;
  }
  if(phrase_gate && !--phrase_gate)strings[0].gate=0;
  phrase_left--;
}

static void apply_events(void);
static volatile unsigned audio_ready;
static void app_audio(int32_t *out, uint32_t n) {
  if (!audio_ready) return; /* Platform supplies a cleared buffer during startup. */
  apply_events();
  if (!loop_mode) {
    unsigned requested = beat_length();
    if (requested != length) {
      length = requested;
      if (position >= length) position %= length;
      if (valid_samples > length) valid_samples = length;
    }
  }
  int peak_in = 0, peak_out = 0;
  int decay=100+knob[PLATE][1]*148/100;
  int damping=235-knob[PLATE][2]*215/100;
  int predelay=knob[PLATE][3]*2205/100, wet=knob[PLATE][0];
  int hf_coefficient=256-2*knob[CHARACTER][0], saturation=knob[CHARACTER][1];
  int wow=knob[CHARACTER][2], flutter=knob[CHARACTER][3];
  for (unsigned i = 0; i < n; i++) {
    if(phrase_enabled)phrase_tick();
    int dry = 0;
    for (int j = 0; j < 4; j++)
      dry += string_tick(&strings[j]);
    int shape = SINGING+knob[SOUND][0], drive = knob[SOUND][1];
    int amp = shape >= SINGING ? dry : saturate(dry * (shape == 0 ? 1 + drive / 35 : 4 + drive / 9)) * 2;
    amp_lp += (amp - amp_lp) * (shape >= SINGING ? 2 : shape == 3 ? 5 : 3) / 8;
    dry = guitar_fx(amp_lp);
    int send = dry * knob[HOME][2] / 100, ret = 0;
    if (transport != STOP) {
      ret = prev_return + (next_return - prev_return) * dec_n / 3;
      dec_sum += send;
      if (++dec_n == 3) {
        dec_n = 0;
        int old = position >= valid_samples ? 0 : tape[position];
        old=read_transport(old,wow,flutter);
        prev_return = next_return;
        next_return = old;
        if (transport != HOLD) {
          tape_lp += (old - tape_lp) * hf_coefficient / 256;
          int write = dec_sum / 3 + tape_lp * knob[HOME][1] / 100;
          tape[position] = (int16_t)tape_record_level(write,saturation);
          advance_head(tape[position]);
          if (position == valid_samples)
            valid_samples++;
        } else {
          advance_head(0);
        }
        dec_sum = 0;
        if (reverse && transport == HOLD) {
          position = position ? position - 1 : length - 1;
        } else if (++position >= length) {
          position = 0;
          has_loop = 1;
          if (transport == RECORD)
            transport = DUB;
        }
        if (reverse && transport == HOLD) turnclock = turnclock ? turnclock - 1 : 827999;
        else if (++turnclock == 828000) turnclock = 0;
      }
      if (transport != HOLD && absolute(send) > peak_in)
        peak_in = absolute(send);
    }
    if (absolute(ret) > peak_out)
      peak_out = absolute(ret);
    int head = head_previous + (head_filtered - head_previous) * dec_n / 3;
    int mix = dry + ret * knob[HOME][3] / 100 + head * knob[OFF_TAPE][0] / 100;
    if (plate_half) plate_tick((plate_input+mix)/4,decay,damping,predelay);
    plate_input=mix;
    int l=plate_half?(wet_l+prev_l)/2:wet_l;
    int r=plate_half?(wet_r+prev_r)/2:wet_r;
    plate_half^=1;
    out[2*i]=bound(mix+l*wet/65,-31500,31500);
    out[2*i+1]=bound(mix+r*wet/65,-31500,31500);
  }
  vu_in = peak_in > vu_in ? peak_in : vu_in * 9 / 10;
  vu_out = peak_out > vu_out ? peak_out : vu_out * 9 / 10;
}
static const uint16_t silver = RGB(191, 192, 179), metal = RGB(124, 128, 122),
                      ink = RGB(22, 25, 22), gold = RGB(247, 187, 88);
static void disc(int x, int y, int r, uint16_t color) {
  for (int dy = -r; dy <= r; dy++) {
    int dx = r;
    while (dx * dx + dy * dy > r * r)
      dx--;
    cv_rect(x - dx, y + dy, 2 * dx + 1, 1, color);
  }
}

static void number(char *s, int n) {
  int k = 0;
  if (n >= 100)
    s[k++] = '0' + n / 100;
  if (n >= 10)
    s[k++] = '0' + n / 10 % 10;
  s[k++] = '0' + n % 10;
  s[k] = 0;
}
static const char *value_text(int k, char *s) {
  int v = knob[page][k];
  if(page==LIGHTS) { static const char *const levels[]={"OFF","HALF","FULL"}; return levels[v]; }
  if(page==ARP && k==0)return v?"ON":"OFF";
  if(page==MIDI && k==0 && !v) return "OMNI";
  if((page==MIDI && (k==1||k==2))||(page==BT && k==0)) return v?"ON":"OFF";
  if(page==LFO && k>=2) {v*=10;if(v==1000)return "1000";}
  if (page == SOUND) {
    if (k == 0)
      return voices[v];
    if (k == 3)
      return v ? "LOOP" : "TAPE";
    if (k == 2)
      v++;
  }
  if (page == SCALE) return k == 0 ? mode_names[v] : tonic_names[v];
  if (page == ART && k == 0) return v ? "ON" : "OFF";
  if (page == ART && k == 1) v *= 5;
  if(page==COMP && k==2) v=(14+3*v)*160/441;
  if(page==ENV && k!=2) {
    int ms = k==0 ? 2+v*v/8 : k==1 ? 5+v*v/5 : 5+v*v/4;
    if(ms>=1000) {
      s[0]='0'+ms/1000; s[1]='0'+(ms/100)%10; s[2]='0'+(ms/10)%10; s[3]='0'+ms%10; s[4]=0;
    } else number(s,ms);
    return s;
  }
  number(s, v);
  return s;
}
/* Antialiased UI glyphs on a solid background, centered at the given x. */
static void lettering(int x, int y, const char *s, uint16_t fg, uint16_t bg) {
  int width = 0;
  for (const char *p = s; *p; p++) width += font_width[*p - 32];
  x -= width / 2;
  for (; *s; s++) {
    int glyph = *s - 32;
    const uint8_t *alpha = font_alpha + glyph * 270;
    for (int j = 0; j < 18; j++) {
      for (int k = 0; k < 15; k++) {
        int shade = alpha[j * 15 + k];
        if (!shade || x+k < 0 || x+k >= 240 || y+j >= (int)cv_h) continue;
        int back = 255 - shade;
        int r = ((fg >> 11) * shade + (bg >> 11) * back + 127) / 255;
        int g = (((fg >> 5) & 63) * shade + ((bg >> 5) & 63) * back + 127) / 255;
        int b = ((fg & 31) * shade + (bg & 31) * back + 127) / 255;
        cv_px[(y+j)*240+x+k] = swap16((r<<11)|(g<<5)|b);
      }
    }
    x += font_width[glyph];
  }
}
static void effect_display(void) {
  int a=knob[page][0], b=knob[page][1], c=knob[page][2], d=knob[page][3];
  const char *caption=page==ENV?"ENVELOPE":page==PLATE?"STEREO  /  POST TAPE":"GUITAR  /  PRE TAPE";
  lettering(120,4,caption,silver,0);
  for(int x=20;x<=220;x+=40) cv_rect(x,30,1,90,RGB(28,36,34));
  for(int y=30;y<=120;y+=30) cv_rect(20,y,201,1,RGB(28,36,34));
  if(page==ENV) {
    int x1=23+a/2, x2=x1+8+b/3, x3=210-d/2;
    int sy=116-78*c/100;
    cv_line(20,116,x1,32,gold);cv_line(x1,32,x2,sy,gold);
    cv_line(x2,sy,x3,sy,gold);cv_line(x3,sy,221,116,gold);
    disc(x1,32,2,silver);disc(x2,sy,2,silver);disc(x3,sy,2,silver);
  } else {
    int previous=0;
    for(int i=0;i<=200;i++) {
      int v;
      if(page==PLATE) {
        int time=i-d/3, span=25+b;
        v=time<0?0:((span*42/(span+time*2))*(100-c*i/400)/100)*(60+40*sinus((unsigned)time*39876821u)/127)/100;
        v=v*a/100;
        cv_rect(20+i,76-v,1,1+2*v,RGB(124,162,152));
        continue;
      } else if(page==FUZZ) {
        int sample=(i-100)*140;
        int clip=(saturate(sample*(2+a/4)+c*32)-saturate(c*32))*2;
        v=76-(sample+(clip-sample)*d/100)/360;
      } else if(page==WAH) {
        int delta=absolute(i-(18+a*16/10));
        v=94-d*(20+b/2)/(100+delta*(2+b/18));
      } else if(page==COMP) {
        int knee=18+a;
        v=115-(i<=knee?i:knee+(i-knee)/b)*75/200-d/8;
      } else {
        v=75-(b-50)*(200-i)/350+(50-c)*i/300
             -a*(100-absolute(i-100))/450;
      }
      if(i)cv_line(19+i,previous,20+i,v,gold);
      previous=v;
    }
  }
  lettering(120,124,page==PLATE?"PLATE":page==AMP?"AMP / CAB":page==WAH?"WAH":page==ENV?"A    D    S    R":page==COMP?"COMPRESSOR":"FUZZ",gold,0);
}
#include "mt_presets.h"
static unsigned intro_start;
static int intro;
enum { INTRO_END_MS = 5600 };
static unsigned intro_ease(unsigned elapsed,unsigned duration) {
  if(elapsed>=duration)return 255;
  unsigned x=elapsed*255/duration;
  return x*x*(765-2*x)/65025;
}
static void boot_draw(unsigned ms) {
  unsigned brightness=intro_ease(ms,650)*(255-intro_ease(ms>5000?ms-5000:0,550))/255;
  unsigned title=brightness*intro_ease(ms>3500?ms-3500:0,550)/255;
  uint16_t colors[128],letters[128];
  for(unsigned i=0;i<128;i++){
    const unsigned char *c=intro_palette+3*i;
    colors[i]=swap16(RGB(c[0]*brightness/255,c[1]*brightness/255,c[2]*brightness/255));
    letters[i]=swap16(RGB(c[0]*title/255,c[1]*title/255,c[2]*title/255));
  }
  cv_begin(240,52,0);cv_blit(0,0);
  cv_begin(240,136,0);
  for(unsigned i=0;i<240*136;i++)cv_px[i]=colors[intro_art[i]];
  /* Closed return path in local LCD coordinates, clockwise from the record head. */
  const int route[][2]={{75,79},{170,79},{172,77},{172,10},{170,8},{78,8},{75,11},{75,79}};
  enum { PATH_LENGTH=95+4+67+4+92+6+68 };
  unsigned clock=ms<4500?ms:4500;
  int head=(clock%2250)*PATH_LENGTH/2250;
  for(int trail=5;trail>=0;trail--){
    int pos=(head+PATH_LENGTH-trail)%PATH_LENGTH;
    for(unsigned i=1;i<sizeof route/sizeof route[0];i++){
      int dx=route[i][0]-route[i-1][0],dy=route[i][1]-route[i-1][1];
      int len=absolute(dx)+absolute(dy);
      if(pos<len){
        int x=route[i-1][0]+dx*pos/len,y=route[i-1][1]+dy*pos/len;
        disc(x,y,trail?1:2,RGB((255-20*trail)*brightness/255,(231-20*trail)*brightness/255,66*brightness/255));
        break;
      }
      pos-=len;
    }
  }
  cv_blit(0,52);
  cv_begin(240,52,0);
  for(unsigned i=0;i<240*22;i++)cv_px[i]=letters[intro_art[240*136+i]];
  cv_blit(0,188);
}

static void draw(void) {
  if(intro && fm1_ms-intro_start<INTRO_END_MS){boot_draw(fm1_ms-intro_start);return;}intro=0;
  static const char *const modes[] = {"STOP", "REC", "DUB", "HOLD"};
  unsigned t = transport;
  cv_begin(240, 146, 0);
  if(page==FUZZ || page==WAH || page==AMP || page==PLATE || page==ENV || page==COMP) effect_display();
  else if(page==OFF_TAPE) {
    lettering(120,12,"OFF-TAPE MONITOR",gold,0);
    cv_line(40,95,200,95,silver);
    cv_line(67,95,67,47,silver);
    cv_line(167,95,167,95-knob[OFF_TAPE][0]*48/100,gold);
    lettering(67,103,"DRY",silver,0);
    lettering(167,103,"180 ms",gold,0);
  } else if(page==ARP) {
    lettering(120,16,"TWO-NOTE PICKING",gold,0);
    for(int i=0;i<8;i++){
      int x=22+i*28,y=(i&1)?62:84;
      int accent=i==0||i==3||i==6;
      cv_rect(x-6,y,12,accent?6:3,accent?gold:silver);
      if(phrase_enabled && phrase_size>=2 && ((phrase_step+7)&7)==i)disc(x,105,3,gold);
    }
    lettering(120,120,"3 + 3 + 2",silver,0);
  } else if(page==CHARACTER) {
    lettering(120,28,"TAPE CHARACTER",gold,0);
    lettering(120,72,"RECORD / PLAYBACK",silver,0);
  } else if(page==LIGHTS) {
    lettering(120,38,"KEY BACKLIGHT",gold,0);
    lettering(120,82,"OFF     HALF     FULL",silver,0);
  } else if(page==BT) {
#if !defined(GRANA_WEB) && !defined(DINGHY_HOST)
    char name[22];const char *peer=radio_name(knob[BT][1]);unsigned j=0;
    while(j<20 && peer[j]) {unsigned ch=(unsigned char)peer[j];name[j++]=(ch>=32 && ch<=126)?ch:'?';}name[j]=0;
    lettering(120,27,bt_status,gold,0);lettering(120,65,name,silver,0);
    lettering(120,104,"OCT+ TO CONFIRM",silver,0);
#else
    lettering(120,42,"SYSTEM MIDI PORTS",gold,0);
    lettering(120,79,"USE MIDI INPUT BELOW",silver,0);
#endif
  } else if(page==MIDI) {
    lettering(120,26,"MIDI INPUT",gold,0);
    lettering(120,64,knob[MIDI][2]?(beat_ms?"CLOCK LOCKED":"WAITING FOR CLOCK"):"INTERNAL TAPE TIME",silver,0);
    lettering(120,103,"GLO  /  BLUETOOTH",silver,0);
  } else if(page==LFO) {
    lettering(120,7,"PITCH VIBRATO",gold,0);
    int prev=76;
    for(int i=0;i<200;i++) {
      int t=2*i-knob[LFO][2],rise=knob[LFO][3];
      int depth=t<=0?0:rise && t<rise?knob[LFO][1]*t/rise:knob[LFO][1];
      int y=76-sinus((unsigned)i*knob[LFO][0]*4300000u)*depth/300;
      if(i)cv_line(19+i,prev,20+i,y,gold);prev=y;
    }
    lettering(120,118,vib_on?"VIBRATO ON":"VIBRATO OFF",silver,0);
  } else {
  static int gap;
  int seconds = loop_mode && (has_loop || t != STOP) ?
      (int)((length + TAPE_RATE/2) / TAPE_RATE) : knob[SOUND][2] + 1;
  gap += bound(bound((seconds-4)*3,0,12)-gap,-2,2);
  int start[2] = {15-gap,121+gap};
  unsigned clock = turnclock;
  for (int i = 0; i < 2; i++) {
    const uint8_t *cab = i ? cabinet1 : cabinet0;
    const uint8_t *wheel = (i ? flange1 : flange0)
       + ((clock / (i ? 1000u : 1150u)) % 36u)*4356u;
    for (int y = 0; y < 120; y++) {
      for (int x = 0; x < 104; x++) {
        int u = x - (i ? 0 : 38);
        unsigned v = y < 66 && (unsigned)u < 66 ? wheel[y*66+u] : cab[y*104+x];
        if (v) cv_px[(y+3)*240+start[i]+x] = grey565[v];
      }
    }
    lettering(start[i]+52,124,i ? "PLAY" : "REC",i || (t!=RECORD && t!=DUB) ? silver : gold,0);
  }
  cv_rect(start[0]+41, 75, start[1]+64-(start[0]+41), 1, RGB(128,115,96));
  }
  if(naming) {
    cv_begin(240,146,ink);
    char id[]={'P','0'+(destination+1)/10,'0'+(destination+1)%10,0};
    lettering(120,7,id,gold,ink);
    lettering(120,36,deleting?"DELETE THIS PRESET?":"EDIT PRESET NAME",silver,ink);
    for(int i=0;i<NAME_LEN;i++){
      char ch[2]={draft[i],0}; if(i==cursor)cv_rect(9+14*i,89,12,2,gold);
      if(draft[i])lettering(15+14*i,64,ch,silver,ink);
      else {if(i>=cursor)break;}
    }
    lettering(120,113,deleting?"SAVE: DELETE   HOME: EXIT":"SAVE: WRITE   HOME: EXIT",gold,ink);
  } else if (program_notice && fm1_ms-program_notice_time<1400u) {
    char title[NAME_LEN+1];slot_name(program,title);
    cv_rect(0,41,240,62,ink);
    lettering(120,46,program_notice,gold,ink); lettering(120,76,title,silver,ink);
  }
  cv_blit(0, 0);
  cv_begin(240, 94, ink);
  char slot_id[]={'P','0'+(program+1)/10,'0'+(program+1)%10,0};
  lettering(123,0,slot_id,silver,ink);
  if(naming){
    lettering(120,5,"SAVE",gold,ink);
    const char *labels[]={"CURSOR","LETTER","DICE","DELETE"};
    for(int k=0;k<4;k++) lettering(30+60*k,48,labels[k],silver,ink);
    cv_blit(0,146);return;
  }
  lettering(46,0,pages[page],gold,ink);
  lettering(204,0,(reverse && t == HOLD) ? "REV" : modes[t],silver,ink);
  int width = length ? position * 228u / length : 0;
  cv_rect(6, 19, 228, 2, metal);
  cv_rect(6, 19, width, 2, gold);
  for (int k = 0; k < 4; k++) {
    if ((page == OFF_TAPE || page == LIGHTS) && k > 0) break;
    if ((page == SCALE || page == BT) && k >= 2) break;
    int x = (page == SCALE || page == BT) ? 60 + 120 * k : 30 + 60 * k;
    int v = knob[page][k], lo = minimum[page][k], hi = maximum[page][k];
    int h = 29 * (v - lo) / (hi - lo);
    cv_rect(x - 4, 26, 8, 30, metal);
    cv_rect(x - 4, 56 - h, 8, h, gold);
    char val[8];
    lettering(x,55,value_text(k,val),silver,ink);
    lettering(x,73,names[page][k],gold,ink);
  }
  cv_blit(0, 146);
}
static int key_notes[27], key_channels[27];
static void keyboard_out(unsigned status,unsigned note,unsigned velocity) {
  midi_send(status,note,velocity);
#if !defined(GRANA_WEB) && !defined(DINGHY_HOST)
  bt_send(status,note,velocity);
#endif
}
static unsigned char midi_notes[2][16][128];
enum {
  EV_NOTE,
  EV_OFF,
  EV_REC,
  EV_PLAY,
  EV_CLEAR, EV_START, EV_CONTINUE, EV_STOP, EV_REVERSE,
  EV_HOLD,
  EV_PANIC
};
typedef struct {
  unsigned char type, a, b;
} Event;
static Event queue[128];
static volatile unsigned writer, reader, panic;
static void post(int type, int a, int b) {
  unsigned w = writer;
  if (w - reader >= 128) {
    panic = 1;
    return;
  }
  queue[w & 127] = (Event){type, a, b};
  RING_PUBLISH();
  writer = w + 1;
}
static void apply_events(void) {
  static int last_glide = 1;
  if (panic || last_glide != knob[ART][0]) {
    last_glide = knob[ART][0];
    held_size = 0;
    for (int j = 0; j < 128; j++) held_counts[j] = 0;
    for (int j = 0; j < 4; j++)
      strings[j].gate = 0;
    if (panic) {reader=writer;phrase_size=0;for(int i=0;i<128;i++)phrase_count[i]=0;}
    panic = 0;
  }
  if(phrase_enabled!=knob[ARP][0]){
    phrase_reset();phrase_enabled=knob[ARP][0];
    if(!phrase_enabled || phrase_size==1)
      for(int i=0;i<phrase_size;i++)note_on(phrase_keys[i],phrase_velocity[phrase_keys[i]]);
  }
  while (reader != writer) {
    Event e = queue[reader & 127];
    reader++;
    switch (e.type) {
    case EV_NOTE:
      phrase_event(e.a,e.b);
      break;
    case EV_OFF:
      phrase_event(e.a,0);
      break;
    case EV_REC:
      rec_command();
      break;
    case EV_PLAY:
      play_command();
      break;
    case EV_START: position=0;dec_n=dec_sum=prev_return=next_return=0;
      /* fall through */
    case EV_CONTINUE: if(has_loop) transport=HOLD;else if(transport==STOP)rec_command(); break;
    case EV_STOP: transport=STOP;break;
    case EV_CLEAR:
      erase_loop();
      break;
    case EV_REVERSE:
      if (has_loop) {
        reverse = !reverse;
        transport = HOLD;
      }
      break;
    case EV_HOLD:
      reverse = 0;
      if (transport == RECORD) {
        rec_command();
        if (transport == DUB)
          transport = HOLD;
      } else if (has_loop)
        transport = transport == HOLD ? DUB : HOLD;
      break;
    case EV_PANIC:
      held_size = 0;
      for (int j = 0; j < 128; j++) held_counts[j] = 0;
      for (int j = 0; j < 4; j++)
        strings[j].gate = 0;
      break;
    }
  }
}
/* VIRGO 0.6.3 idle pulses; pressed keys use the full scan-column time. */
static void set_key_backlight(int level) {
  static const uint16_t ns[] = {0, 1500, 5000};
  fm1_led_bg_ns = ns[level];
  for (unsigned k = 0; k < 27; k++) fm1_led_back(KEY_LED(k), level != 0);
}
static unsigned char globals[5]={2,32,0,0,0};
static int globals_changed;
static unsigned light_changed;
static uint16_t midi_lit[128];

static void update_leds(unsigned keys) {
  static const unsigned char page_button[PAGE_COUNT] = {
    BTN_HOME, BTN_EDIT, BTN_SEL, BTN_FX, BTN_SEL, BTN_FX, BTN_FX, BTN_FX,
    BTN_ENV, BTN_FX, BTN_LFO, BTN_GLO, BTN_GLO, BTN_ARP, BTN_FX, BTN_GLO, BTN_GLO
  };
  static unsigned previous_keys, previous_buttons;
  unsigned buttons = 1u << page_button[page];
  unsigned t = transport;
  if (vib_on) buttons |= 1u << BTN_SEQ;
  if (knob[ARP][0]) buttons |= 1u << BTN_ARP;
  if (octave < 0) buttons |= 1u << BTN_OCT_DOWN;
  if (octave > 0) buttons |= 1u << BTN_OCT_UP;
  if (naming || (reverse && t == HOLD)) buttons |= 1u << BTN_SAVE;
  if (t != STOP) buttons |= 1u << BTN_PLAY;
  static unsigned blink_start;
  static int was_recording;
  int recording = t == RECORD || t == DUB;
  if (recording && !was_recording) blink_start = fm1_ms;
  if (recording && (fm1_ms - blink_start) % 500u < 250u) buttons |= 1u << BTN_REC;
  was_recording = recording;
  for (unsigned id=0; id<14; id++)
    if ((previous_buttons ^ buttons) & (1u << id)) fm1_led_key(id, buttons & (1u << id));
  previous_buttons = buttons;
  for (unsigned k=0; k<27; k++) {
    if (midi_lit[53 + octave*12 + (int)k]) keys |= 1u << k;
    if ((previous_keys ^ keys) & (1u << k)) fm1_led_key(KEY_LED(k), keys & (1u << k));
  }
  previous_keys = keys;
}
static void app_setup(void) {
  unsigned char stored[5];
  if(!settings_load(stored,sizeof stored) && stored[0]<=2) {
    globals[0]=stored[0];
    for(int i=1;i<5;i++) if(stored[i]<=100)globals[i]=stored[i];
  } else if(!settings_load(stored,1) && stored[0]<=2) globals[0]=stored[0];
  knob[LIGHTS][0]=globals[0];set_key_backlight(globals[0]);
  for(int i=0;i<4;i++)knob[CHARACTER][i]=globals[1+i];
  intro=1;intro_start=fm1_ms;plate_init();
  RING_PUBLISH();
  programs_init();
  audio_ready=1;

  draw();
}
static void set_control(int k,int v) {
  intro=0;
  if(page==BT && k==0) {
#if !defined(GRANA_WEB) && !defined(DINGHY_HOST)
    v=radio_power(v!=0);
#else
    v=0;
#endif
  }
  if(page==LIGHTS && k==0 && knob[LIGHTS][0]!=bound(v,0,2)) {
    globals_changed=1; light_changed=fm1_ms;
    globals[0]=bound(v,0,2);
    set_key_backlight(bound(v,0,2));
  }
  if(page==CHARACTER && knob[page][k]!=bound(v,0,100)) {
    globals[1+k]=bound(v,0,100); globals_changed=1; light_changed=fm1_ms;
  }
  knob[page][k]=bound(v,minimum[page][k],maximum[page][k]);
  if(k==0 && (page==ENV || page==HOME))
    knob[page==ENV?HOME:ENV][0]=knob[page][0];
  if(page==LFO && k<2) knob[ART][3-k]=knob[LFO][k];
  if(page==ART && k>=2) knob[LFO][3-k]=knob[ART][k];

}
static unsigned last_tick,beat_start,ticks;
static void midi_input(unsigned src,unsigned st,unsigned a,unsigned b) {
  unsigned ch=st&15, cmd=st&240;
  if(st>=248) {
    if(!knob[MIDI][2])return;
    if(st==248) {
      if(!last_tick || fm1_ms-last_tick>500){ticks=0;beat_start=fm1_ms;}
      last_tick=fm1_ms;
      if(++ticks==25){unsigned dt=fm1_ms-beat_start;if(dt>=120 && dt<=3000)beat_ms=dt;beat_start=fm1_ms;ticks=1;}
    } else if(st==250){ticks=0;last_tick=0;post(EV_START,0,0);}
    else if(st==251)post(EV_CONTINUE,0,0);
    else if(st==252)post(EV_STOP,0,0);
    return;
  }
  if(a>=128 || b>=128)return;
  unsigned char *tracked=&midi_notes[src][ch][a];
  if(cmd==128 || (cmd==144 && !b)){if(*tracked){midi_lit[*tracked-1]--;post(EV_OFF,*tracked-1,0);*tracked=0;}return;}
  if(knob[MIDI][0] && knob[MIDI][0]!=(int)ch+1)return;
  if(cmd==144){intro=0;if(*tracked){midi_lit[*tracked-1]--;post(EV_OFF,*tracked-1,0);}
    *tracked=1+scale_note(a);midi_lit[*tracked-1]++;post(EV_NOTE,*tracked-1,b);}
  if(cmd==192 && a<PROGRAM_COUNT){intro=0;recall_program(a);}
  if(cmd!=176)return;
  if(a==120||a==123){for(int i=0;i<128;i++){unsigned char *n=&midi_notes[src][ch][i];if(*n){midi_lit[*n-1]--;post(EV_OFF,*n-1,0);*n=0;}}return;}
  if(!knob[MIDI][1])return;
  int v=(b*100+63)/127;
  switch(a){
    case 1:knob[LFO][1]=knob[ART][2]=b*80/127;break;
    case 7:knob[AMP][3]=v;break;
    case 11:knob[HOME][2]=v;break;
    case 20:knob[FUZZ][0]=v;break;
    case 21:knob[WAH][0]=v;break;
    case 22:knob[HOME][1]=v;break;
    case 23:knob[HOME][3]=v;break;
    case 91:knob[PLATE][0]=v;break;
  }
}
static void app_step(void) {
  static unsigned key_prev, last_frame, rec_started;
  static int rec_pending;
  unsigned rel = 0, edge = fm1_input_edges(&rel), keys = fm1_in.notes;
  if(edge || keys)intro=0;
  if(edge & ~(1u<<BTN_SAVE)) naming=0;
  if (page==BT && (edge & (1u << BTN_OCT_UP))) {
#if !defined(GRANA_WEB) && !defined(DINGHY_HOST)
    radio_action(knob[BT][1]);
#endif
  } else if (edge & (1u << BTN_OCT_UP))
    octave = bound(octave + 1, -2, 3);
  if (edge & (1u << BTN_OCT_DOWN))
    octave = bound(octave - 1, -2, 3);
  if (edge & (1u << BTN_HOME))
    page = HOME;
  if (edge & (1u << BTN_LFO)) page=LFO;
  if (edge & (1u << BTN_EDIT))
    page = SOUND;
  if (edge & (1u << BTN_SEL)) page = page==ART ? SCALE : ART;
  if (edge & (1u << BTN_GLO)) page = page==MIDI ? BT : page==BT ? LIGHTS : page==LIGHTS ? CHARACTER : MIDI;
  if (edge & (1u << BTN_ARP)) { knob[ARP][0] = !knob[ARP][0]; page = ARP; }
  if (edge & (1u << BTN_SEQ)) { vib_on = !vib_on; page = LFO; }
  static unsigned save_started;
  static int save_pending;
  if (edge & (1u << BTN_SAVE)) {
    if(naming){ save_program();save_pending=0; }
    else {save_started=fm1_ms;save_pending=1;}
  }
  if (save_pending && fm1_ms-save_started>=1000u) {
    start_name(program); save_pending=0;
  }
  if (save_pending && (rel & (1u << BTN_SAVE))) {
    post(EV_REVERSE,0,0); save_pending=0;
  }
  if (edge & (1u << BTN_REC)) {
    rec_started = fm1_ms;
    rec_pending = 1;
  }
  if (rec_pending && fm1_ms - rec_started >= 1200) {
    post(EV_CLEAR, 0, 0);
    rec_pending = 0;
  }
  if (rec_pending && (rel & (1u << BTN_REC))) {
    post(EV_REC, 0, 0);
    rec_pending = 0;
  }
  if (edge & (1u << BTN_PLAY))
    post(EV_PLAY, 0, 0);
  if (edge & (1u << BTN_FX))
    page = page==COMP ? FUZZ : page==FUZZ ? WAH : page==WAH ? AMP : page==AMP ? OFF_TAPE : page==OFF_TAPE ? PLATE : COMP;
  if (edge & (1u << BTN_ENV)) page = ENV;
  int d = fm1_enc_take(ENC_SELECT);
  if(d)intro=0;
  if (d) naming=0;
  if (d)
    page = ((page + d) % PAGE_COUNT + PAGE_COUNT) % PAGE_COUNT;
  d = fm1_enc_take(ENC_ALGORITHM);
  if(d)intro=0;
  if (d && !naming) knob[SOUND][2] = bound(knob[SOUND][2] + d, 0, 7);
  d = fm1_enc_take(ENC_PRESETS);
  if(d)intro=0;
  if (d)
    { if(naming)start_name(bound(destination+d,0,PROGRAM_COUNT-1));
      else recall_program(bound(program+d,0,PROGRAM_COUNT-1)); }
  for (int k = 0; k < 4; k++) {
    d = fm1_enc_take(ENC_KNOB1 + k);
    if(d && naming) edit_name(k,d);
    else if(d) set_control(k,knob[page][k]+d);
  }
  for (int k = 0; k < 27; k++)
    if ((keys ^ key_prev) & (1u << k)) {
      if (keys & (1u << k)) {
        key_notes[k] = scale_note(bound(53 + k + octave * 12, 0, 127));
        key_channels[k]=knob[MIDI][0]?knob[MIDI][0]-1:0;
        keyboard_out(144+key_channels[k],key_notes[k],110);
        post(EV_NOTE, key_notes[k], 110);
      } else {
        keyboard_out(128+key_channels[k],key_notes[k],0);
        post(EV_OFF, key_notes[k], 0);
      }
    }
  key_prev = keys;
  uint8_t st, a, b;
  while(midi_get(&st,&a,&b)) midi_input(0,st,a,b);
#if !defined(GRANA_WEB) && !defined(DINGHY_HOST)
  radio_poll();
  while(bti_r!=bti_w){unsigned pkt=bt_in_q[bti_r++%BTQ];midi_input(1,(pkt>>8)&255,(pkt>>16)&127,(pkt>>24)&127);}
  if(bt_lost){bt_lost=0;for(int ch=0;ch<16;ch++)for(int n=0;n<128;n++)midi_input(1,128+ch,n,0);}
#endif
  if(!knob[MIDI][2] || (last_tick && fm1_ms-last_tick>1000)){last_tick=ticks=beat_ms=0;}
  update_leds(keys);
  if(globals_changed && fm1_ms-light_changed>=600u) {
    if(!settings_save(globals,sizeof globals)) globals_changed=0;
    light_changed=fm1_ms;
  }
  if (fm1_ms - last_frame >= (intro ? 33u : 80u)) {
    last_frame = fm1_ms;
    draw();

  }
}
#ifndef GRANA_WEB
static void app_main(void) {
  app_setup();
  for (;;) {
    platform_poll();
    app_step();
  }
}
#endif
static void app_sysex(const uint8_t *p, uint32_t n) {
  (void)p;
  (void)n;
}
#ifdef GRANA_WEB
static void web_presets_out(void) {}
static void web_presets_in(void) {}
static uint8_t *web_bank_buf(void) { return (uint8_t *)0; }
static uint32_t web_bank_size(void) { return 0; }
static void web_bank_changed(void) {}
static uint32_t web_state(void) {
  return ((naming?destination:program) << 16) | (naming << 11) | (page & 15) | (transport << 4) | (knob[ARP][0] << 8) | (vib_on << 9) | (reverse << 10);
}
WEB_EXPORT("page") int web_page(void){return page;}
WEB_EXPORT("idle_preview") void web_idle_preview(void){intro=0;draw();}
WEB_EXPORT("param") int web_param(unsigned k) {
  return k < 4 ? knob[page][k] : 0;
}
WEB_EXPORT("set_param") void web_set_param(unsigned k, int v) {
  if (k < 4)
    set_control(k,v);
}
#endif
