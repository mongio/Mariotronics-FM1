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
#include "mt_piano.h"
#include "mt_vu.h"
#include "mt_global.h"

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
enum { SINGING = 4, HOLLOW = 5, PIANO = 6, BRIGHT, GLASS_BELL, TAPE_CHOIR, RHODES, BOWED, ORGAN, REED, REED_ORGAN, HARMONIUM };
enum { HOME, SOUND, ART, FUZZ, SCALE, WAH, AMP, PLATE, ENV, COMP, LFO, MIDI, BT, ARP, OFF_TAPE, LIGHTS, CHARACTER, EQ, FILTER, HEAD2, PICK_ART, ERASE, SPEED, PAGE_COUNT };
static const char *const pages[] = {"TAPE", "INSTRUMENT", "PHRASING", "FUZZ", "SCALES", "WAH", "AMP", "PLATE", "ENV","COMP","LFO","MIDI","BLE MIDI","ARP","OFF-TAPE","LIGHTS","TAPE CHAR","EQ","FILTER","HEAD 2","ARP PICK","ERASE","SPEED"};
static const char *const names[][4] = {{"DELAY", "FDBK", "SEND", "RETURN"},
                                       {"VOICE", "DRIVE", "ENGINE", "MODE"},
                                       {"GLISS", "MS", "CENT", "HZ"}, {"DRIVE","TONE","BIAS","MIX"}, {"MODE","ROOT"}, {"SWEEP","RES","TONE","MIX"}, {"GAIN","BASS","TREBLE","VOL"}, {"MIX","DECAY","DAMP","PRE ms"}, {"A ms","D ms","S %","R ms"},{"THRESH","RATIO","REL ms","GAIN"},{"HZ","CENT","WAIT ms","RISE ms"},{"RX CH","CC IN","SYNC","BEATS"},{"RADIO","DEVICE"},{"PLAY","HZ","GATE %","ACCENT"},{"LEVEL %"},{"KEYS"},{"HF LOSS","SAT","WOW","FLUTTER"},{"LOW","MID","MID Hz","HIGH"},{"TYPE","CUT Hz","RES","MIX %"},{"HEAD 2"},{"WAIT ms","RISE ms","LEGATO","SEMI"},{"MODE","ARM"},{"cm/s","MODE"}};
static volatile int knob[][4] = {
    {40, 81, 80, 72}, {0, 55, 0, 0}, {1, 24, 22, 5}, {54,45,18,42}, {0,6}, {40,52,60,0}, {32,48,42,85}, {19,72,52,18}, {38,32,84,36},{46,2,40,12},{5,22,12,20},{0,1,0,8},{0,0},{0,9,72,38},{25},{2},{32,0,0,0},{12,12,4,12},{0,77,0,100},{0},{12,36,1,7},{0,0},{2,0}};
static const int maximum[][4] = {{80, 100, 100, 100},
                                 {1, 100, 2, 1},
                                 
                                 {1, 100, 80, 9}, {100, 100, 100, 100}, {2, 11},{100,100,100,100},{100,100,100,100},{100,100,100,80},{100,100,100,100},{100,8,100,100},{9,80,100,100},{16,1,1,16},{1,8},{1,16,95,100},{100},{2},{100,100,100,100},{24,24,8,24},{2,100,100,100},{1},{100,100,1,24},{1,1},{96,1}};
static const int minimum[][4] = {{40}, {0}, {0, 1, 0, 2}, {0}, {0},{0},{0},{0},{0},{0,1,0,0},{2,0,0,0},{0,0,0,1},{0},{0,3,20,0},{0},{0},{0},{0},{0},{0},{0},{0},{0}};
static const char *const riley_voices[]={"ORGAN","REED","REED ORG","HARMON."};
static const char *const ambient_voices[]={"PIANO","BRIGHT","GLASS","CHOIR","RHODES","BOW GL."};
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
static const char *const engines[] = {"FRIPP","ENO","RILEY"};
static int voice_kind(void) { return (knob[SOUND][2]==2 ? ORGAN : knob[SOUND][2]==1 ? PIANO : SINGING) + knob[SOUND][0]; }
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
  unsigned phase, increment, vibrato, sample_pos, sample_step, sample_wave, bell_phase;
  int sample_root, sample_reverse, hammer_filter, damper, piano_tone, tremolo;
  int bloom, strength, neck, pitch, start_pitch, target_pitch, glide_ticks, glide_at, pitch_clock;
  int16_t line[STRING_LEN];
  int note, gate, active, kind, at, delay, lp, level, attack, age;
  unsigned lfo_age;
  int env_q8, decay_step, sustain, release_step, release_length, peaked;
} String;
static String strings[4];
static unsigned char held_notes[128], held_counts[128];
static int phrase_enabled;
static int held_size;
static unsigned serial;
static unsigned note_frames(int kind){return kind==GLASS_BELL || kind==TAPE_CHOIR?36015u:PIANO_FRAMES;}
static unsigned timeline_step(int kind,int note){
  if(kind==GLASS_BELL || kind==TAPE_CHOIR){
    int root=36+8*bound((note-32)/8,0,7);
    return (period256[root]*21845u)/period256[note];
  }
  return kind==RHODES || kind==BOWED?16000u:piano_step[note];
}
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
  if(kind>=ORGAN){s->sample_wave=s->bell_phase=0;return;}
  if(kind>=PIANO) {
    s->at=-1;
    s->sample_root=bound((note-34)/9,0,5);
    s->sample_reverse=knob[ARP][0];
    s->sample_pos=s->sample_reverse ? (note_frames(kind)-2)<<16 : 0;
    s->sample_step=timeline_step(kind,note);
    s->strength=velocity;
    s->hammer_filter=0; s->sample_wave=s->bell_phase=0; s->piano_tone=128; s->damper=48; s->tremolo=0;
    return;
  }
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
  s->glide_ticks = (phrase_enabled ? 24 : knob[ART][1]) * 441 / 32;
  if (s->kind==SINGING || s->kind==HOLLOW) {
    int distance=absolute(s->target_pitch-s->start_pitch)/4096;
    s->glide_ticks=s->glide_ticks*bound(55+distance*7,55,160)/100;
  }
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
    if (strings[0].active && strings[0].gate && strings[0].kind==voice_kind())
      glide_to(&strings[0], note);
    else
      strike(0, note, vel, voice_kind());
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
  strike(slot, note, vel, voice_kind());
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
  if(!phrase_enabled)pluck=pluck*knob[ART][2]/40;
  int input = (s->kind==HOLLOW ? pluck : pluck/2) + held;
  int pre = input * (3 + drive / 9);
  int clipped = saturate(pre + absolute(pre) / 12);
  s->neck += (clipped - s->neck) / 4;
  return s->neck * 2;
}
static void update_pitch(String *s) {
  if (s->glide_at < s->glide_ticks) {
    s->glide_at++;
    int t=s->glide_at*1024/s->glide_ticks;
    if(s->kind==SINGING || s->kind==HOLLOW)
      {
        int eased=(t*t/1024)*(3072-2*t)/1024;
        t+=(eased-t)*(phrase_enabled?7:knob[ART][3]-2)/7;
      }
    s->pitch=s->start_pitch+(s->target_pitch-s->start_pitch)*t/1024;
  }
  s->vibrato += (unsigned)knob[LFO][0] * 1558265u;
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
static void piano_block(String *s,unsigned block){
  int weight=bound((s->strength-35)*3+(s->kind==BRIGHT?65:0),0,256);
  for(int layer=0;layer<2;layer++){
    const uint8_t *p=piano_bank+35*((2*s->sample_root+layer)*(PIANO_FRAMES/64)+block);
    int pred=(int16_t)(p[0]+256*p[1]),index=p[2];
    for(int i=0;i<=64;i++){
      if(i==64){
        if(block+1<PIANO_FRAMES/64)pred=(int16_t)(p[35]+256*p[36]);
      }else if(i){
        int byte=p[3+(i-1)/2],code=i&1?byte&15:byte>>4;
        int step=ima_step[index];
        int d=(step>>3)+(code&4?step:0)+(code&2?step>>1:0)+(code&1?step>>2:0);
        pred=bound(pred+(code&8?-d:d),-32768,32767);
        index=bound(index+ima_adjust[code&7],0,88);
      }
      if(!layer)s->line[i]=pred;
      else s->line[i]+=(pred-s->line[i])*weight/256;
    }
  }
  s->at=(int)block;
}
static int piano_tick(String *s) {
  if (!(s->pitch_clock++ & 15)) {
    update_pitch(s);
    unsigned read_delay=s->delay+64;
    s->sample_step=(timeline_step(s->kind,s->note)/32)*(period256[s->note]+64u)/read_delay*32;
    if(knob[SOUND][2]==1) {
      s->piano_tone=bound(16+knob[ARP][1]*7+s->strength/2+(s->kind==BRIGHT?45:0),16,245);
      s->damper=knob[ARP][2];
      s->tremolo=knob[ARP][3];
    }
  }
  unsigned frames=note_frames(s->kind);
  unsigned at=s->sample_pos>>16;
  int sample;
  if(s->kind<=BRIGHT) {
    if(s->at!=(int)(at>>6))piano_block(s,at>>6);
    int a=s->line[at&63],b=s->line[(at&63)+1];
    sample=a+(b-a)*(int)((s->sample_pos>>8)&255)/256;
  } else {
    /* Use the same reversible note timeline as the piano. */
    unsigned elapsed=at;
    unsigned ph=s->sample_wave;
    s->sample_wave+=s->sample_reverse ? -s->increment : s->increment;
    int tail=(int)(frames-1-elapsed)*256/(int)frames;
    if(s->kind==RHODES) {
      /* Tine fundamental and short-lived flexural mode feed a nonlinear pickup. */
      int dir=s->sample_reverse?-1:1;
      s->bell_phase+=dir*(s->increment*6u+(s->increment>>2));
      int edge=bound(256-(int)elapsed/14,0,256);
      int tine=sinus(ph)*tail/2;
      if(s->increment<0x11000000u)tine+=sinus(s->bell_phase)*edge*s->strength/1024;
      sample=tine+(sinus(ph*2u)*tail/12)*s->strength/127;
      sample+=((tine/128)*(tine/128)-4096)*s->strength/240;
      sample=sample*bound(elapsed,0,40)/40*bound(frames-1-elapsed,0,700)/700;
    } else if(s->kind==BOWED){
      s->bell_phase+=s->sample_reverse?-s->increment-(s->increment>>10):s->increment+(s->increment>>10);
      int bow=bound(elapsed,0,3400);
      sample=(sinus(ph)*42+sinus(s->bell_phase)*28+sinus(ph*3u)*bow/240)*bow/3400;
      sample=sample*bound(frames-1-elapsed,0,4200)/4200;
    } else if(s->kind==GLASS_BELL) {
      s->bell_phase+=s->sample_reverse ? -s->increment*3u-(s->increment>>1) :
                                         s->increment*3u+(s->increment>>1);
      sample=(sinus(ph)*64+sinus(s->bell_phase)*tail/8)*tail/256;
      sample=sample*bound(elapsed,0,32)/32;
    } else {
      s->bell_phase+=s->sample_reverse ? -s->increment+(s->increment>>8) :
                                         s->increment-(s->increment>>8);
      int choir=sinus(ph)*32+sinus(s->bell_phase)*28+
                sinus(ph*3u)*15+sinus(s->bell_phase*5u)*8;
      sample=choir*bound(elapsed,0,2600)/2600*bound(frames-1-elapsed,0,7500)/7500;
    }
  }
  /* The felt damper darkens released notes; ENV sets their amplitude. */
  int tone=s->gate?s->piano_tone:s->piano_tone*(112-s->damper)/112;
  s->hammer_filter+=(sample-s->hammer_filter)*tone/256;
  s->phase+=486957u; /* 5 Hz tremolo */
  int amp=256-(127+sinus(s->phase))*s->tremolo/200;
  if(s->sample_reverse ? s->sample_pos<s->sample_step :
      s->sample_step>=((frames-1)<<16)-s->sample_pos) s->active=0;
  else s->sample_pos+=s->sample_reverse ? -s->sample_step : s->sample_step;
  return (s->hammer_filter*s->strength/127)*amp/256;
}

static int envelope_tick(String *s) {
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
  return s->level;
}
static int string_tick(String *s) {
  if (!s->active)
    return 0;
  int level=envelope_tick(s);
  if(!s->active) return 0;
  if(s->kind>=ORGAN) {
    if (!(s->pitch_clock++ & 15)) update_pitch(s);
    unsigned p=s->phase;
    s->phase+=s->increment;
    int wave=sinus(p)*8 + sinus(2u*p)*3;
    if(s->increment < 0x20000000u) wave+=sinus(3u*p)*(s->kind==REED?5:2);
    if(s->increment < 0x18000000u) wave+=sinus(5u*p)*(s->kind==REED?3:1);
    if(s->kind==REED_ORGAN || s->kind==HARMONIUM){
      int slow=s->kind==HARMONIUM;
      s->sample_wave+=s->increment+(s->increment>>(slow?9:11));
      s->bell_phase+=slow?132000u:208000u;
      s->bloom=bound(s->bloom+1,0,slow?16000:3200);
      int breath=sinus(s->bell_phase);
      wave+=sinus(s->sample_wave)*6;
      if(s->increment<0x18000000u)wave+=sinus(4u*p)*(slow?2:4);
      if(!slow && s->bloom<3200)wave+=(rnd()/256)*(3200-s->bloom)/3200;
      wave=(wave*(slow?210+breath/5:235+breath/12)/256)*s->bloom/(slow?16000:3200);
    }
    return ((wave*s->strength/1024)*level)>>15;
  }
  if(s->kind>=PIANO) return (piano_tick(s)*level)>>15;
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
  int picked = y - pickup * 3 / 5;
  if (s->kind >= SINGING)
    picked = singing_pickup(s, picked, knob[SOUND][1]);
  return (picked * s->level) >> 15;
}
static int dec_samples;
static const unsigned short speed_q8[]={64,65,67,68,70,71,73,74,76,78,79,81,83,85,87,89,91,92,95,97,99,101,103,105,108,110,112,115,117,120,123,125,128,131,134,137,140,143,146,149,152,156,159,162,166,170,173,177,181,185,189,193,197,202,206,211,215,220,225,230,235,240,245,251,256,262,267,273,279,285,292,298,304,311,318,325,332,339,347,354,362,370,378,386,395,403,412,421,431,440,450,459,470,480,490,501,512};
static int speed_index(void){return knob[SPEED][1]?knob[SPEED][0]:32*knob[SPEED][0];}
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
static volatile int range_down;
static unsigned erased_count, erased_at[32];
static int erased_old[32];
static int slip_target, slip_offset, slip_mix;
static void finish_range(void) {
  unsigned n=erased_count<32?erased_count:32;
  for(unsigned i=0;i<n;i++){
    unsigned slot=(erased_count-1-i)&31;
    int edge=erased_old[slot]*(31-(int)i)/32;
    if(absolute(edge)>absolute(tape[erased_at[slot]]))tape[erased_at[slot]]=edge;
  }
  erased_count=0;
}
static int moving_head(int normal) {
  if(slip_offset!=slip_target){
    slip_offset+=bound(slip_target-slip_offset,-8,8);
    slip_mix=bound(slip_mix+4,0,256);
  }else if(slip_mix)slip_mix--;
  else {slip_offset=slip_target=0;return normal;}
  int q=(int)position*256+slip_offset*(reverse && transport==HOLD?-1:1);
  int span=(int)length*256;
  if(q<0)q+=span;else if(q>=span)q-=span;
  unsigned a=(unsigned)q>>8,b=a+1==length?0:a+1;
  int x=a<valid_samples?tape[a]:0,y=b<valid_samples?tape[b]:0;
  return normal+((x+(y-x)*(q&255)/256)-normal)*slip_mix/256;
}
static unsigned layer_pos;
static int layer_mix;
static unsigned silent_input;
static unsigned beat_length(void) {return beat_ms && knob[MIDI][2] ? (unsigned)bound((int)(beat_ms*knob[MIDI][3]*147u/10u),TAPE_RATE/4,CAPACITY) : knob[HOME][0]*(TAPE_RATE/10);}
static void rec_command(void) {
  reverse = 0;
  if (transport == STOP && !has_loop) {
    position = 0;
    dec_samples = dec_n = dec_sum = prev_return = next_return = tape_lp = 0;
    loop_mode = knob[SOUND][3];
    length = loop_mode ? CAPACITY : beat_length();
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
  valid_samples = layer_pos = 0;
  transport = STOP;
  has_loop = 0;
  position = 0;
  next_return = prev_return = tape_lp = dec_samples = dec_sum = dec_n = 0;
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
#include "mt_tone_fx.h"
static int instrument_fx(int sample) {
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
static int phrase_size, phrase_step, phrase_left, phrase_gate;
static int phrase_wait, phrase_gain=1048576, phrase_rise_step;
static void phrase_start(void) {
  int fripp=knob[SOUND][2]==0;
  phrase_wait=fripp ? knob[PICK_ART][0]*441 : 0;
  int rise=fripp ? knob[PICK_ART][1]*441 : 0;
  phrase_gain=rise ? 0 : 1048576;
  phrase_rise_step=rise ? (1048576+rise-1)/rise : 0;
}
static void phrase_reset(void) {
  for(int i=0;i<4;i++) strings[i].gate=0;
  held_size=0;
  for(int i=0;i<128;i++) held_counts[i]=0;
  phrase_step=phrase_left=phrase_gate=0;
}
static void phrase_event(int note,int velocity) {
  int previous_size=phrase_size;
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
  } else if (!phrase_size || !previous_size || knob[SOUND][2]!=0) {
    phrase_reset();
    if(phrase_size)phrase_start();
  }
}
static volatile unsigned pick_display, pick_time;
static volatile int pick_struck;
static void phrase_tick(void) {
  if(!phrase_size)return;
  if(phrase_wait){phrase_wait--;return;}
  if(phrase_gain<1048576)phrase_gain=bound(phrase_gain+phrase_rise_step,0,1048576);
  if(!phrase_left){
    int root=phrase_keys[0];
    int n=phrase_size==1 ? ((phrase_step&1)?scale_note(bound(root+(knob[SOUND][2]==0?knob[PICK_ART][3]:7),0,127)):root)
                         : phrase_keys[phrase_size-2+(phrase_step&1)];
    int left=phrase_size==1?root:phrase_keys[phrase_size-2];
    int right=phrase_size==1?scale_note(bound(root+knob[PICK_ART][3],0,127)):phrase_keys[phrase_size-1];
    pick_display=left | (right<<8) | ((phrase_step&1)<<16);
    pick_time=fm1_ms;
    pick_struck=!(knob[PICK_ART][2] && (phrase_step&1) && strings[0].gate);
    int accented=phrase_step==0 || phrase_step==3 || phrase_step==6;
    int v=phrase_velocity[phrase_size==1?root:n]*(accented?100:100-knob[ARP][3]*3/5)/100;
    int tied=knob[SOUND][2]==0 && knob[PICK_ART][2];
    phrase_left=44100/knob[ARP][1];
    if(tied && (phrase_step&1) && strings[0].gate){
      glide_to(&strings[0],n);
      strings[0].glide_ticks=bound(strings[0].glide_ticks,1,phrase_left/48);
    } else strike(0,n,v?v:1,voice_kind());
    /* A picked transient must fit inside a fast gate, even with long manual swells. */
    if(strings[0].attack<8388352/(44*6))strings[0].attack=8388352/(44*6);
    if(strings[0].release_length>1323)strings[0].release_length=1323;
    phrase_gate=tied && !(phrase_step&1)?0:phrase_left*knob[ARP][2]/100;
    phrase_step=(phrase_step+1)&7;
  }
  if(phrase_gate && !--phrase_gate)strings[0].gate=0;
  phrase_left--;
}

static void apply_events(void);
static volatile int16_t scope_ring[1024] POOLMEM;
static volatile unsigned scope_head;
static volatile int meter_level[2];
static volatile unsigned meter_peak_time[2];
static volatile unsigned meter_peak_valid;

static volatile unsigned audio_ready;
static void app_audio(int32_t *out, uint32_t n) {
  if (!audio_ready) return; /* Platform supplies a cleared buffer during startup. */
  apply_events();
  if(transport==STOP){slip_offset=slip_target=slip_mix=0;range_down=0;}
  if(!range_down && erased_count)finish_range();
  if (!loop_mode) {
    unsigned requested = beat_length();
    if (requested != length) {
      if(transport!=STOP && !knob[MIDI][2]){
        int delta=(int)requested-(int)length;
        slip_target=bound(slip_target-delta*4,-65536,65536);
      }
      length = requested;
      if (position >= length) position %= length;
      /* Keep the rest of the reel available when a longer setting returns. */
    }
  }
  unsigned layer_length = length * 731u / 1000u;
  int layer_target = knob[SOUND][2]==1 && knob[HEAD2][0] ? 256 : 0;
  ToneControls tone=tone_controls();
  tone_prepare(tone);
  int peak_in = 0, peak_out = 0;
  int decay=100+knob[PLATE][1]*148/100;
  int damping=235-knob[PLATE][2]*215/100;
  int predelay=knob[PLATE][3]*2205/100, wet=knob[PLATE][0];
  int hf_coefficient=256-2*knob[CHARACTER][0], saturation=knob[CHARACTER][1];
  int wow=knob[CHARACTER][2], flutter=knob[CHARACTER][3];
  unsigned scope_write=scope_head;
  int monitor_voice=page==SOUND, monitor_mix=page==CHARACTER;
  int tape_rate=speed_q8[bound(speed_index(),0,96)];
  unsigned sums[2]={0,0}, peaks[2]={0,0};
  for (unsigned i = 0; i < n; i++) {
    if(phrase_enabled)phrase_tick();
    int dry = 0;
    for (int j = 0; j < 4; j++)
      dry += string_tick(&strings[j]);
    if(monitor_voice)scope_ring[scope_write++ & 1023]=bound(dry,-32767,32767);
    int shape = voice_kind(), drive = knob[SOUND][1];
    int amp = shape >= SINGING ? dry : saturate(dry * (shape == 0 ? 1 + drive / 35 : 4 + drive / 9)) * 2;
    amp_lp += (amp - amp_lp) * (shape >= SINGING ? 2 : shape == 3 ? 5 : 3) / 8;
    dry = tone_process(instrument_fx(amp_lp),tone.mix);
    /* IIR stages can retain a small DC residue after the voice has ended. */
    if(strings[0].active || strings[1].active || strings[2].active || strings[3].active)
      silent_input=0;
    else if(silent_input<1024) silent_input++;
    else dry=0;
    if(phrase_enabled && phrase_wait) dry=0;
    else if(phrase_enabled && phrase_gain<1048576) dry=dry*(phrase_gain>>10)/1024;
    int send = dry * knob[HOME][2] / 100, ret = 0;
    if (transport != STOP) {
      ret = prev_return + (next_return - prev_return) * dec_n / 768;
      dec_sum += send;
      dec_samples++;
      dec_n+=tape_rate;
      if (dec_n>=768) {
        dec_n-=768;
        int old = position >= valid_samples ? 0 : tape[position];
        old=read_transport(old,wow,flutter);
        prev_return = next_return;
        next_return = (slip_offset || slip_target || slip_mix)?moving_head(old):old;
        if (layer_pos >= layer_length) layer_pos = 0;
        layer_mix += (layer_target>layer_mix) - (layer_target<layer_mix);
        if (layer_mix) {
          int layer = layer_pos < valid_samples ? tape[layer_pos] : 0;
          unsigned edge = layer_length-1-layer_pos;
          if (edge > layer_pos) edge = layer_pos;
          if (edge < 128) layer = layer*(int)edge/128;
          next_return += (layer-old)*layer_mix/768;
        }
        if (reverse && transport==HOLD)
          layer_pos = layer_pos ? layer_pos-1 : layer_length-1;
        else if (++layer_pos >= layer_length) layer_pos=0;
        if(range_down){
          unsigned slot=erased_count&31;
          erased_at[slot]=position;erased_old[slot]=position<valid_samples?tape[position]:0;
          tape[position]=erased_count<32?erased_old[slot]*(31-(int)erased_count)/32:0;
          erased_count++;advance_head(tape[position]);
          if(position==valid_samples)valid_samples++;
        }else if (transport != HOLD) {
          tape_lp += (old - tape_lp) * hf_coefficient / 256;
          int write = dec_sum / dec_samples + tape_lp * knob[HOME][1] / 100;
          tape[position] = (int16_t)tape_record_level(write,saturation);
          advance_head(tape[position]);
          if (position == valid_samples)
            valid_samples++;
        } else {
          advance_head(0);
        }
        dec_samples=dec_sum = 0;
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
    int head = head_previous + (head_filtered - head_previous) * dec_n / 768;
    int mix = dry + ret * knob[HOME][3] / 100 + head * knob[OFF_TAPE][0] / 100;
    if (plate_half) plate_tick((plate_input+mix)/4,decay,damping,predelay);
    plate_input=mix;
    int l=plate_half?(wet_l+prev_l)/2:wet_l;
    int r=plate_half?(wet_r+prev_r)/2:wet_r;
    plate_half^=1;
    out[2*i]=bound(mix+l*wet/65,-31500,31500);
    out[2*i+1]=bound(mix+r*wet/65,-31500,31500);
    if(monitor_mix)for(int ch=0;ch<2;ch++){
      unsigned v=absolute(out[2*i+ch]);
#if defined(GRANA_WEB)
      v=v*(unsigned)web_master>>15;
#elif !defined(DINGHY_HOST)
      v=v*(unsigned)master_q12>>12;
#endif
      sums[ch]+=v;
      if(v>peaks[ch])peaks[ch]=v;
    }
  }
  scope_head=scope_write;
  if(monitor_mix && n)for(int ch=0;ch<2;ch++){
    int mean=sums[ch]/n;
    meter_level[ch]+=(mean-meter_level[ch])*(int)n/(int)(4410+n);
    if(peaks[ch]>=29204){meter_peak_time[ch]=fm1_ms;meter_peak_valid|=1u<<ch;}
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
static void speed_label(char *s){
  unsigned q=speed_q8[bound(speed_index(),0,96)],v=(q*1900+128)/256;
  unsigned whole=v/100;number(s,whole);int n=whole>=10?2:1;
  s[n++]='.';s[n++]='0'+(v/10)%10;s[n++]='0'+v%10;s[n]=0;
}
static const char *value_text(int k, char *s) {
  int v = knob[page][k];
  if(page==LIGHTS) { static const char *const levels[]={"OFF","HALF","FULL"}; return levels[v]; }
  if(page==EQ || page==FILTER) {
    if(page==FILTER && !k) {static const char *const type[]={"OFF","LP","HP"};return type[v];}
    if((page==EQ && k==2)||(page==FILTER && k==1)) {
      unsigned hz=page==EQ?mid_hz[v]:cutoff_hz[v];
      if(hz>=1000) {number(s,hz/1000);int j=hz>=10000?2:1;s[j++]='.';s[j++]='0'+(hz/100)%10;s[j++]='k';s[j]=0;}
      else number(s,hz);
      return s;
    }
    if(page==EQ) {int gain=v-12;s[0]=gain<0?'-':'+';number(s+1,absolute(gain));return s;}
  }
  if(page==SPEED){if(k)return v?"VAR":"STEP";speed_label(s);return s;}
  if(page==ART && !knob[SOUND][2] && k>=2){number(s,k==2?v*100/40:(v-2)*100/7);return s;}
  if(page==ERASE)return k?(v?"ARMED":"OFF"):(v?"RANGE":"ALL");
  if(page==PICK_ART && k==2)return v?"ON":"OFF";
  if(page==PICK_ART && k<2){if(v==100)return "1000";number(s,10*v);return s;}
  if(page==HEAD2) return v?"ON":"OFF";
  if(page==ARP && knob[SOUND][2]==1 && k==0) return v?"REV":"FWD";
  if(page==ARP && k==0)return v?"ON":"OFF";
  if(page==MIDI && k==0 && !v) return "OMNI";
  if((page==MIDI && (k==1||k==2))||(page==BT && k==0)) return v?"ON":"OFF";
  if(page==LFO && k>=2) {v*=10;if(v==1000)return "1000";}
  if (page==HOME && k==0) {
    s[0]='0'+v/10; s[1]='.'; s[2]='0'+v%10; s[3]='s'; s[4]=0;
    return s;
  }
  if (page == SOUND) {
    if (k == 0)
      return knob[SOUND][2]==2 ? riley_voices[v] : knob[SOUND][2]==1 ? ambient_voices[v] : voices[v];
    if (k == 3)
      return v ? "LOOP" : "TAPE";
    if (k == 2)
      return engines[v];
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
  const char *caption=page==ENV?"ENVELOPE":page==PLATE?"STEREO  /  POST TAPE":"INSTRUMENT  /  PRE TAPE";
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

/* The operating display uses the unlettered signal diagram from the intro.
 * Transport phase is shared with the decks so a direction change retraces it. */
static void eno_diagram(int gap, unsigned clock) {
  int left=69-gap, right=171+gap;
  uint16_t frame=RGB(104,130,138), wire=RGB(184,207,205);
  for(int side=0;side<2;side++){
    int x=side?right:left, inward=side?-1:1;
    cv_line(x,46,x+inward*38,46,frame);
    cv_line(x+inward*38,46,x+inward*38,117,frame);
    cv_line(x+inward*38,117,x,117,frame);
    cv_line(x,117,x,46,frame);
    disc(x,47,21,frame);disc(x,47,18,0);
    disc(x,47,6,wire);disc(x,47,3,0);
    cv_line(x-inward*14,60,x+inward*20,88,wire);
  }
  int a=left+20,b=right-20;
  int route[][2]={{a,88},{b,88},{b,16},{a,16},{a,88}};
  int perimeter=2*(b-a+72);
  int pos=(clock%18400u)*perimeter/18400u;
  for(int i=0;i<4;i++){
    int x=route[i][0],y=route[i][1],dx=route[i+1][0]-x,dy=route[i+1][1]-y;
    cv_line(x,y,x+dx,y+dy,wire);
  }
  cv_line(14,16,a-8,16,frame);cv_line(a-8,16,a-8,77,frame);
  cv_line(a-8,77,a,88,frame);cv_line(a,88,a,132,frame);
  for(int i=0;i<4;i++){
    int x=route[i][0],y=route[i][1],dx=route[i+1][0]-x,dy=route[i+1][1]-y;
    int len=absolute(dx)+absolute(dy);
    if(pos<len){disc(x+dx*pos/len,y+dy*pos/len,3,RGB(255,226,62));break;}
    pos-=len;
  }
}

static void voice_scope(void) {
  uint16_t grid=RGB(33,53,54), trace=RGB(108,232,199);
  lettering(120,4,"VOICE / SCOPE",silver,0);
  for(int x=10;x<=230;x+=22)cv_line(x,29,x,128,grid);
  for(int y=29;y<=129;y+=25)cv_line(10,y,230,y,grid);
  unsigned end=scope_head, at=end-221;
  for(int lag=222;lag<800;lag++){
    unsigned pos=end-lag;
    if(scope_ring[(pos-1)&1023]<=0 && scope_ring[pos&1023]>0){at=pos;break;}
  }
  int prev=79;
  for(int x=0;x<=220;x++){
    int y=79-bound(scope_ring[(at+x)&1023]/350,-49,49);
    if(x)cv_line(9+x,prev,10+x,y,trace);
    prev=y;
  }
}
static void stereo_vu(void) {
  /* Cream meter cards, zinc bezels and red overload sectors. Nominal 0 VU
   * is -12 dBFS in mean-absolute units (a sine peaking near -8 dBFS). */
  static const int levels[]={0,823,2603,4629,5829,7338,8234,9239,10365,11630};
  static const int degrees[]={-45,-43,-32,-17,-4,11,19,27,35,42};
  for(int ch=0;ch<2;ch++){
    int origin=120*ch;
    for(int y=0;y<105;y++)for(int x=0;x<120;x++)
      cv_px[(y+10)*240+origin+x]=swap16(vu_palette[vu_face[y*120+x]]);
    int v=bound(meter_level[ch],0,levels[9]),k=0;
    while(k<8 && v>levels[k+1])k++;
    int deg=degrees[k]+(degrees[k+1]-degrees[k])*(v-levels[k])/(levels[k+1]-levels[k]);
    int angle=(deg*256/360)&255,dx=sine256[angle],dy=sine256[(angle+64)&255];
    int cx=origin+60;
    cv_line(cx+dx*20/127,118-dy*22/127,cx+dx*65/127,118-dy*72/127,RGB(47,39,29));
    int hot=(meter_peak_valid&(1u<<ch)) && fm1_ms-meter_peak_time[ch]<650;
    disc(origin+104,102,2,hot?RGB(255,55,27):RGB(91,47,35));
    lettering(cx,95,ch?"R":"L",silver,0);
  }
}
static void panel_art(const uint8_t *data,int x,int y,int lamp){
  unsigned count=0;int col=0;
  for(unsigned i=0;count<10000;i++){
    int code=data[i],shade=code&15;
    if(lamp>=0 && shade>=11 && lamp<2)shade=lamp?shade-2:5;
    uint16_t color=swap16(panel_colors[shade]);
    for(int n=0;n<=(code>>4);n++){
      cv_px[y*240+x+col]=color;count++;
      if(++col==100){col=0;y++;}
    }
  }
}
static void speed_dial(void){
  panel_art(dial_art,70,21,-1);
  int a=((-96+speed_index()*2)+256)&255;
  int dx=sine256[a],dy=-sine256[(a+64)&255];
  cv_line(120+dx*14/127,71+dy*14/127,120+dx*31/127,71+dy*31/127,gold);
  lettering(46,83,"4.75",silver,0);lettering(74,3,"9.5",silver,0);
  lettering(164,3,"19",silver,0);lettering(194,83,"38",silver,0);
  lettering(120,123,"TAPE SPEED",silver,0);
}
static void bluetooth_mark(void){
  uint16_t c=RGB(90,175,226);
  for(int i=0;i<3;i++){
    cv_line(66+i,29,66+i,99,c);cv_line(66+i,29,84+i,47,c);
    cv_line(84+i,47,48+i,82,c);cv_line(48+i,46,84+i,82,c);
    cv_line(84+i,82,66+i,99,c);
  }
}
static void note_label(int x,int note,uint16_t color) {
  char label[8];const char *p=tonic_names[note%12];int j=0;
  while(*p)label[j++]=*p++;
  int oct=note/12-1;if(oct<0){label[j++]='-';oct=-oct;}
  number(label+j,oct);lettering(x,48,label,color,0);
}
static void picking_view(void) {
  static const char *const iv[]={"P1","m2","M2","m3","M3","P4","TT","P5","m6","M6","m7","M7","P8"};
  unsigned state=pick_display;
  int active=phrase_enabled && phrase_size && !phrase_wait;
  int first=active?(state&127):phrase_size?phrase_keys[phrase_size>1?phrase_size-2:0]:60;
  int second=active?(state>>8)&127:phrase_size>1?phrase_keys[phrase_size-1]:scale_note(bound(first+knob[PICK_ART][3],0,127));
  int interval=absolute(second-first), side=(state>>16)&1;
  char text[8];number(text,interval);
  lettering(74,8,interval<=12?iv[interval]:"INTERVAL",silver,0);
  lettering(144,8,text,gold,0);lettering(180,8,"st",silver,0);
  if(knob[PICK_ART][2]){
    int prev=47;
    for(int x=62;x<=178;x+=2){int y=47-(x-62)*(178-x)/200;cv_line(x-2,prev,x,y,metal);prev=y;}
  }
  note_label(60,first,active && !side?gold:silver);note_label(180,second,active && side?gold:silver);
  if(active)disc(side?180:60,79,4,pick_struck && fm1_ms-pick_time<70?RGB(255,246,182):gold);
  lettering(120,107,phrase_wait && phrase_size?"WAIT":knob[PICK_ART][2]?"PICK + LEGATO":"PICK",silver,0);
}

static void draw(void) {
  if(intro && fm1_ms-intro_start<INTRO_END_MS){boot_draw(fm1_ms-intro_start);return;}intro=0;
  static const char *const modes[] = {"STOP", "REC", "DUB", "HOLD"};
  unsigned t = transport;
  cv_begin(240, 146, 0);
  if(page==SPEED)speed_dial();
  else if(page==ART && !knob[SOUND][2]){
    lettering(120,16,"MANUAL TOUCH",gold,0);
    int last=104;
    for(int i=0;i<=160;i++){
      int smooth=i*i*(480-2*i)/(160*160);
      int curve=i+(smooth-i)*(knob[ART][3]-2)/7;
      int y=104-curve*52/160;
      if(i)cv_line(39+i,last,40+i,y,silver);last=y;
    }
  }else if(page==ERASE){
    lettering(120,28,"ERASE HEAD",gold,0);
    lettering(120,71,range_down?"ERASING":knob[ERASE][0]?(knob[ERASE][1]?"HOLD REC TO ERASE":"ARM WITH K2"):"HOLD REC 1.2s",silver,0);
  }else if(page==SOUND)voice_scope();
  else if((page==ARP || page==PICK_ART) && knob[SOUND][2]==0)picking_view();
  else if(page==FUZZ || page==WAH || page==AMP || page==PLATE || page==ENV || page==COMP) effect_display();
  else if(page==OFF_TAPE) {
    lettering(120,12,"OFF-TAPE MONITOR",gold,0);
    cv_line(40,95,200,95,silver);
    cv_line(67,95,67,47,silver);
    cv_line(167,95,167,95-knob[OFF_TAPE][0]*48/100,gold);
    lettering(67,103,"DRY",silver,0);
    lettering(167,103,"180 ms",gold,0);
  } else if(page==EQ || page==FILTER) {
    ToneControls t=tone_controls();
    ToneCoefficients coeff[4];
    for(int i=0;i<4;i++) coeff[i]=tone_coeff(t,i);
    lettering(120,5,page==EQ?"EQUALIZER / PRE TAPE":"FILTER / PRE TAPE",gold,0);
    cv_line(20,73,220,73,metal);
    int last=73;
    for(int j=0;j<=40;j++) {
      float power=1;
      if(page==EQ) for(int i=0;i<3;i++) power*=tone_power(coeff[i],j,1);
      else if(t.mode) power=tone_power(coeff[3],j,t.mix*0.01f);
      int d=0;while(d<48 && power>plot_power[d]) d++;
      int y=73-2*(d-24);
      if(j)cv_line(15+j*5,last,20+j*5,y,gold);
      last=y;
    }
    lettering(32,122,"40 Hz",silver,0);lettering(203,122,"16 kHz",silver,0);
  } else if(page==CHARACTER || page==LIGHTS || page==BT || page==MIDI) {
    if(page==CHARACTER)stereo_vu();
    else if(page==LIGHTS)panel_art(bulb_art,70,12,knob[LIGHTS][0]);
    else if(page==MIDI)panel_art(din_art,70,12,-1);
    else {bluetooth_mark();panel_art(din_art,100,12,-1);}
    const char *status=page==MIDI?(beat_ms && knob[MIDI][2]?"CLOCK LOCKED":"MIDI INPUT"):
                       page==CHARACTER?"TAPE CHARACTER":page==LIGHTS?"KEY BACKLIGHT":"SYSTEM MIDI PORTS";
#if !defined(GRANA_WEB) && !defined(DINGHY_HOST)
    char peer[19];
    if(page==BT){
      const char *src=radio_name(knob[BT][1]);unsigned j=0;
      while(j<18 && src[j]){unsigned c=(unsigned char)src[j];peer[j++]=c>=32 && c<=126?c:'?';}
      peer[j]=0;status=knob[BT][1]?peer:bt_status;
    }
#endif
    lettering(120,123,status,silver,0);
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
  int target = bound(((int)length-4*(int)TAPE_RATE)*14/(4*(int)TAPE_RATE),0,14);
  gap += bound(target-gap,-2,2);
  int start[2] = {17-gap,121+gap};
  unsigned clock = turnclock;
  if(knob[SOUND][2]==1) eno_diagram(gap,clock);
  else {
  for (int i=0;i<2;i++) {
    const uint16_t *deck = knob[SOUND][2] ? (i?eno1:eno0) : (i?fripp1:fripp0);
    int cx=i?31:69, cy=31;
    unsigned angle=(clock*256u/(i?23000u:20700u))&255u;
    int cs=sine256[(angle+64)&255], sn=sine256[angle];
    for(int y=0;y<115;y++) {
      for(int x=0;x<101;x++) {
        int dx=x-cx,dy=y-cy, index=y*101+x;
        if(angle && dx*dx+dy*dy<30*30) {
          int u=cx+(dx*cs+dy*sn+8191)/127-64;
          int v=cy+(dy*cs-dx*sn+8191)/127-64;
          index=v*101+u;
        }
        cv_px[(y+5)*240+start[i]+x]=deck[index];
      }
    }
    lettering(start[i]+50,122,i?"PLAY":"REC",i?RGB(80,178,96):RGB(222,75,66),0);
  }
  cv_rect(start[0]+100,74,start[1]-start[0]-99,1,RGB(157,113,69));
  }
  }
  if(naming) {
    cv_begin(240,146,ink);
    char id[]={'P','0'+(destination+1)/10,'0'+(destination+1)%10,0};
    lettering(120,6,deleting?"DELETE PRESET":"SAVE PRESET",gold,ink);
    lettering(81,34,"Destination",silver,ink);lettering(166,34,id,gold,ink);
    cv_rect(9,57,222,1,metal);
    for(int i=0;i<NAME_LEN;i++){
      char ch[2]={draft[i],0}; if(i==cursor)cv_rect(9+14*i,89,12,2,gold);
      if(draft[i])lettering(15+14*i,64,ch,silver,ink);
      else {if(i>=cursor)break;}
    }
    lettering(120,113,deleting?"SAVE to delete   HOME to cancel":"SAVE to write   HOME to cancel",gold,ink);
  } else if (program_notice && fm1_ms-program_notice_time<1400u) {
    char title[NAME_LEN+1];slot_name(program,title);
    cv_rect(0,41,240,62,ink);
    lettering(120,46,program_notice,gold,ink); lettering(120,76,title,silver,ink);
  }
  cv_blit(0, 0);
  cv_begin(240, 94, ink);
  lettering(113,0,engines[knob[SOUND][2]],gold,ink);
  char slot_id[]={'P','0'+(program+1)/10,'0'+(program+1)%10,0};
  if(naming){
    lettering(120,20,"PRESETS selects destination",silver,ink);
    const char *labels[]={"CURSOR","LETTER","DICE","DELETE"};
    for(int k=0;k<4;k++) {
      char key[]={'K',(char)('1'+k),0};
      lettering(30+60*k,43,key,gold,ink);
      lettering(30+60*k,64,labels[k],silver,ink);
    }
    cv_blit(0,146);return;
  }
  lettering(155,0,slot_id,silver,ink);
  lettering(46,0,page==ARP && knob[SOUND][2]==1?"NOTE FX":page==ART && !knob[SOUND][2]?"SEL TOUCH":page==ARP && !knob[SOUND][2]?"ARP RATE":pages[page],gold,ink);
  if(t==HOLD) cv_rect(178,0,56,18,gold);
  lettering(204,0,(reverse && t==HOLD)?"REV":modes[t],t==HOLD?ink:silver,t==HOLD?gold:ink);
  int width = length ? position * 228u / length : 0;
  cv_rect(6, 19, 228, 2, metal);
  cv_rect(6, 19, width, 2, gold);
  for (int k = 0; k < 4; k++) {
    if ((page == OFF_TAPE || page == LIGHTS || page == HEAD2) && k > 0) break;
    if ((page == SCALE || page == BT || page==ERASE || page==SPEED) && k >= 2) break;
    int x = (page == SCALE || page == BT || page==ERASE || page==SPEED) ? 60 + 120 * k : 30 + 60 * k;
    int v = knob[page][k], lo = minimum[page][k], hi = page==SOUND && !k && knob[SOUND][2]? (knob[SOUND][2]==1?5:3):maximum[page][k];
    if(page==SPEED && !k && !knob[SPEED][1])hi=3;
    int h = 29 * (v - lo) / (hi - lo);
    cv_rect(x - 4, 26, 8, 30, metal);
    cv_rect(x - 4, 56 - h, 8, h, gold);
    char val[8];
    lettering(x,55,value_text(k,val),silver,ink);
    static const char *const piano_fx[]={"DIR","TONE","DAMP","TREM"};
    static const char *const manual[]={"GLISS","MS","PICK %","CURVE"};
    lettering(x,73,page==ART && !knob[SOUND][2]?manual[k]:page==ARP && knob[SOUND][2]==1?piano_fx[k]:names[page][k],gold,ink);
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
  int manual_glide = (knob[SOUND][2]!=1 && knob[ARP][0]) ? 0 : knob[ART][0];
  if (panic || last_glide != manual_glide) {
    last_glide = manual_glide;
    held_size = 0;
    for (int j = 0; j < 128; j++) held_counts[j] = 0;
    for (int j = 0; j < 4; j++)
      strings[j].gate = 0;
    if (panic) {reader=writer;phrase_size=0;for(int i=0;i<128;i++)phrase_count[i]=0;}
    panic = 0;
  }
  int picking=knob[SOUND][2]!=1 && knob[ARP][0];
  if(phrase_enabled!=picking){
    phrase_reset();phrase_enabled=picking;
    if(picking)phrase_start();
    if(!phrase_enabled)
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
    case EV_START: layer_pos=position=0;dec_samples=dec_n=dec_sum=prev_return=next_return=0;
      /* fall through */
    case EV_CONTINUE: if(has_loop) transport=HOLD;else if(transport==STOP)rec_command(); break;
    case EV_STOP: transport=STOP;break;
    case EV_CLEAR:
      erase_loop();
      break;
    case EV_REVERSE:
      if(transport==RECORD && valid_samples) rec_command();
      if (has_loop) {
        reverse = !reverse;
        transport = HOLD;
      }
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
static unsigned char globals[7]={2,32,0,0,0,2,0};
static int globals_changed;
static unsigned light_changed;
static uint16_t midi_lit[128];

static void update_leds(unsigned keys) {
  static const unsigned char page_button[PAGE_COUNT] = {
    BTN_HOME, BTN_EDIT, BTN_SEL, BTN_FX, BTN_SEL, BTN_FX, BTN_FX, BTN_FX,
    BTN_ENV, BTN_FX, BTN_LFO, BTN_GLO, BTN_GLO, BTN_ARP, BTN_FX, BTN_GLO, BTN_GLO, BTN_FX, BTN_FX, BTN_ARP, BTN_ARP, BTN_REC, BTN_GLO
  };
  static unsigned previous_keys, previous_buttons;
  unsigned buttons = 1u << page_button[page];
  unsigned t = transport;
  if (vib_on) buttons |= 1u << BTN_LFO;
  if (t==HOLD && reverse) buttons |= 1u << BTN_SEQ;
  if (knob[SOUND][2]!=1 && knob[ARP][0]) buttons |= 1u << BTN_ARP;
  if (octave < 0) buttons |= 1u << BTN_OCT_DOWN;
  if (octave > 0) buttons |= 1u << BTN_OCT_UP;
  if (naming) buttons |= 1u << BTN_SAVE;
  if (t != STOP) buttons |= 1u << BTN_PLAY;
  static unsigned blink_start;
  static int was_recording;
  int recording = t == RECORD || t == DUB;
  if (recording && !was_recording) blink_start = fm1_ms;
  if (recording && (fm1_ms - blink_start) % 500u < 250u) buttons |= 1u << BTN_REC;
  if(page==ERASE && !range_down && !recording)buttons&=~(1u<<BTN_REC);
  if(range_down)buttons|=1u<<BTN_REC;
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
  for(unsigned i=0;i<1024;i++)scope_ring[i]=0;
  unsigned char stored[7];
  if(!settings_load(stored,7) && stored[6]<=1 && stored[5]<=(stored[6]?96:3)){
    globals[5]=knob[SPEED][0]=stored[5];globals[6]=knob[SPEED][1]=stored[6];
  }
  if((!settings_load(stored,7) || !settings_load(stored,5)) && stored[0]<=2) {
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
  if(page==ERASE && (!k || !knob[ERASE][0])){knob[ERASE][1]=0;if(k)return;}
  if(page==SOUND && k==2) { switch_engine(bound(v,0,2)); return; }
  if(page==BT && k==0) {
#if !defined(GRANA_WEB) && !defined(DINGHY_HOST)
    v=radio_power(v!=0);
#else
    v=0;
#endif
  }
  if(page==SPEED){
    if(k>1)return;
    int index=bound(speed_index(),0,96);
    int mode=k==1?bound(v,0,1):knob[SPEED][1];
    int rate=k==0?bound(v,0,mode?96:3):(mode?index:(index+16)/32);
    globals[5]=knob[SPEED][0]=rate;globals[6]=knob[SPEED][1]=mode;
    globals_changed=1;light_changed=fm1_ms;return;
  }
  if(page==LIGHTS && k==0 && knob[LIGHTS][0]!=bound(v,0,2)) {
    globals_changed=1; light_changed=fm1_ms;
    globals[0]=bound(v,0,2);
    set_key_backlight(bound(v,0,2));
  }
  if(page==CHARACTER && knob[page][k]!=bound(v,0,100)) {
    globals[1+k]=bound(v,0,100); globals_changed=1; light_changed=fm1_ms;
  }
  knob[page][k]=bound(v,minimum[page][k],page==SOUND && !k && knob[SOUND][2]? (knob[SOUND][2]==1?5:3):maximum[page][k]);
  if(knob[SOUND][2] && page==LFO && k<2) knob[ART][3-k]=knob[LFO][k];
  if(knob[SOUND][2] && page==ART && k>=2) knob[LFO][3-k]=knob[ART][k];

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
    case 1:knob[LFO][1]=b*80/127;if(knob[SOUND][2])knob[ART][2]=knob[LFO][1];break;
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
  if (edge & (1u << BTN_LFO)) { if(page==LFO) vib_on=!vib_on; page=LFO; }
  if (edge & (1u << BTN_EDIT))
    page = SOUND;
  if (edge & (1u << BTN_SEL)) page = page==ART ? SCALE : ART;
  if (edge & (1u << BTN_GLO)) page = page==CHARACTER ? SPEED : page==SPEED ? MIDI : page==MIDI ? BT : page==BT ? LIGHTS : CHARACTER;
  if(edge & (1u<<BTN_ARP)) {
    if(knob[SOUND][2]==0 && page==ARP){page=PICK_ART;}
    else if(knob[SOUND][2]==0 && page==PICK_ART){page=ARP;}
    else if(knob[SOUND][2]==1) page=page==ARP ? HEAD2 : ARP;
    else { knob[ARP][0]=!knob[ARP][0]; page=ARP; }
  }
  if(edge & (1u<<BTN_SEQ)) {page=HOME;post(EV_REVERSE,0,0);}
  if(edge & (1u<<BTN_SAVE)) {
    if(naming)save_program(); else start_name(program);
  }
  if (edge & (1u << BTN_REC)) {
    rec_started = fm1_ms;
    if(knob[ERASE][0] && knob[ERASE][1]){
      range_down=transport!=STOP;knob[ERASE][1]=0;rec_pending=2;page=ERASE;
    }else rec_pending=1;
  }
  if (rec_pending==1 && !knob[ERASE][0] && fm1_ms - rec_started >= 1200) {
    post(EV_CLEAR, 0, 0);
    rec_pending = 0;
  }
  if (rec_pending && (rel & (1u << BTN_REC))) {
    if(rec_pending==2)range_down=0;else post(EV_REC, 0, 0);
    rec_pending = 0;
  }
  if (edge & (1u << BTN_PLAY))
    post(EV_PLAY, 0, 0);
  if (edge & (1u << BTN_FX))
    page = page==COMP ? FUZZ : page==FUZZ ? WAH : page==WAH ? AMP : page==AMP ? EQ : page==EQ ? FILTER : page==FILTER ? OFF_TAPE : page==OFF_TAPE ? PLATE : COMP;
  if (edge & (1u << BTN_ENV)) page = ENV;
  int d = fm1_enc_take(ENC_SELECT);
  if(d)intro=0;
  if (d) naming=0;
  if (d)
    {
      int last=knob[SOUND][2]==2 ? HEAD2 : HEAD2+1;
      int count=last+2;
      int index=page==SPEED?last:page==ERASE?last+1:page==PICK_ART?HEAD2:page;
      index=((index+d)%count+count)%count;
      page=index==last+1?ERASE:index==last?SPEED:index==HEAD2 && knob[SOUND][2]==0 ? PICK_ART : index;
    }
  d = fm1_enc_take(ENC_ALGORITHM);
  if(d)intro=0;
  if (d && !naming) switch_engine(bound(knob[SOUND][2]+d,0,2));
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
#include "mt_backup.h"
#ifdef GRANA_WEB
static void web_presets_out(void) {}
static void web_presets_in(void) {}
static uint8_t *web_bank_buf(void) { return (uint8_t *)0; }
static uint32_t web_bank_size(void) { return 0; }
static void web_bank_changed(void) {}
static uint32_t web_state(void) {
  return ((naming?destination:program) << 16) | (knob[SOUND][2] << 12) | (naming << 11) | (page & 15) | (transport << 4) | (knob[ARP][0] << 8) | (vib_on << 9) | (reverse << 10);
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
