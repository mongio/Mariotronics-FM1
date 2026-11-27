#include <stdint.h>
/* SPDX-License-Identifier: GPL-3.0-only
 * Mario Texture's Mariotronics. FM-1 host/platform from VIRGO, GRANA and
 * Felucca. Guitar: fixed-point adaptation of De-Pattern/Ravel_FM1 GUITAR
 * (rv_synth.c). Triangle pluck, lossy fractional delay and a second pickup tap.
 */
#define APP_NAME "MARIOTRONICS"
#define CV_MAX (240u * 146u)
/* The audio path runs from RAM: from the flash it shared the XIP cache with
   the piano samples and stalled when several voices read them at once. */
#if defined(GRANA_WEB) || defined(DINGHY_HOST)
#define AUDIO_FAST
#define AUDIO_TABLE
#else
#define AUDIO_FAST __attribute__((section(".fast_text")))
#define AUDIO_TABLE __attribute__((section(".fast_rodata")))
#endif
#if defined(GRANA_WEB) || defined(DINGHY_HOST)
#define POOLMEM
#else
#define POOLMEM __attribute__((section(".pool")))
#endif
#include "mt_art.h"
#include "platform.c"
#include "mt_ble.h"
#include "mt_intro.h"
#include "mt_piano.h"
#include "mt_eno_bank.h"
#include "mt_vu.h"

#ifndef RING_PUBLISH
#define RING_PUBLISH()                                                         \
  do {                                                                         \
  } while (0)
#endif
#define TAPE_RATE 14700u
#define CAPACITY (8u * TAPE_RATE)
#define STRING_LEN 2048
static int16_t tape[CAPACITY] POOLMEM;
enum { STOP, RECORD, DUB, HOLD };
enum { SINGING = 4, HOLLOW = 5, PIANO = 6, BRIGHT, GLASS_BELL, TAPE_CHOIR, RHODES, BOWED, ORGAN, REED, REED_ORGAN, HARMONIUM, BEATING, SUB_DRONE, VEIL, FIFTH, EVENING, DUSK, TIDAL, ABYSS, NOCTURNE, UMBRA, AIR };
enum { HOME, SOUND, ART, FUZZ, SCALE, WAH, AMP, PLATE, ENV, COMP, LFO, MIDI, BT, ARP, OFF_TAPE, LIGHTS, CHARACTER, EQ, FILTER, HEAD2, PICK_ART, ERASE, SPEED, TIMBRE, NOTE_FX, MOTION, UNCERT, ROUTE, FEEL, LFO2, MODFX, RING, MATRIX, MLFO, PAGE_COUNT };
static const char *const pages[] = {"TAPE", "INSTRUMENT", "PHRASING", "FUZZ", "SCALES", "WAH", "AMP", "PLATE", "ENV","COMP","LFO","MIDI","BLE MIDI","ARP","OFF-TAPE","KEYS","TAPE CHAR","EQ","FILTER","HEAD 2","ARP PICK","ERASE","SPEED","TIMBRE","NOTE FX","MOTION","SOURCE","ROUTING","ARP FEEL","LFO 2","ENSEMBLE","RING MOD","MATRIX","MATRIX LFO"};
static const char *const names[][4] = {{"Delay", "FDBK", "Send", "Return"},
                                       {"Voice", "Drive", "Engine", "Mode"},
                                       {"Gliss", "MS", "Cent", "HZ"}, {"Drive","Tone","Bias","Mix"}, {"Mode","Root","Transp.","Tune"}, {"Sweep","Res","Tone","Mix"}, {"Gain","Bass","Treble","Vol"}, {"Mix","Decay","Damp","Pre ms"}, {"A ms","D ms","S %","R ms"},{"Thresh","Ratio","Rel ms","Gain"},{"HZ","Cent","Wait ms","Rise ms"},{"RX CH","CC IN","Sync","Beats"},{"Radio","Device","Keys Out","Thru"},{"Play","HZ","Gate %","Accent"},{"Level %","Tone","Pan","Time ms"},{"Light","Hold Key","Velocity","Hold s"},{"HF Loss","Sat","Wow","Flutter"},{"Low","Mid","Mid Hz","High"},{"Type","Cut Hz","Res","Mix %"},{"Head 2","Level %","Length","Reverse"},{"Wait ms","Rise ms","Legato","Semi"},{"Mode","Arm","Depth %","Soft"},{"cm/s","Mode","Glide","Fine"},{"Color","Body"},{"Dir","Tone","Damp","Trem"},{"Rate","Depth","Shape","Delay"},{"Wi-Fi","Shape","Station","Slew"},{"Target","Amount","Target 2","Amount 2"},{"Swing","Accent","Human","Repeat"},{"Rate","Shape","Target","Depth"},{"Type","Rate","Depth","Mix %"},{"Ring","Ratio","Excite","Drift"},{"Slot","Source","Target","Amount"},{"LFO A","LFO B","Spread","Shape"}};
static volatile int knob[][4] = {
    {40, 81, 80, 72}, {0, 55, 0, 0}, {1, 24, 22, 5}, {54,45,18,42}, {0,6,0,0}, {40,52,60,0}, {32,48,42,85}, {19,72,52,18}, {38,32,84,36},{46,2,40,12},{5,22,12,20},{0,1,0,8},{0,0,1,0},{0,9,72,38},{25,100,50,180},{2,0,110,10},{32,0,0,0},{12,12,4,12},{0,77,0,100},{0,100,73,0},{12,36,1,7},{0,0,100,0},{2,0,0,0},{4,4,4,4},{0,10,48,0},{1,0,0,0},{0,0,0,3},{0,0,4,0},{0,1,0,1},{12,0,4,0},{0,30,50,50},{0,2,0,0},{0,0,0,0},{3,2,2,0}};
static const int maximum[][4] = {{80, 100, 100, 100},
                                 {1, 100, 4, 1},
                                 
                                 {1, 100, 80, 9}, {100, 100, 100, 100}, {2, 11, 12, 50},{100,100,100,100},{100,100,100,100},{100,100,100,80},{100,100,100,100},{100,8,100,100},{9,80,100,100},{16,1,1,16},{1,8,1,1},{1,16,95,100},{100,100,100,180},{2,1,127,20},{100,100,100,100},{24,24,8,24},{2,100,100,100},{1,100,95,1},{100,100,1,24},{1,1,100,100},{96,1,100,50},{8,8,8,8},{1,16,95,100},{2,7,3,8},{1,2,8,8},{10,100,10,100},{50,3,100,4},{40,6,10,100},{8,100,100,100},{8,8,2,8},{3,3,9,7},{7,7,7,1}};
static const int minimum[][4] = {{40}, {0}, {0, 1, 0, 2}, {0}, {0,0,-12,-50},{0},{0},{0},{0},{0,1,0,0},{2,0,0,0},{0,0,0,1},{0},{0,-9,20,0},{0,0,0,20},{0,0,1,5},{0},{0},{0},{0,0,50,0},{0},{0},{0,0,0,-50},{0},{0,3,20,0},{0},{0},{0,-100,0,-100},{0,0,0,1},{0,0,0,-100},{0},{0},{0,0,0,-7},{0}};
static const char *const riley_voices[]={"ORGAN","REED","REED ORG","HARMON."};
static const char *const ambient_voices[]={"PIANO","PIANO","GLASS","CHOIR","RHODES","BOW GL.","E.GLASS","D.CHOIR","T.HAZE","ABYSS","NOCTURNE","UMBRA"};
/* 1.20: the ENO voice menu. Stored numbers stay as they were (presets keep their voice); 1 was
   Bright Piano and now plays the Piano. 9-11 are the dark pads. */
enum { ENO_VOICES=11 };
static const unsigned char eno_menu[ENO_VOICES]={0,4,2,3,5,6,7,8,9,10,11};
static int eno_menu_pos(int stored){for(int i=0;i<ENO_VOICES;i++)if(eno_menu[i]==stored)return i;return 0;}
static const char *const voices[] = {"SING", "HOLLOW"};
static const char *const mode_names[] = {"CHROM", "DORIAN", "MAJOR"};
static const char *const tonic_names[] = {"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
static inline __attribute__((always_inline)) int scale_note_now(int note) {
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
static int scale_note(int note) { return scale_note_now(note); }
static const char *const engines[] = {"FRIPP","ENO","RILEY","RADIGUE","STOCKHAUSEN"};
static const char *const drones[]={"BEATING","SUB","VEIL","FIFTH"};
static const char *const patterns[]={"OFF","CLIMB","FOLD","PHASE","ORBIT"};
static const char *const timbre_labels[][4]={
  {"Hammer","Body","Damper","Reson."},{"Hammer","Body","Damper","Reson."},{"Strike","Alloy","Octave","Sympath."},{"Wave","Air","Octave","Sympath."},
  {"Tine","Pickup","Octave","Sympath."},{"Bow","Glass","Octave","Sympath."},{"4 FT","2 2/3 FT","Perc","Leslie"},{"Edge","Breath","Perc","Leslie"},
  {"4 FT","Mixture","Perc","Leslie"},{"Bellows","Air","Perc","Leslie"},{"Beat","Color","Sub","Drift"},{"Beat","Color","Sub","Drift"},
  {"Beat","Color","Sub","Drift"},{"Beat","Color","Sub","Drift"},{"Beat","Color","Drift","Shape"},{"Vowel","Blend","Drift","Air"},{"Noise","Tone","Swell","Drift"},{"Sub","Dark","Drift","Swell"},{"Spread","Tone","Motion","Air"},{"Hollow","Tone","Breath","Rumble"},{"Tune","Inertia","Material","Ring"}};
static inline __attribute__((always_inline)) int voice_kind_now(void) { if(knob[SOUND][2]==4)return AIR;if(knob[SOUND][2]==1){int v=knob[SOUND][0];if(v>=9)return ABYSS+v-9;if(v>=6)return EVENING+v-6;if(v==1)return PIANO;}return (knob[SOUND][2]==3 ? BEATING : knob[SOUND][2]==2 ? ORGAN : knob[SOUND][2]==1 ? PIANO : SINGING) + knob[SOUND][0]; }
static int voice_kind(void) { return voice_kind_now(); }
/* Arpeggio rate: values 3..32 are whole hertz as before; below 3 the knob slows
   down in twelve steps to one step every 20 seconds. */
static const unsigned short arp_slow100[12] AUDIO_TABLE = {250,200,150,125,100,75,50,33,25,20,10,5};
static inline __attribute__((always_inline)) int arp_hz100(int v){return v>=3?v*100:arp_slow100[v<-9?11:2-v];}
/* Voices whose third and fourth TIMBRE knobs are new in Lab 8 (stored in the Lab record). */
static inline __attribute__((always_inline)) int new_timbre_kind(int k){return k>=GLASS_BELL && k<=FIFTH;}
static int page, octave;
static volatile int vib_on = 1;
static volatile unsigned transport, position, length = TAPE_RATE * 4, has_loop,
                                              turnclock;
static volatile int vu_in, vu_out;
static unsigned valid_samples;
static unsigned random_state = 14701;
static inline __attribute__((always_inline)) int bound(int x, int lo, int hi) {
  return x < lo ? lo : x > hi ? hi : x;
}
static inline __attribute__((always_inline)) int absolute(int x) { return x < 0 ? -x : x; }
static inline __attribute__((always_inline)) int rnd(void) {
  random_state = random_state * 1664525u + 1013904223u;
  return (int)(random_state >> 16) - 32768;
}

typedef struct {
  unsigned phase, increment, vibrato, sample_pos, sample_step, sample_wave, bell_phase;
  unsigned motion_phase; int motion_filter, motion_hold;
  int sample_frames, sample_root, sample_reverse, hammer_filter, damper, piano_tone, tremolo;
  int bloom, strength, neck, pitch, start_pitch, target_pitch, glide_ticks, glide_at, pitch_clock;
  int16_t line[STRING_LEN];
  int note, gate, active, kind, at, delay, lp, level, attack, age;
  unsigned lfo_age;
  int env_q8, decay_step, sustain, release_step, release_length, peaked;
} String;
/* Lab 8 voice extras keep their state in the string line, unused by these voices. */
typedef struct { int follow, perc, click, drift, drift_target; unsigned oct_phase, sub_phase, rotor, comb_at, clock; } __attribute__((may_alias)) VoiceX;
enum { COMB_AT=24, COMB_LEN=STRING_LEN-COMB_AT };
static String strings[4];
static int sinus(unsigned phase);
static void update_pitch(String *s);
/* Signed RF modulation, applied to the instrument before the tape send. */
enum { RF_TIMBRE, RF_COLOR, RF_LEVEL, RF_PITCH, RF_FILTER, RF_TAPE, RF_PLATE, RF_RATE, RF_DENSITY, RF_FXRATE, RF_FXDEPTH };
static volatile int rf_amount,rf_amount2,rf_source,lfo2_amount,lfo2_wave;
static volatile unsigned lfo2_phase;
/* Each target sums the two Wi-Fi routes and LFO 2 (the vibrato stays separate). */
static inline __attribute__((always_inline)) int rf_mod(int dest){
  int sum=knob[LFO2][2]==dest?lfo2_amount:0;
  if(knob[UNCERT][0])sum+=(knob[ROUTE][0]==dest?rf_amount:0)+(knob[ROUTE][2]==dest?rf_amount2:0);
  return sum;
}
/* LFO 2 rate: 41 steps from 0.02 to 20 Hz (hundredths of a hertz). */
static const unsigned short lfo2_rates[41]={2,2,3,3,4,4,5,6,7,8,10,12,14,16,20,24,28,33,40,48,56,67,80,95,110,130,160,190,220,260,320,380,450,530,630,750,900,1060,1260,1500,2000};
static int lfo2_hz100(int v){return lfo2_rates[bound(v,0,40)];}
static AUDIO_FAST int rf_timbre(int base){return bound(base+rf_mod(RF_TIMBRE)*8/100,0,8);}
#include "mt_acoustic.h"
#include "mt_air.h"
#include "mt_modal.h"
static unsigned char held_notes[128], held_counts[128];
static int phrase_enabled;
static int held_size;
static unsigned serial;
static inline __attribute__((always_inline)) unsigned note_frames(int kind){return kind==GLASS_BELL || kind==TAPE_CHOIR?36015u:kind==RHODES?64000u:(kind==PIANO || kind==BRIGHT?PIANO_FRAMES:43520u);}
static inline __attribute__((always_inline)) unsigned timeline_step(int kind,int note){
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
  if(kind>=PIANO && kind<ORGAN)velocity=bound(velocity+rf_mod(RF_TIMBRE)/2,1,127);
  int corner = bound(n * ((kind == 0 ? 19 : 30)+rf_mod(RF_TIMBRE)/4) / 100, 1, n - 1);
  s->phase = s->vibrato = s->lfo_age = 0;
  s->increment = (0xffffffffu / ((unsigned)delay + 64u)) * 256u;
  s->motion_phase=0;s->motion_filter=0;
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
  if(kind==PIANO || kind==BRIGHT){__builtin_memset(&piano_strings[slot],0,sizeof(PianoStrings));__builtin_memset(s->line,0,sizeof s->line);}
  if(kind==AIR) __builtin_memset(&air_bodies[slot],0,sizeof(AirBody));
  if(new_timbre_kind(kind)){
    __builtin_memset(s->line,0,sizeof s->line);
    VoiceX *x=(VoiceX *)(void *)s->line;x->perc=32767;x->click=160;
  }
  if(kind>=ABYSS && kind<=UMBRA){s->hammer_filter=s->damper=0;s->sample_pos=0x40000000u;}
  if(kind>=ORGAN){s->sample_wave=s->bell_phase=0;return;}
  if(kind>=PIANO) {
    s->at=-1;
    s->sample_root=bound((note-32)/9,0,5);
    if(kind<=BRIGHT){unsigned a=eno_frames(2*s->sample_root),b=eno_frames(2*s->sample_root+1);s->sample_frames=(int)(a<b?a:b);}
    else s->sample_frames=(int)note_frames(kind);
    s->sample_reverse=knob[NOTE_FX][0];
    s->sample_pos=s->sample_reverse ? ((unsigned)s->sample_frames-2)<<(kind<=BRIGHT?15:16) : 0;
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
static inline __attribute__((always_inline)) int sinus(unsigned phase) {
  unsigned index = phase >> 24;
  int a = sine256[index], b = sine256[(index + 1u) & 255u];
  return a + (b - a) * (int)((phase >> 16) & 255u) / 256;
}
static AUDIO_FAST int singing_pickup(String *s, int pluck, int drive) {
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
static volatile int air_drift;
/* Block-rate randomness has its own generator, so the voices' noise is unchanged. */
static int control_rnd(void){static unsigned x=0x9e3779b9u;x^=x<<13;x^=x>>17;x^=x<<5;return (int)(x>>16)-32768;}   /* RING MOD Drift: a slow random walk, +/- 60 cents */
static AUDIO_FAST void update_pitch(String *s) {
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
  offset+=rf_mod(RF_PITCH)*4096*7/100;            /* up to a fifth either way */
  offset+=knob[SCALE][2]*4096+knob[SCALE][3]*4096/100;   /* SCALES Transpose and Tune */
  if(s->kind==AIR && knob[RING][3])offset+=air_drift*knob[RING][3]/8;
  int pitch = bound(s->pitch + offset, 0, 127 * 4096);
  int note = pitch >> 12, fraction = pitch & 4095;
  int delay = period256[note];
  if (fraction) delay += ((int)period256[note+1] - delay) * fraction / 4096;
  s->delay = delay;
  s->increment = (0xffffffffu / ((unsigned)delay + 64u)) * 256u;
}
static AUDIO_FAST const uint8_t *piano_packet(unsigned zone,unsigned block){
  unsigned base_blocks=PIANO_FRAMES/64;
  const EnoSlot *d=&eno_slots[zone];
  if(d->mode==2)return eno_flash+d->offset+35*block;       /* a replaced recording */
  if(block<base_blocks)return piano_bank+35*(zone*base_blocks+block);
  return eno_flash+d->offset+35*(block-base_blocks);       /* the sustain continuation */
}
static AUDIO_FAST void piano_block(String *s,unsigned block){
  int weight=bound((s->strength-35)*3+(s->kind==BRIGHT?65:0),0,256);
  /* At the ends of the velocity range one layer is the whole result: decode only it. */
  for(int layer=weight==256;layer<(weight?2:1);layer++){
    unsigned zone=2*s->sample_root+layer;
    const uint8_t *p=piano_packet(zone,block);
    int pred=(int16_t)(p[0]+256*p[1]),index=p[2];
    for(int i=0;i<=64;i++){
      if(i==64){
        if(block+1<(unsigned)s->sample_frames/64){
          const uint8_t *q=piano_packet(zone,block+1);pred=(int16_t)(q[0]+256*q[1]);
        }
      }else if(i){
        int byte=p[3+(i-1)/2],code=i&1?byte&15:byte>>4;
        int step=ima_step[index];
        int d=(step>>3)+(code&4?step:0)+(code&2?step>>1:0)+(code&1?step>>2:0);
        pred=bound(pred+(code&8?-d:d),-32768,32767);
        index=bound(index+ima_adjust[code&7],0,88);
      }
      if(!layer || weight==256)piano_strings[s-strings].decoded[i]=pred;
      else piano_strings[s-strings].decoded[i]+=(pred-piano_strings[s-strings].decoded[i])*weight/256;
    }
  }
  s->at=(int)block;
}
/* 2^(-x/256) in Q15, x >= 0: one octave of 33 points, linear in between. */
static AUDIO_TABLE const unsigned short rhodes_exp_tab[33]={32767,32066,31379,30708,30048,29405,28774,28158,27554,26964,26386,25821,25268,24726,24196,23678,23170,22674,22188,21712,21247,20792,20346,19910,19484,19066,18658,18258,17867,17484,17109,16743,16384};
static inline __attribute__((always_inline)) int rhodes_exp(int x){
  if(x<0)x=0;
  if(x>=256*15)return 0;
  int f=x&255,i=f>>3,a=rhodes_exp_tab[i],b=rhodes_exp_tab[i+1];
  return (a+((b-a)*(f&7)>>3))>>(x>>8);
}
static unsigned piano_rate(unsigned long long scaled,unsigned delay){return (unsigned)(scaled/delay);}
static unsigned (*volatile piano_rate_p)(unsigned long long,unsigned) = piano_rate;
static AUDIO_FAST int piano_tick(String *s) {
  if (!(s->pitch_clock++ & 15)) {
    update_pitch(s);
    unsigned read_delay=s->delay+64;
    s->sample_step=(timeline_step(s->kind,s->note)/32)*(period256[s->note]+64u)/read_delay*32;
    if(s->kind<=BRIGHT) s->sample_step=piano_rate_p(11889ull*(period256[36+9*s->sample_root]+64u),read_delay);
    if(knob[SOUND][2]==1) {
      s->piano_tone=bound(16+knob[NOTE_FX][1]*7+(knob[TIMBRE][0]-4)*12+s->strength/2+(s->kind==BRIGHT?45:0),16,245);
      s->damper=knob[NOTE_FX][2];
      s->tremolo=knob[NOTE_FX][3];
    }
  }
  unsigned frames=(unsigned)s->sample_frames;
  unsigned shift=s->kind<=BRIGHT?15:16;
  unsigned at=s->sample_pos>>shift;
  int sample;
  if(s->kind<=BRIGHT) {
    if(s->at!=(int)(at>>6))piano_block(s,at>>6);
    int a=piano_strings[s-strings].decoded[at&63],b=piano_strings[s-strings].decoded[(at&63)+1];
    sample=a+(b-a)*(int)((s->sample_pos>>7)&255)/256;
    sample=sample*(int)bound(frames-1-at,0,3200)/3200;
    if(!s->sample_reverse){
      if(s->sample_pos==((frames-1)<<shift))sample=0;
      sample+=piano_bridge(s,sample)/3;
    }
    s->neck+=(sample-s->neck)/12;sample+=(knob[TIMBRE][1]-4)*s->neck/8;
  } else {
    /* Use the same reversible note timeline as the piano. */
    unsigned elapsed=at;
    unsigned ph=s->sample_wave;
    s->sample_wave+=s->sample_reverse ? -s->increment : s->increment;
    int tail=(int)(frames-1-elapsed)*256/(int)frames;
    if(s->kind==RHODES) {
      /* 1.20 Rhodes: a struck tine and its tonebar ring with a pitch-dependent exponential
         decay; the tine's inharmonic bending mode (x5.9) and a short metallic tick give the
         attack; the magnetic pickup adds the asymmetric "bark" that grows with velocity
         (Pickup knob). Every envelope is a function of the note's elapsed time, so reverse
         notes still play backwards. */
      int dir=s->sample_reverse?-1:1, note=bound(s->note,24,108), vel=bound((s->strength-2800)/36,1,127);
      s->bell_phase+=dir*(int)(s->increment*5u+s->increment*9u/10u);
      unsigned tick_ph=ph*16u+(ph>>2);
      int t=(int)elapsed;
      int e_tine=rhodes_exp(t*(24+(note-24)*73/84)>>10);            /* T60 ~10 s low, ~2.5 s high */
      int e_bell=rhodes_exp(t*(600+(note-24)*3)>>10);                /* ~0.35 s */
      int e_tick=rhodes_exp(t*9700>>10);                            /* ~25 ms */
      int attack=bound(t,0,24)*256/24;                             /* 2 ms hammer contact */
      int drive=72+vel*(4+knob[TIMBRE][1])/8;                       /* how close the tine swings to the pickup */
      int amp=(int)((long long)127*e_tine*drive>>22);              /* +-127 at full */
      int x=sinus(ph)*amp/127+sinus(ph*2u)*amp/(18+knob[TIMBRE][1]*2);
      int bark=(x*x)/127-(amp*amp)/254;                            /* the pickup's even harmonics, DC removed */
      x+=bark*(knob[TIMBRE][1]+1)*vel/(127*10);
      int bell=sinus(s->bell_phase)*e_bell/32767*vel*(knob[TIMBRE][0]+2)/(127*6);
      int tick=sinus(tick_ph)*e_tick/32767*vel/300;
      sample=(x*66+bell*32+tick*20)*attack/256;
      sample=sample*bound(frames-1-elapsed,0,700)/700;
    } else if(s->kind==BOWED){
      s->bell_phase+=s->sample_reverse?-s->increment-(s->increment>>10):s->increment+(s->increment>>10);
      int bow=bound(elapsed,0,3400);
      sample=(sinus(ph)*42+sinus(s->bell_phase)*(12+4*knob[TIMBRE][1])+sinus(ph*3u)*bow/240)*bow/3400;
      sample=sample*bound(frames-1-elapsed,0,4200)/4200;
    } else if(s->kind==GLASS_BELL) {
      s->bell_phase+=s->sample_reverse ? -s->increment*3u-(s->increment>>1) :
                                         s->increment*3u+(s->increment>>1);
      sample=(sinus(ph)*64+sinus(s->bell_phase)*tail*(knob[TIMBRE][1]+1)/40)*tail/256;
      sample=sample*bound(elapsed,0,32)/32;
    } else {
      s->bell_phase+=s->sample_reverse ? -s->increment+(s->increment>>8) :
                                         s->increment-(s->increment>>8);
      int choir=sinus(ph)*32+sinus(s->bell_phase)*28+
                sinus(ph*3u)*(3+3*knob[TIMBRE][0])+sinus(s->bell_phase*5u)*(2*knob[TIMBRE][1]);
      sample=choir*bound(elapsed,0,2600)/2600*bound(frames-1-elapsed,0,7500)/7500;
    }
  }
  /* The felt damper darkens released notes; ENV sets their amplitude. */
  int tone=(s->gate || (pedal && s->kind<=BRIGHT))?s->piano_tone:s->piano_tone*(112-s->damper)/112;
  s->hammer_filter+=(sample-s->hammer_filter)*tone/256;
  s->phase+=486957u; /* 5 Hz tremolo */
  int amp=256-(127+sinus(s->phase))*s->tremolo/200;
  if(!s->sample_reverse && s->kind<=BRIGHT && s->sample_step>=((frames-1)<<shift)-s->sample_pos)s->sample_pos=(frames-1)<<shift;
  else if(s->sample_reverse ? s->sample_pos<s->sample_step :
      s->sample_step>=((frames-1)<<shift)-s->sample_pos) s->active=0;
  else s->sample_pos+=s->sample_reverse ? -s->sample_step : s->sample_step;
  return (s->hammer_filter*s->strength/127)*amp/256;
}

static AUDIO_FAST int envelope_tick(String *s) {
  if (!s->gate && !(pedal && (s->kind==PIANO || s->kind==BRIGHT))) {
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
/* RADIGUE MATRIX (1.20): four routings from slow sources to the drone.
   Sources: LFO A (sine or triangle), LFO B (a smooth random walk), the Wi-Fi source, note age
   (0 to full over about 20 s). Targets: Beat, Color, Sub, Drift, Pitch and Level per voice;
   Filter, Plate and Tape for the whole instrument. Spread sets each of the four voices
   apart: LFO A a quarter-turn per step, LFO B its own walk per voice. */
enum { MT_OFF, MT_BEAT, MT_COLOR, MT_SUB, MT_DRIFT, MT_PITCH, MT_LEVEL, MT_FILTER, MT_PLATE, MT_TAPE, MT_COUNT };
typedef struct { signed char src, dst, amount, spare; } MatrixSlot;
typedef struct {
  MatrixSlot slot[4];
  unsigned phase_a, phase_b, serial[4], age[4];
  int walk[4], walk_to[4];
  short voice[4][MT_COUNT];                       /* -127..127 per voice and target, updated each block */
  short global[MT_COUNT];
  unsigned char active;
} Matrix;
static Matrix mtx POOLMEM;
static const unsigned short mtx_rate_q8[8]={1,3,5,13,26,64,128,512};   /* Hz x 256: 0.005 .. 2 Hz */
static const char *const mtx_rate_name[8]={"0.005","0.01","0.02","0.05","0.1","0.25","0.5","2 Hz"};
static const char *const mtx_src_name[4]={"LFO A","LFO B","WI-FI","AGE"};
static const char *const mtx_dst_name[MT_COUNT]={"OFF","BEAT","COLOR","SUB","DRIFT","PITCH","LEVEL","FILTER","PLATE","TAPE"};
static void matrix_block(unsigned n){
  int on=knob[SOUND][2]==3;
  mtx.active=0;
  for(int i=0;i<4;i++)if(mtx.slot[i].dst && mtx.slot[i].amount)mtx.active=on;
  __builtin_memset(mtx.voice,0,sizeof mtx.voice);__builtin_memset(mtx.global,0,sizeof mtx.global);
  if(!mtx.active)return;
  int spread=knob[MLFO][2], tri=knob[MLFO][3];
  mtx.phase_a+=(unsigned)((unsigned long long)mtx_rate_q8[knob[MLFO][0]]*n*4294967296ull/(44100ull*256ull));
  unsigned before=mtx.phase_b;
  mtx.phase_b+=(unsigned)((unsigned long long)mtx_rate_q8[knob[MLFO][1]]*n*4294967296ull/(44100ull*256ull));
  int turn=mtx.phase_b<before;
  for(int v=0;v<4;v++){
    if(turn && (v==0 || spread))mtx.walk_to[v]=control_rnd()>>8;
    if(!spread && v)mtx.walk_to[v]=mtx.walk_to[0];
    mtx.walk[v]+=(mtx.walk_to[v]*256-mtx.walk[v])/48;
    if(strings[v].age!=(int)mtx.serial[v]){mtx.serial[v]=(unsigned)strings[v].age;mtx.age[v]=0;}
    else if(mtx.age[v]<3450u)mtx.age[v]++;          /* blocks of 5.8 ms: about 20 s */
    unsigned pa=mtx.phase_a+(unsigned)(spread*v)*0x10000000u;
    int la=tri?(pa<0x80000000u?(int)(pa>>23)-128:383-(int)(pa>>23)):sinus(pa);
    int src[4]={bound(la,-127,127),bound(mtx.walk[v]/256,-127,127),bound(rf_source*127/100,-127,127),(int)(mtx.age[v]*127u/3450u)};
    for(int i=0;i<4;i++){
      const MatrixSlot *m=&mtx.slot[i];
      if(!m->dst || !m->amount)continue;
      int x=src[m->src&3]*m->amount/7;
      mtx.voice[v][(int)m->dst]=(short)bound(mtx.voice[v][(int)m->dst]+x,-127,127);
    }
  }
  for(int d=MT_FILTER;d<MT_COUNT;d++)mtx.global[d]=mtx.voice[0][d];
}
static inline __attribute__((always_inline)) int matrix_at(const String *s,int dst){
  return mtx.active?mtx.voice[s-strings][dst]:0;
}
static const char *const motion_titles[]={"SOUNDBOARD","SOUNDBOARD","SHIMMER","CHORUS","TREMOLO","SHIMMER","ROTARY","AIR PULSE","ROTARY","BELLOWS","SLOW FADE","SLOW FADE","SLOW FADE","SLOW FADE","SHIMMER","CHORUS","AIR PULSE","SLOW FADE","CHORUS","AIR PULSE","AIR FLOW"};
static AUDIO_FAST int motion_sample(String *s,int sample){
  int depth=knob[MOTION][1];
  if(!depth)return sample;
  int kind=s->kind, rate=knob[MOTION][0];
  unsigned speed=kind>=BEATING?7300u:kind==HARMONIUM?53000u:kind==RHODES || kind==ORGAN || kind==REED_ORGAN?231000u:95000u;
  s->motion_phase+=speed<<rate;
  unsigned mp=s->motion_phase;int wave;
  switch(knob[MOTION][2]){                        /* Shape */
    case 1:wave=mp<0x80000000u?(int)(mp>>23)-128:383-(int)(mp>>23);break;
    case 2:wave=mp<0x80000000u?120:-120;break;
    case 3:if(mp<speed<<rate)s->motion_hold=rnd()>>8;wave=bound(s->motion_hold,-127,127);break;
    default:wave=sinus(mp);
  }
  if(knob[MOTION][3]){                             /* Delay: the motion fades in after the note starts */
    unsigned fade=(unsigned)knob[MOTION][3]*1378u;  /* 0.5 s per step at 16-sample ticks */
    if(s->lfo_age<fade)depth=depth*(int)s->lfo_age/(int)fade;
    if(!depth)return sample;
  }
  s->motion_filter+=(sample-s->motion_filter)/(4+4*rate);
  if(kind<=BRIGHT)return sample+depth*s->motion_filter/14;
  if(kind==GLASS_BELL || kind==TAPE_CHOIR || kind==BOWED)
    return sample+(s->motion_filter-sample)*((128+wave)*depth)/2048;
  return sample-sample*((128-wave)*depth)/2560;
}
static AUDIO_FAST int string_tick(String *s) {
  if (!s->active)
    return 0;
  int level=envelope_tick(s);
  if(!s->active) return 0;
  if(s->kind==AIR) return air_modal_tick(s)*level/32768;
  if(s->kind>=EVENING)return motion_sample(s,pad_tick(s))*level/32768;
  if(s->kind>=ORGAN) {
    if (!(s->pitch_clock++ & 15)) update_pitch(s);
    unsigned p=s->phase;
    s->phase+=s->increment;
    int color=rf_timbre(knob[TIMBRE][0]), body=knob[TIMBRE][1];
    if(s->kind>=BEATING){
      VoiceX *x=(VoiceX *)(void *)s->line;
      int drift=bound(knob[TIMBRE][3]+matrix_at(s,MT_DRIFT)*8/127,0,8);
      if(drift){                                 /* Drift: the beating wanders, a new course every 1.5 s */
        if(!(++x->clock&65535))x->drift_target=rnd();
        x->drift+=(x->drift_target-x->drift)>>12;
      }
      /* Independent oscillators accumulate phase; the beat rate follows the difference frequency. */
      if(mtx.active){color=bound(color+matrix_at(s,MT_BEAT)*8/127,0,8);body=bound(body+matrix_at(s,MT_COLOR)*8/127,0,8);
        int bend=matrix_at(s,MT_PITCH);   /* Pitch: +-50 cents */
        if(bend){unsigned d=(unsigned)((long long)(int)(s->increment>>8)*bend*7/127);s->phase+=d;s->sample_wave+=d;s->bell_phase+=d;} }
      s->sample_wave+=s->increment+(color+1)*39000u+(unsigned)((x->drift*drift)>>3);
      s->bell_phase+=s->kind==SUB_DRONE?s->increment/2:s->kind==FIFTH?s->increment+s->increment/2:s->increment-21000u;
      int wave=7*sinus(p)+5*sinus(s->sample_wave)+3*sinus(s->bell_phase);
      if(s->increment<0x18000000u)wave+=body*sinus(3u*p)/2;
      if(s->kind==VEIL)wave=wave*(192+sinus(p-s->sample_wave)/2)/256;
      { int sub=bound(knob[TIMBRE][2]+matrix_at(s,MT_SUB)*8/127,0,8);
        if(sub){x->sub_phase+=s->increment/2;wave+=sub*sinus(x->sub_phase)*3/2;} }   /* Sub: an octave below */
      if(mtx.active)wave=wave*bound(128+matrix_at(s,MT_LEVEL),0,256)/128;
      return (motion_sample(s,wave*s->strength/1024)*level)>>15;
    }
    VoiceX *x=(VoiceX *)(void *)s->line;
    int perc=knob[TIMBRE][2], leslie=knob[TIMBRE][3], rotor_depth=0;
    if(leslie){                                  /* Leslie: slow chorale (1-4) or fast tremolo (5-8) */
      x->rotor+=leslie>4?650000u:78000u;rotor_depth=(leslie-1)%4+1;
      p+=(unsigned)(sinus(x->rotor)*rotor_depth)<<19;
    }
    int wave=sinus(p)*8 + sinus(2u*p)*(s->kind==HARMONIUM?3:color);
    if(s->increment < 0x20000000u) wave+=sinus(3u*p)*(s->kind==REED?color+1:body);
    if(s->increment < 0x18000000u) wave+=sinus(5u*p)*(s->kind==REED?3:1);
    if(s->kind==REED || s->kind==HARMONIUM){s->neck+=(rnd()/128-s->neck)/8;wave+=s->neck*body;}
    if(s->kind==REED_ORGAN || s->kind==HARMONIUM){
      int slow=s->kind==HARMONIUM;
      s->sample_wave+=s->increment+(s->increment>>(slow?9:11));
      s->bell_phase+=slow?132000u:208000u;
      s->bloom=bound(s->bloom+1,0,slow?16000:3200);
      int breath=sinus(s->bell_phase);
      wave+=sinus(s->sample_wave)*6;
      if(s->increment<0x18000000u)wave+=sinus(4u*p)*(slow?2:4);
      if(!slow && s->bloom<3200)wave+=(rnd()/256)*(3200-s->bloom)/3200;
      wave=(wave*(slow?190+breath*color/12:235+breath/12)/256)*s->bloom/(slow?16000:3200);
    }
    if(perc){                                    /* Perc: a decaying third harmonic and a key click */
      wave+=sinus(3u*p)*perc*(x->perc>>8)/(8*16);x->perc-=x->perc>>13;
      if(x->click){x->click--;wave+=(rnd()>>6)*perc*x->click/(8*160);}
    }
    if(rotor_depth)wave=wave*(256-rotor_depth*(128+sinus(x->rotor+0x30000000u))/20)/256;
    return (motion_sample(s,wave*s->strength/1024)*level)>>15;
  }
  if(s->kind>=PIANO){
    int sample=piano_tick(s);
    if(s->kind>=GLASS_BELL && (knob[TIMBRE][2] || knob[TIMBRE][3])){
      VoiceX *x=(VoiceX *)(void *)s->line;
      int oct=knob[TIMBRE][2], sym=knob[TIMBRE][3];
      if(oct){                                   /* Octave: a pure partial an octave up, riding the voice's level */
        x->follow+=(absolute(sample)-x->follow)>>6;x->oct_phase+=s->increment*2u;
        sample+=sinus(x->oct_phase)*x->follow/127*oct/10;
      }
      if(sym){                                   /* Sympathetic: a string tuned to the note rings along */
        int16_t *buf=s->line+COMB_AT;int period=bound((s->delay+64)>>8,8,COMB_LEN-1);
        unsigned at=x->comb_at,from=at>=(unsigned)period?at-period:at+COMB_LEN-period;
        int d=buf[from],y=sample/4+(d*(27852+sym*491)>>15);
        buf[at]=(int16_t)bound(y,-30000,30000);x->comb_at=at+1>=COMB_LEN?0:at+1;
        sample+=d*sym/14;
      }
    }
    return (motion_sample(s,sample)*level)>>15;
  }
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

static AUDIO_FAST int read_transport(int sample, int wow, int flutter) {
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
static AUDIO_FAST int tape_record_level(int sample,int saturation) {
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

static unsigned head_delay=HEAD_SAMPLES;        /* OFF-TAPE Time: 20..180 ms */
static AUDIO_FAST void advance_head(int recorded) {
  unsigned back=HEAD_SAMPLES-head_delay;           /* 0 at 180 ms: the oldest sample, as before */
  int sample = head_count >= head_delay ? head_history[head_index+back<HEAD_SAMPLES?head_index+back:head_index+back-HEAD_SAMPLES] : 0;
  if (head_count < HEAD_SAMPLES) head_count++;
  head_history[head_index] = recorded;
  if (++head_index == HEAD_SAMPLES) head_index = 0;
  head_previous = head_filtered;
  head_filtered += (sample - head_filtered) * 3 / 4;
}
static volatile unsigned reverse, loop_mode, beat_ms;
static volatile int range_down;
static unsigned erased_count, erased_at[32];
static int erase_depth=100, erase_ramp=32;
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
static AUDIO_FAST int moving_head(int normal) {
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
static inline __attribute__((always_inline)) int saturate(int x) {
  int a = absolute(x);
  int y = 8192 - 67108864 / (8192 + a);
  return x < 0 ? -y : y;
}
#include "mt_plate.h"
static int wah_low,wah_band,wah_tone,fuzz_tone,amp_bass,amp_cab;
static int comp_envelope,comp_gain=4096;
static unsigned comp_clock;
#include "mt_tone_fx.h"
static AUDIO_FAST int instrument_fx(int sample) {
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
static int phrase_repeats, phrase_sub, phrase_next, phrase_note, phrase_vel;   /* ARP FEEL: repeats inside a step */
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
  phrase_step=phrase_left=phrase_gate=phrase_repeats=0;
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
  } else if (!phrase_size || !previous_size) {   /* adding or lifting a note keeps the arpeggio's place */
    phrase_reset();
    if(phrase_size)phrase_start();
  }
}
static volatile unsigned pick_display, pick_time;
static volatile int pick_struck;
/* RAM code reaches the flash only through pointers: a direct call is out of range. */
static void (*volatile strike_p)(int, int, int, int) = strike;
static void (*volatile glide_to_p)(String *, int) = glide_to;
static AUDIO_FAST void phrase_tick(void) {
  if(!phrase_size)return;
  if(phrase_wait){phrase_wait--;return;}
  if(phrase_gain<1048576)phrase_gain=bound(phrase_gain+phrase_rise_step,0,1048576);
  if(!phrase_left){
    int root=phrase_keys[0];
    int n=phrase_size==1 ? ((phrase_step&1)?scale_note_now(bound(root+(knob[SOUND][2]==0?knob[PICK_ART][3]:7),0,127)):root)
                         : phrase_keys[phrase_size-2+(phrase_step&1)];
    int velocity_note=phrase_size==1?root:n;
    if(knob[SOUND][2]){
      static const unsigned char order[][8]={{0,1,2,3,4,5,6,7},{0,1,2,3,2,1,0,1},{0,2,1,3,1,4,2,5},{0,3,1,4,2,5,1,3}};
      static const int intervals[][8]={{0,7,14,16,19,24,26,31},{0,2,5,7,9,12,14,17},{0,5,7,12,17,19,24,31}};
      int index=order[bound(knob[ARP][0]-1,0,3)][phrase_step];
      velocity_note=phrase_keys[index%phrase_size];
      n=scale_note_now(bound(phrase_size==1?root+intervals[bound(knob[SOUND][2]-1,0,2)][index]:velocity_note,0,127));
    }
    int left=phrase_size==1?root:phrase_keys[phrase_size-2];
    int right=phrase_size==1?scale_note_now(bound(root+knob[PICK_ART][3],0,127)):phrase_keys[phrase_size-1];
    pick_display=left | (right<<8) | ((phrase_step&1)<<16);
    pick_time=fm1_ms;
    pick_struck=!(knob[PICK_ART][2] && (phrase_step&1) && strings[0].gate);
    int eng=knob[SOUND][2], feel_accent=knob[FEEL][1];
    int accented=!eng || feel_accent==1 ? phrase_step==0 || phrase_step==3 || phrase_step==6
                : feel_accent==2 ? !(phrase_step&3) : feel_accent==3 ? !(phrase_step&1) : 0;
    int v=phrase_velocity[velocity_note]*(accented?100:100-knob[ARP][3]*3/5)/100;
    int tied=knob[SOUND][2]==0 && knob[PICK_ART][2];
    if(!eng)phrase_left=knob[ARP][1]>=3?44100/knob[ARP][1]:4410000/arp_hz100(knob[ARP][1]);
    else{
      /* ARP FEEL: swing lengthens the first of each pair, Human loosens time and touch,
         Wi-Fi can push the rate up to four times either way. */
      int rate=rf_mod(RF_RATE), hz100=arp_hz100(knob[ARP][1]);
      hz100=rate>0?hz100*(100+3*rate)/100:hz100*100/(100-3*rate);
      phrase_left=4410000/bound(hz100,5,6400);
      phrase_left+=phrase_left*knob[FEEL][0]/100*((phrase_step&1)?-1:1);
      int human=knob[FEEL][2];
      if(human){phrase_left+=phrase_left*human/100*(rnd()>>6)/(6*512);v+=human*(rnd()>>8)/800;}
      v=bound(v,1,127);
    }
    int density=eng?rf_mod(RF_DENSITY):0, rest=density<0 && (rnd()&127)*100<-density*128;
    if(rest){}
    else if(tied && (phrase_step&1) && strings[0].gate){
      glide_to_p(&strings[0],n);
      strings[0].glide_ticks=bound(strings[0].glide_ticks,1,phrase_left/48);
    } else strike_p(0,n,v?v:1,voice_kind_now());
    /* A picked transient must fit inside a fast gate, even with long manual swells. */
    if(strings[0].attack<8388352/(44*6))strings[0].attack=8388352/(44*6);
    if(strings[0].release_length>1323)strings[0].release_length=1323;
    phrase_gate=rest?0:tied && !(phrase_step&1)?0:phrase_left*knob[ARP][2]/100;
    phrase_repeats=0;
    if(eng && !rest){
      int repeats=knob[FEEL][3]+(density>0 && (rnd()&127)*100<density*128);
      if(repeats>4)repeats=4;
      if(repeats>1){
        phrase_sub=phrase_left/repeats;phrase_next=phrase_left-phrase_sub;phrase_repeats=repeats-1;
        phrase_note=n;phrase_vel=v*85/100;phrase_gate=phrase_sub*knob[ARP][2]/100;
      }
    }
    phrase_step=(phrase_step+1)&7;
  }
  if(phrase_repeats && phrase_left==phrase_next){
    strike_p(0,phrase_note,phrase_vel?phrase_vel:1,voice_kind_now());
    phrase_gate=phrase_sub*knob[ARP][2]/100;phrase_next-=phrase_sub;phrase_repeats--;
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

typedef struct {
  unsigned layer_length, sums[2], peaks[2];
  int layer_target, mix, decay, damping, predelay, wet, hf_coefficient, saturation, wow, flutter;
  int monitor_voice, monitor_mix, color_mod, level_mod, tape_rate, peak_in, peak_out;
  int fx_type, fx_post, fx_depth, fx_mix, off_level, off_tone, off_left, off_right, layer_reverse;
  unsigned fx_inc;
} AudioBlock;
/* ENSEMBLE page: string ensemble, chorus, phaser and flanger on the instrument,
   before the tape, in stereo. One 23 ms line in external RAM. */
typedef struct { int16_t ring[1024]; unsigned w, phase, slow; int ap[2][4], apx[2][4], fb[2], head_lp; } ModFx;
static ModFx modfx POOLMEM;
static inline __attribute__((always_inline)) int fx_read(const ModFx *f,int delay_q8){
  unsigned at=f->w-(unsigned)(delay_q8>>8);
  int a=f->ring[at&1023],b=f->ring[(at-1)&1023];
  return a+((b-a)*(delay_q8&255)>>8);
}
static AUDIO_FAST void mod_fx(int x,int *l,int *r,int type,unsigned inc,int depth,int mix){
  ModFx *f=&modfx;
  f->phase+=inc;f->slow+=inc*7u;
  int sl=sinus(f->phase),sr=sinus(f->phase+0x40000000u),wl,wr;
  if(type==3){                                   /* PHASER: four first-order all-pass stages per side */
    for(int ch=0;ch<2;ch++){
      int lfo=ch?sr:sl, a=19000+lfo*depth*95/100, in=bound(x+f->fb[ch]/2,-32767,32767);
      for(int st=0;st<4;st++){
        int y=(int)(((long long)a*(in-f->ap[ch][st]))>>15)+f->apx[ch][st];
        f->apx[ch][st]=in;f->ap[ch][st]=bound(y,-32767,32767);in=f->ap[ch][st];
      }
      f->fb[ch]=in;
      if(ch)wr=(x+in)/2;else wl=(x+in)/2;
    }
  }else{
    int fb=type==4?f->fb[0]*9/16:0;
    f->ring[f->w&1023]=(int16_t)bound(x+fb,-32767,32767);
    if(type==1){                                 /* ENSEMBLE: three taps, 120 degrees apart, slow and fast */
      int s3=sinus(f->phase+0x55555555u),s2=sinus(f->phase+0xAAAAAAAAu),fast=sinus(f->slow)/6;
      int sw=depth*290;                           /* q8 samples: up to 2.6 ms each way around 8 ms */
      int t1=fx_read(f,(353<<8)+(sl+fast)*sw/148),t2=fx_read(f,(353<<8)+(s3+fast)*sw/148),t3=fx_read(f,(353<<8)+(s2+fast)*sw/148);
      wl=(t1+t2)/2;wr=(t2+t3)/2;
    }else if(type==2){                           /* CHORUS: one line, two taps in opposite phase */
      int sw=depth*169;                           /* 4.5 ms, 1.5 ms each way */
      wl=fx_read(f,(198<<8)+sl*sw/127);wr=fx_read(f,(198<<8)-sl*sw/127);
    }else{                                       /* FLANGER: short swept line with feedback */
      int sw=depth*225;                           /* 2.5 ms, 2 ms each way */
      wl=fx_read(f,(112<<8)+sl*sw/127);wr=fx_read(f,(112<<8)-sl*sw/127);
      f->fb[0]=wl;
      wl=(x+wl)/2;wr=(x+wr)/2;
    }
    f->w++;
  }
  *l=x+(wl-x)*mix/100;*r=x+(wr-x)*mix/100;
}
/* The per-sample work: voices, instrument effects, tape and plate. It runs from RAM. */
static AUDIO_FAST void audio_loop(int32_t *out, uint32_t n, AudioBlock *b) {
  unsigned layer_length=b->layer_length; int layer_target=b->layer_target;
  int decay=b->decay, damping=b->damping, predelay=b->predelay, wet=b->wet;
  int hf_coefficient=b->hf_coefficient, saturation=b->saturation, wow=b->wow, flutter=b->flutter;
  int monitor_voice=b->monitor_voice, monitor_mix=b->monitor_mix;
  int color_mod=b->color_mod, level_mod=b->level_mod, tape_rate=b->tape_rate;
  int fx_type=b->fx_type, fx_post=b->fx_post, fx_depth=b->fx_depth, fx_mix=b->fx_mix; unsigned fx_inc=b->fx_inc;
  int off_level=b->off_level, off_tone=b->off_tone, off_left=b->off_left, off_right=b->off_right, layer_reverse=b->layer_reverse;
  int peak_in=0, peak_out=0;
  unsigned sums0=0, sums1=0, peaks0=0, peaks1=0;
  unsigned scope_write=scope_head;
  struct { int mix; } tone = { b->mix };
  for (unsigned i = 0; i < n; i++) {
    if(phrase_enabled)phrase_tick();
    int dry=0, bridge=0;
    for(int j=0;j<4;j++){
      int voice=string_tick(&strings[j]);dry+=voice;
      if(strings[j].kind==PIANO || strings[j].kind==BRIGHT)bridge+=voice;
    }
    int body=board_tick(bridge,knob[TIMBRE][3]);dry+=body;
    if(monitor_voice)scope_ring[scope_write++ & 1023]=bound(dry,-32767,32767);
    int shape = voice_kind_now(), drive = knob[SOUND][1];
    int amp = shape >= SINGING ? dry : saturate(dry * (shape == 0 ? 1 + drive / 35 : 4 + drive / 9)) * 2;
    amp_lp += (amp - amp_lp) * (shape >= SINGING ? 2 : shape == 3 ? 5 : 3) / 8;
    int colored=amp_lp;
    if(color_mod)colored=bound(amp_lp+(amp-amp_lp)*color_mod/90,-32767,32767);
    dry=tone_process(instrument_fx(colored),tone.mix);
    if(level_mod)dry=dry*bound(100+level_mod,0,200)/100;   /* down to silence: a gate */
    /* IIR stages can retain a small DC residue after the voice has ended. */
    if(body || strings[0].active || strings[1].active || strings[2].active || strings[3].active)
      silent_input=0;
    else if(silent_input<1024) silent_input++;
    else dry=0;
    if(phrase_enabled && phrase_wait) dry=0;
    else if(phrase_enabled && phrase_gain<1048576) dry=dry*(phrase_gain>>10)/1024;
    int dl=dry, dr=dry;
    if(fx_type && !fx_post)mod_fx(dry,&dl,&dr,fx_type,fx_inc,fx_depth,fx_mix);
    int send = (fx_type && !fx_post?(dl+dr)/2:dry) * knob[HOME][2] / 100, ret = 0;
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
        if ((reverse && transport==HOLD) ^ layer_reverse)
          layer_pos = layer_pos ? layer_pos-1 : layer_length-1;
        else if (++layer_pos >= layer_length) layer_pos=0;
        if(range_down){
          unsigned slot=erased_count&31;
          erased_at[slot]=position;erased_old[slot]=position<valid_samples?tape[position]:0;
          { int keep=erased_old[slot]*(100-erase_depth)/100, ramp=erase_ramp;   /* ERASE Depth and Soft */
            tape[position]=erased_count<(unsigned)ramp?keep+(erased_old[slot]-keep)*(ramp-1-(int)erased_count)/ramp:keep; }
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
    if(off_tone<256){modfx.head_lp+=(head-modfx.head_lp)*off_tone/256;head=modfx.head_lp;}
    int common = ret * knob[HOME][3] / 100, off = head * off_level / 100;
    int mix_l = dl + common + off*off_left/100, mix_r = dr + common + off*off_right/100;
    if(fx_type && fx_post){                       /* POST: the whole monitor (instrument, tape, off-tape) is modulated */
      int centre=(mix_l+mix_r)/2,pl,pr;
      mod_fx(centre,&pl,&pr,fx_type,fx_inc,fx_depth,fx_mix);
      mix_l=pl+(mix_l-centre);mix_r=pr+(mix_r-centre);
    }
    int mix = (mix_l==mix_r)?mix_l:(mix_l+mix_r)/2;
    if (plate_half) plate_tick((plate_input+mix)/4,decay,damping,predelay);
    plate_input=mix;
    int l=plate_half?(wet_l+prev_l)/2:wet_l;
    int r=plate_half?(wet_r+prev_r)/2:wet_r;
    plate_half^=1;
    out[2*i]=bound(mix_l+l*wet/65,-31500,31500);
    out[2*i+1]=bound(mix_r+r*wet/65,-31500,31500);
    if(monitor_mix)for(int ch=0;ch<2;ch++){
      unsigned v=absolute(out[2*i+ch]);
#if defined(GRANA_WEB)
      v=v*(unsigned)web_master>>15;
#elif !defined(DINGHY_HOST)
      v=v*(unsigned)master_q12>>12;
#endif
      if(ch){sums1+=v;if(v>peaks1)peaks1=v;}else{sums0+=v;if(v>peaks0)peaks0=v;}
    }
  }
  scope_head=scope_write;
  b->peak_in=peak_in; b->peak_out=peak_out;
  b->sums[0]=sums0; b->sums[1]=sums1; b->peaks[0]=peaks0; b->peaks[1]=peaks1;
}
static void (*volatile audio_loop_p)(int32_t *, uint32_t, AudioBlock *) = audio_loop;
static volatile unsigned audio_ready;
/* 1.20 load governor: above 85 % of the time a block allows, the oldest voice goes; four blocks over 95 %
   turn off the ensemble until the load is back under 70 %. A click is better than a frozen, restarting FM-1. */
static unsigned governor_hits, governor_fx_off;
static void load_governor(void){
#if !defined(GRANA_WEB) && !defined(DINGHY_HOST)
  unsigned load=audio_load_q8;
  if(load>218u){
    int oldest=-1,live=0;
    for(int i=0;i<4;i++)if(strings[i].active){live++;if(oldest<0 || strings[i].age<strings[oldest].age)oldest=i;}
    if(live>1){strings[oldest].active=0;governor_hits++;}
    if(load>243u && governor_fx_off<4)governor_fx_off++;
  }else if(load<180u && governor_fx_off)governor_fx_off=0;
#endif
}
static void app_audio(int32_t *out, uint32_t n) {
  load_governor();
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
  unsigned layer_length = knob[HEAD2][2]==73 ? length * 731u / 1000u : length * (unsigned)knob[HEAD2][2] / 100u;
  int layer_target = knob[SOUND][2]==1 && knob[HEAD2][0] ? 256*knob[HEAD2][1]/100 : 0;
  ToneControls tone=tone_controls();
  matrix_block(n);
  if(mtx.active && mtx.global[MT_FILTER]){int f=mtx.global[MT_FILTER]*60/127;if(!tone.mode){tone.mode=1;tone.mix=100;tone.cutoff=70;}tone.cutoff=bound(tone.cutoff+f,0,100);}   /* MATRIX Filter */
  { int f=rf_mod(RF_FILTER);                        /* the Wi-Fi opens a low-pass even when FILTER is off */
    if(f){if(!tone.mode){tone.mode=1;tone.mix=100;}tone.cutoff=bound(tone.cutoff+f*70/100,0,100);} }
  tone_prepare(tone);
  AudioBlock b;
  b.layer_length=layer_length; b.layer_target=layer_target; b.mix=tone.mix;
  b.decay=100+knob[PLATE][1]*148/100;
  b.damping=235-knob[PLATE][2]*215/100;
  b.predelay=knob[PLATE][3]*2205/100; b.wet=bound(knob[PLATE][0]+rf_mod(RF_PLATE),0,100);
  b.decay=bound(b.decay+rf_mod(RF_PLATE)/2,100,248);
  if(mtx.active && mtx.global[MT_PLATE])b.wet=bound(b.wet+mtx.global[MT_PLATE]*50/127,0,100);   /* MATRIX Plate */
  b.hf_coefficient=256-2*knob[CHARACTER][0]; b.saturation=knob[CHARACTER][1];
  { int t=absolute(rf_mod(RF_TAPE))+(mtx.active?absolute(mtx.global[MT_TAPE])*60/127:0);b.wow=bound(knob[CHARACTER][2]+t,0,100);b.flutter=bound(knob[CHARACTER][3]+t/2,0,100); }
  b.monitor_voice=page==SOUND; b.monitor_mix=page==CHARACTER;
  b.color_mod=rf_mod(RF_COLOR); b.level_mod=rf_mod(RF_LEVEL);
  { /* SPEED Glide eases the reels to a new speed; Fine trims it by up to 5 %. */
    static int rate_q8;int target=speed_q8[bound(speed_index(),0,96)]*(1000+knob[SPEED][3])*256/1000;
    if(!rate_q8 || !knob[SPEED][2])rate_q8=target;
    else{int step=(target-rate_q8)*(int)n/(int)(knob[SPEED][2]*882+1);if(!step)step=target>rate_q8?1:target<rate_q8?-1:0;rate_q8+=step;}
    b.tape_rate=(rate_q8+128)>>8; }
  { /* LFO 2, block rate */
    static int hold,smooth;unsigned phase=lfo2_phase,before=phase;
    phase+=(unsigned)((unsigned long long)lfo2_hz100(knob[LFO2][0])*n*4294967296ull/4410000ull);
    if(phase<before){hold=control_rnd()>>8;}
    smooth+=(hold-smooth)/6;
    int w;switch(knob[LFO2][1]){
      case 1:w=phase<0x80000000u?(int)(phase>>23)-128:383-(int)(phase>>23);break;
      case 2:w=(int)(phase>>24)-128;break;
      case 3:w=127-(int)(phase>>24);break;
      case 4:w=phase<0x80000000u?127:-127;break;
      case 5:w=hold;break;
      case 6:w=smooth;break;
      default:w=sinus(phase);
    }
    lfo2_amount=bound(w,-127,127)*knob[LFO2][3]/127;lfo2_phase=phase;lfo2_wave=bound(w,-127,127); }
  b.fx_type=governor_fx_off>=4?0:knob[MODFX][0]>4?knob[MODFX][0]-4:knob[MODFX][0]; b.fx_post=knob[MODFX][0]>4;   /* 1.20: 5-8 run after the tape */
  { int rate=bound(knob[MODFX][1]+rf_mod(RF_FXRATE),0,100);
    unsigned hz100=5u<<(rate/10);hz100+=hz100*(unsigned)(rate%10)/10;           /* 0.05 Hz .. about 50 Hz, doubling every 10 */
    if(hz100>1000)hz100=1000;
    b.fx_inc=(unsigned)((unsigned long long)hz100*4294967296ull/4410000ull);
    b.fx_depth=bound(knob[MODFX][2]+rf_mod(RF_FXDEPTH),0,100);b.fx_mix=knob[MODFX][3]; }
  b.off_level=knob[OFF_TAPE][0];b.off_tone=knob[OFF_TAPE][1]>=100?256:32+knob[OFF_TAPE][1]*224/100;
  b.off_left=knob[OFF_TAPE][2]<=50?100:(100-knob[OFF_TAPE][2])*2;b.off_right=knob[OFF_TAPE][2]>=50?100:knob[OFF_TAPE][2]*2;
  head_delay=bound(knob[OFF_TAPE][3],20,180)*TAPE_RATE/1000u;if(knob[OFF_TAPE][3]>=180)head_delay=HEAD_SAMPLES;
  b.layer_reverse=knob[HEAD2][3];
  erase_depth=knob[ERASE][2];erase_ramp=32+knob[ERASE][3]*14;
  { static int target,clock;                       /* RING MOD Drift walk */
    if(++clock>=344){clock=0;target=(control_rnd()*2458)>>15;}
    air_drift+=(target-air_drift)/64; }
  b.peak_in=b.peak_out=0; b.sums[0]=b.sums[1]=b.peaks[0]=b.peaks[1]=0;
  audio_loop_p(out,n,&b);
  int peak_in=b.peak_in, peak_out=b.peak_out, monitor_mix=b.monitor_mix;
  unsigned *sums=b.sums, *peaks=b.peaks;

  if(monitor_mix && n)for(int ch=0;ch<2;ch++){
    int mean=sums[ch]/n;
    meter_level[ch]+=(mean-meter_level[ch])*(int)n/(int)(4410+n);
    if(peaks[ch]>=29204){meter_peak_time[ch]=fm1_ms;meter_peak_valid|=1u<<ch;}
  }
  vu_in = peak_in > vu_in ? peak_in : vu_in * 9 / 10;
  vu_out = peak_out > vu_out ? peak_out : vu_out * 9 / 10;
}
static const uint16_t silver = RGB(191, 192, 179), metal = RGB(124, 128, 122),
                      ink = RGB(5, 5, 4), gold = RGB(247, 187, 88);
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
static const char *rf_target_name(int t);
static const char *rf_target(void){
  return rf_target_name(knob[ROUTE][0]);
}
static const char *rf_target_name(int t){
  if(t)return ((const char *const[]){"","Color","Level","Pitch","Filter","Tape","Plate","Arp Rate","Density","FX Rate","FX Depth"})[t];
  int kind=voice_kind();
  if(kind==AIR)return "Material";
  if(kind>=EVENING)return timbre_labels[kind-PIANO][0];
  if(kind>=ORGAN)return timbre_labels[kind-PIANO][0];
  return kind<PIANO?"Pick":"Touch";
}
static void signed_text(char *s,int v,char unit){s[0]=v<0?'-':'+';number(s+1,absolute(v));if(unit){int n=0;while(s[n])n++;s[n]=unit;s[n+1]=0;}}
static void tenths_text(char *s,int v,const char *unit){   /* v in hundredths */
  if(v>=100){number(s,v/100);int n=0;while(s[n])n++;if(v<1000 && v%100){s[n++]='.';s[n++]='0'+v/10%10;}s[n]=0;}
  else{s[0]='.';s[1]='0'+v/10;s[2]=v%10?'0'+v%10:0;s[3]=0;}
  int n=0;while(s[n])n++;while(*unit)s[n++]=*unit++;s[n]=0;
}
static const char *value_text(int k, char *s) {
  int v = knob[page][k];
  if(page==SCALE && k>=2){signed_text(s,v,k==3?'c':0);return s;}
  if(page==BT && k>=2)return v?"ON":"OFF";
  if(page==OFF_TAPE && k==2){if(v==50)return "C";s[0]=v<50?'L':'R';number(s+1,absolute(v-50)*2);return s;}
  if(page==LIGHTS && k==1)return v?"RELEASE":"RESTART";
  if(page==LIGHTS && k==3){tenths_text(s,v*10,"s");return s;}
  if(page==LIGHTS && k==2){number(s,v);return s;}
  if(page==HEAD2 && k==3)return v?"REV":"FWD";
  if(page==HEAD2 && k==2){number(s,v);int n=0;while(s[n])n++;s[n]='%';s[n+1]=0;return s;}
  if(page==HEAD2 && k==1){number(s,v);return s;}
  if(page==ERASE && k==3){number(s,2+v);int n=0;while(s[n])n++;s[n++]='m';s[n++]='s';s[n]=0;return s;}
  if(page==ERASE && k==2){number(s,v);return s;}
  if(page==SPEED && k==2){number(s,v*20);int n=0;while(s[n])n++;s[n++]='m';s[n++]='s';s[n]=0;return s;}
  if(page==SPEED && k==3){s[0]=v<0?'-':'+';number(s+1,absolute(v)/10);int n=0;while(s[n])n++;s[n++]='.';s[n++]='0'+absolute(v)%10;s[n++]='%';s[n]=0;return s;}
  if(page==MOTION && k==2)return ((const char *const[]){"Sine","Tri","Square","Random"})[v];
  if(page==MOTION && k==3){if(!v)return "OFF";tenths_text(s,v*50,"s");return s;}
  if(page==LFO2){
    if(k==0){tenths_text(s,lfo2_hz100(v),"");return s;}
    if(k==1)return ((const char *const[]){"Sine","Tri","Saw Up","Saw Dn","Square","S&H","Smooth"})[v];
    if(k==2)return rf_target_name(v);
    signed_text(s,v,0);return s;
  }
  if(page==MODFX && k==0)return ((const char *const[]){"OFF","ENS PRE","CHO PRE","PHA PRE","FLA PRE","ENS POST","CHO POST","PHA POST","FLA POST"})[v];
  if(page==RING && k==1)return ((const char *const[]){"1:2","3:4","1:1","5:4","3:2","2:1","5:2","3:1","3.71"})[v];
  if(page==RING && k==2)return ((const char *const[]){"STRIKE","BOW","SCRAPE"})[v];
  if(page==MATRIX){if(k==0){s[0]=(char)('1'+v);s[1]=0;return s;}if(k==1)return mtx_src_name[v&3];if(k==2)return mtx_dst_name[bound(v,0,MT_COUNT-1)];signed_text(s,v,0);return s;}
  if(page==MLFO){if(k<2)return mtx_rate_name[v&7];if(k==3)return v?"TRI":"SINE";}
  if(page==UNCERT){if(!k)return v?"ON":"OFF";if(k==1)return ((const char *const[]){"Drift","Steps","Pulse"})[v];if(k==2 && !v)return "Auto";}
  if(page==ROUTE){if(!(k&1))return rf_target_name(v);s[0]=v<0?'-':'+';number(s+1,absolute(v));return s;}
  if(page==FEEL){
    if(k==0){if(!v)return "Straight";number(s,50+v/2);int n=0;while(s[n])n++;s[n]='%';s[n+1]=0;return s;}
    if(k==1)return ((const char *const[]){"OFF","3+3+2","4","2"})[v];
    if(k==3){s[0]='x';s[1]='0'+v;s[2]=0;return s;}
  }
  if(page==ARP && k==1 && v<3)return ((const char *const[]){"2.5","2","1.5","1.25","1",".75",".5",".33",".25",".2",".1",".05"})[bound(2-v,0,11)];
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
  if(page==NOTE_FX && !k)return v?"REV":"FWD";
  if(page==ARP && knob[SOUND][2] && !k)return patterns[v];
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
      return knob[SOUND][2]==4 ? "FIELD" : knob[SOUND][2]==3 ? drones[v] : knob[SOUND][2]==2 ? riley_voices[v] : knob[SOUND][2]==1 ? ambient_voices[v] : voices[v];
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
#include "eno_panel.h"
static unsigned glyph_index(char c){return c>=32 && c<=126?(unsigned)(c-32):31;}
static void panel_text(int x,int y,const char *s,uint16_t fg,int limit,unsigned face){
  int width=0;
  for(const char *p=s;*p;p++)width+=panel_adv[face+glyph_index(*p)];
  if(!width)return;
  if(width>limit && face){            /* too wide: the small face before any squeezing */
    int small=0;for(const char *p=s;*p;p++)small+=panel_adv[glyph_index(*p)];
    if(small<=limit || small<width){face=0;width=small;}
  }
  int out=width<limit?width:limit, left=x<0?-x:x-out/2, pen=0;
  for(;*s;s++){
    unsigned glyph=face+glyph_index(*s),advance=panel_adv[glyph];
    int start=pen*out/width,end=(pen+advance)*out/width;
    for(int col=start;col<end;col++){
      int px=left+col;if(px<0 || px>=(int)cv_w)continue;
      unsigned src=(col-start)*advance/(end-start);
      unsigned rows=panel_rows[glyph],first=(rows>>4)*2;
      for(unsigned row=first;row<(rows&15)*2;row++){
        if((unsigned)(y+row)>=cv_h)continue;
        unsigned i=(row-first)*advance+src, pair=panel_alpha[panel_off[glyph]+i/2];
        unsigned t=(i&1)?pair&15:pair>>4;if(!t)continue;
        uint16_t *p=&cv_px[(y+row)*cv_w+px],bg=swap16(*p);
        unsigned red=((fg>>11)*t+(bg>>11)*(15-t)+7)/15;
        unsigned green=(((fg>>5)&63)*t+((bg>>5)&63)*(15-t)+7)/15;
        unsigned blue=((fg&31)*t+(bg&31)*(15-t)+7)/15;
        *p=swap16((red<<11)|(green<<5)|blue);
      }
    }
    pen+=advance;
  }
}
static void lettering(int x,int y,const char *s,uint16_t fg,uint16_t bg){
  (void)bg;
  unsigned face=(cv_h==146 && x==120 && y>=0 && y<=12)?95:0;
  panel_text(x,y,s,fg,240,face);
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
static unsigned latch_keys, latch_armed;
static void release_hold(unsigned keys);
#include "mt_presets.h"
static unsigned intro_start;
static int intro;
enum { INTRO_END_MS = 5600 };
static unsigned intro_ease(unsigned elapsed,unsigned duration) {
  if(elapsed>=duration)return 255;
  unsigned x=elapsed*255/duration;
  return x*x*(765-2*x)/65025;
}
/* PackBits (tools/pack_intro.py): n < 128 copies n+1 pixels, n >= 128 repeats one n-125 times. */
static void unpack_intro(const unsigned char *src,const uint16_t *palette,unsigned count){
  uint16_t *dst=cv_px,*end=cv_px+count;
  while(dst<end){
    unsigned n=*src++;
    if(n<128){for(n++;n && dst<end;n--)*dst++=palette[*src++];}
    else{uint16_t c=palette[*src++];for(n-=125;n && dst<end;n--)*dst++=c;}
  }
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
  unpack_intro(intro_art,colors,240*136);
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
  unpack_intro(intro_title,letters,240*22);

  cv_blit(0,188);
}

/* The operating display uses the unlettered signal diagram from the intro.
 * Transport phase is shared with the decks so a direction change retraces it. */
static void eno_diagram(int gap, unsigned clock) {
  int left=69-gap, right=171+gap;
  uint16_t frame=RGB(151,128,91), wire=RGB(222,196,147);
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
/* Antialiased drawing on the canvas itself: no box, no different background. Units of 1/16 px. */
static void aa_px(int x,int y,uint16_t c,int a){
  y+=cv_oy;
  if(x<0 || y<0 || x>=(int)cv_w || y>=(int)cv_h || a<=0)return;
  uint16_t *p=&cv_px[(unsigned)y*cv_w+(unsigned)x];
  if(a>=256){*p=swap16(c);return;}
  unsigned d=swap16(*p);
  unsigned r=((d>>11)*(256-a)+(c>>11)*a)>>8,g=(((d>>5)&63)*(256-a)+((c>>5)&63)*a)>>8,b=((d&31)*(256-a)+(c&31)*a)>>8;
  *p=swap16((uint16_t)(r<<11|g<<5|b));
}
static int isqrt32(unsigned v){unsigned r=0,bit=1u<<30;while(bit>v)bit>>=2;while(bit){if(v>=r+bit){v-=r+bit;r=(r>>1)+bit;}else r>>=1;bit>>=2;}return (int)r;}
static int coverage(int inside16){return inside16<=-8?0:inside16>=8?256:(inside16+8)*16;}
static void aa_ring(int cx,int cy,int r,int w,uint16_t c,int alpha){   /* centre and radius in 1/16 px */
  int reach=(r+w)/16+2;
  for(int y=cy/16-reach;y<=cy/16+reach;y++)for(int x=cx/16-reach;x<=cx/16+reach;x++){
    int dx=x*16+8-cx,dy=y*16+8-cy,d=isqrt32((unsigned)(dx*dx+dy*dy));
    aa_px(x,y,c,coverage(w/2-absolute(d-r))*alpha>>8);
  }
}
static void aa_disc(int cx,int cy,int r,uint16_t c,int alpha){
  int reach=r/16+2;
  for(int y=cy/16-reach;y<=cy/16+reach;y++)for(int x=cx/16-reach;x<=cx/16+reach;x++){
    int dx=x*16+8-cx,dy=y*16+8-cy;
    aa_px(x,y,c,coverage(r-isqrt32((unsigned)(dx*dx+dy*dy)))*alpha>>8);
  }
}
static void aa_line(int x0,int y0,int x1,int y1,int w,uint16_t c){
  int lo_x=(x0<x1?x0:x1)-w,hi_x=(x0>x1?x0:x1)+w,lo_y=(y0<y1?y0:y1)-w,hi_y=(y0>y1?y0:y1)+w;
  int vx=x1-x0,vy=y1-y0,len2=vx*vx+vy*vy;
  for(int y=lo_y/16-1;y<=hi_y/16+1;y++)for(int x=lo_x/16-1;x<=hi_x/16+1;x++){
    int px=x*16+8-x0,py=y*16+8-y0,t=len2?(px*vx+py*vy)*256/len2:0;
    t=bound(t,0,256);
    int ex=px-vx*t/256,ey=py-vy*t/256;
    aa_px(x,y,c,coverage(w/2-isqrt32((unsigned)(ex*ex+ey*ey))));
  }
}
/* TAPE SPEED: a Revox A77 control knob drawn on the black panel itself, no box behind it.
   A black fluted skirt that turns with the speed, a turned-aluminium cap (two soft
   reflections opposite each other, fine lathe rings), a cream index line, and a printed
   scale with the four standard speeds. */
static void a77_knob(int cx,int cy,int angle){
  enum { R_SKIRT=36, R_CAP=23 };
  for(int y=cy-R_SKIRT-1;y<=cy+R_SKIRT+1;y++)for(int x=cx-R_SKIRT-1;x<=cx+R_SKIRT+1;x++){
    int dx=x-cx,dy=y-cy,d2=dx*dx+dy*dy,d16=isqrt32((unsigned)(d2*256+(dx+dy)*16+128));
    int edge=coverage(R_SKIRT*16-d16);
    if(edge<=0)continue;
    int r,g,b;
    if(d16<=R_CAP*16+8){                         /* the cap: conical reflections and lathe rings */
      int refl=d2?-2*dx*dy*128/d2:0;              /* -128..128, bright towards top-left and bottom-right */
      int lum=150+refl*60/128+((d16>>4)%3==0?-10:4)-(d16>>4)*1;
      if(d16>R_CAP*16-24)lum-=46;                 /* the chamfer */
      lum=bound(lum,40,235);r=lum;g=lum;b=lum-6;
      int c=coverage(R_CAP*16-d16);               /* blend into the skirt at the rim */
      int s=26;r=(r*c+s*(256-c))>>8;g=(g*c+s*(256-c))>>8;b=(b*c+s*(256-c))>>8;
    }else{                                        /* the skirt: glossy black, lit from above */
      int lum=20-dy*10/R_SKIRT;
      if(d16>(R_SKIRT-3)*16)lum+=10;              /* the rounded outer lip */
      r=g=b=bound(lum,8,48);
    }
    aa_px(x,y,RGB(r,g,b),edge);
  }
  for(int i=0;i<36;i++){                          /* the flutes, turning with the knob */
    int a=(angle+i*256/36)&255,sx=sine256[a],sy=-sine256[(a+64)&255];
    int lit=sy<0?96:60;
    aa_line(cx*16+sx*(R_CAP+4)*16/127,cy*16+sy*(R_CAP+4)*16/127,cx*16+sx*(R_SKIRT-2)*16/127,cy*16+sy*(R_SKIRT-2)*16/127,14,RGB(lit,lit,lit-2));
  }
  int sx=sine256[angle&255],sy=-sine256[(angle+64)&255];
  aa_line(cx*16+sx*(R_CAP+2)*16/127,cy*16+sy*(R_CAP+2)*16/127,cx*16+sx*(R_SKIRT-1)*16/127,cy*16+sy*(R_SKIRT-1)*16/127,40,RGB(243,236,211));
  aa_line(cx*16+sx*5*16/127,cy*16+sy*5*16/127,cx*16+sx*(R_CAP-4)*16/127,cy*16+sy*(R_CAP-4)*16/127,28,RGB(58,56,52));
}
static void speed_dial(void){
  enum { CX=120, CY=70 };
  for(int i=0;i<=96;i+=4){                        /* the printed scale */
    int a=(-96+i*2+256)&255,sx=sine256[a],sy=-sine256[(a+64)&255],major=!(i%32);
    int r0=major?41:43,r1=major?49:46;
    aa_line(CX*16+sx*r0*16/127,CY*16+sy*r0*16/127,CX*16+sx*r1*16/127,CY*16+sy*r1*16/127,major?24:12,major?silver:RGB(120,118,108));
  }
  { static const char *const mark[4]={"4.75","9.5","19","38"};
    for(int i=0;i<4;i++){
      int a=(-96+i*64+256)&255,sx=sine256[a],sy=-sine256[(a+64)&255];
      lettering(CX+sx*64/127,CY+sy*60/127-8,mark[i],speed_index()==i*32?gold:silver,0);
    } }
  a77_knob(CX,CY,((-96+speed_index()*2)+256)&255);
  lettering(120,123,"TAPE SPEED",silver,0);
}
static unsigned midi_seen;                          /* last MIDI input, for the connector's pins */
/* MIDI: a 5-pin DIN socket in outline; the pins light while MIDI arrives. */
static void midi_icon(int cx,int cy){
  int live=fm1_ms-midi_seen<180;
  aa_ring(cx*16,cy*16,40*16,40,silver,256);
  aa_ring(cx*16,cy*16,34*16,16,RGB(110,104,92),256);
  aa_ring(cx*16,(cy+36)*16,5*16,24,silver,256);                       /* the key notch */
  for(int i=0;i<5;i++){
    static const signed char px[]={-22,-16,0,16,22},py[]={0,-16,-22,-16,0};
    int x=(cx+px[i])*16,y=(cy+py[i]+2)*16;
    if(live)aa_disc(x,y,9*16,RGB(120,84,30),200);
    aa_disc(x,y,4*16+8,live?gold:RGB(150,128,90),256);
    aa_disc(x,y,2*16,ink,256);
  }
}
/* KEYS: an outlined bulb; the filament and its glow follow the key light. */
static void bulb_icon(int cx,int cy,int level){
  uint16_t glass=RGB(185,187,173), warm=level==2?RGB(255,226,140):level?RGB(232,176,92):RGB(96,84,70);
  if(level)for(int r=36;r>=26;r-=2)aa_disc(cx*16,cy*16,r*16,RGB(110,74,28),level==2?40:22);
  aa_ring(cx*16,cy*16,24*16,28,glass,256);
  aa_line((cx-12)*16,(cy+21)*16,(cx-9)*16,(cy+33)*16,28,glass);
  aa_line((cx+12)*16,(cy+21)*16,(cx+9)*16,(cy+33)*16,28,glass);
  for(int i=0;i<3;i++)aa_line((cx-10)*16,(cy+36+i*5)*16,(cx+10)*16,(cy+36+i*5)*16,40,i==1?RGB(150,128,90):glass);
  aa_line((cx-4)*16,(cy+52)*16,(cx+4)*16,(cy+52)*16,40,glass);
  aa_line((cx-6)*16,(cy+30)*16,(cx-6)*16,(cy+4)*16,16,RGB(150,146,140));
  aa_line((cx+6)*16,(cy+30)*16,(cx+6)*16,(cy+4)*16,16,RGB(150,146,140));
  for(int i=0;i<6;i++)aa_line((cx-6+2*i)*16,(cy+4+(i&1)*5)*16,(cx-4+2*i)*16,(cy+4+((i+1)&1)*5)*16,level?28:20,warm);
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

#include "mt_instrument_views.h"
/* Battery thresholds and icon adapted from GRANA (grana.c). */
static uint32_t batt_level(void)
{
#if defined(BATT_DEMO)
    return 3u; /* screenshots */
#elif defined(GRANA_WEB) || defined(DINGHY_HOST)
    return 6u; /* Unknown: the simulator has no battery ADC. */
#else
    int32_t b = batt_raw;
    if (batt_usb()) return 5u;
    if (!b) return 6u;
    return b >= 591 ? 4u : b >= 561 ? 3u : b >= 531 ? 2u : 1u;
#endif
}
static uint32_t batt_shown(void) { uint32_t l = batt_level(); return l == 1u && (fm1_ms >> 9 & 1u) ? 0u : l; }   /* 0: the blink's off half */
static int ptext_w(const char *s,unsigned face){int w=0;for(;*s;s++)w+=panel_adv[face+glyph_index(*s)];return w;}
static void batt_h(int x,int y){                 /* horizontal: 18 x 9 with the nub on the right */
  uint32_t l=batt_shown();
  uint16_t c=l==2?RGB(240,200,60):l<2?RGB(255,70,60):RGB(80,210,100), rim=l==5?c:RGB(150,146,140);
  cv_rect(x,y,18,1,rim);cv_rect(x,y+8,18,1,rim);cv_rect(x,y,1,9,rim);cv_rect(x+17,y,1,9,rim);cv_rect(x+18,y+2,2,5,rim);
  if(l==5){cv_line(x+10,y+1,x+6,y+4,c);cv_line(x+6,y+4,x+11,y+4,c);cv_line(x+11,y+4,x+7,y+7,c);}
  else if(l==6){for(int k=0;k<4;k++)cv_rect(x+3+k*4,y+4,2,1,rim);}
  else for(uint32_t k=0;k<l;k++)cv_rect(x+2+4*k,y+2,3,5,c);
}
/* One row, nothing squeezed: page on the left, status on the right, engine in between. */
static void header_row(const char *pg,const char *eng,const char *slot,const char *mode,int hold){
  int right=234;
  unsigned fm=0;int wm=ptext_w(mode,fm);panel_text(-(right-wm),fm?0:2,mode,silver,240,fm);right-=wm+8;
  unsigned fs=0;int ws=ptext_w(slot,fs);panel_text(-(right-ws),fs?0:2,slot,silver,240,fs);right-=ws+8;
  if(hold){int wh=ptext_w("HOLD",0);panel_text(-(right-wh),2,"HOLD",gold,240,0);right-=wh+8;}
  unsigned fp=95,fe=95;int wp=ptext_w(pg,fp),we=ptext_w(eng,fe),room=right-6-8;
  if(wp+we>room){fp=0;wp=ptext_w(pg,fp);}
  if(wp+we>room){fe=0;we=ptext_w(eng,fe);}
  panel_text(-6,fp?0:2,pg,RGB(215,192,159),240,fp);
  int gap_l=6+wp+8, cx=(gap_l+right)/2;if(cx-we/2<gap_l)cx=gap_l+we/2;
  panel_text(cx,fe?0:2,eng,gold,we+1,fe);
}
static void draw(void) {
  if(intro && fm1_ms-intro_start<INTRO_END_MS){boot_draw(fm1_ms-intro_start);return;}intro=0;
  static const char *const modes[] = {"STOP", "REC", "DUB", "PLAY"};
  unsigned t = transport;
  cv_begin(240, 146, 0);
  if(page==FEEL)feel_view();
  else if(page==LFO2)lfo2_view();
  else if(page==MODFX)modfx_view();
  else if(page==RING)ring_view();
  else if(page==MATRIX)matrix_view();
  else if(page==MLFO)mlfo_view();
  else if(page==ROUTE)route_view();
  else if(page==UNCERT){
    lettering(120,7,page==UNCERT?"SOURCE OF UNCERTAINTY":"WI-FI MODULATION",gold,0);
    lettering(120,38,page==ROUTE?rf_target():air_started<0?"NO RF CALIBRATION":!knob[UNCERT][0]?"OFF":air_pick<0?"SCANNING":air_stations[air_pick].name,silver,0);
    int value=page==ROUTE?rf_amount:rf_source;
    cv_rect(18,78,204,2,metal);cv_rect(119,72,2,13,silver);cv_rect(118+value,71,4,16,gold);
#if defined(GRANA_WEB) || defined(DINGHY_HOST)
    lettering(120,108,"SIMULATED RF",silver,0);
#else
    lettering(120,108,"PASSIVE RECEIVER",silver,0);
#endif
  }else if(voice_kind()==AIR && (page==HOME || page==TIMBRE || page==ARP))air_display();
  else if(page==SCALE)fretboard();
  else if(page==MOTION)motion_view();
  else if(page==TIMBRE || page==NOTE_FX || page==HEAD2)instrument_diagram();
  else if(page==ARP && knob[SOUND][2])pattern_view();
  else if(page==SPEED)speed_dial();
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
    else if(page==LIGHTS)bulb_icon(120,50,knob[LIGHTS][0]);
    else if(page==MIDI)midi_icon(120,62);
    else {bluetooth_mark();midi_icon(152,62);}
#if !defined(GRANA_WEB) && !defined(DINGHY_HOST)
    if(page==MIDI){char c[24]="CPU ";number(c+4,(int)(audio_load_peak_q8*100u/256u));
      unsigned k=0;while(c[k])k++;c[k++]='%';c[k++]=' ';c[k++]=' ';c[k]=0;
      if(governor_hits){c[k++]='-';number(c+k,(int)governor_hits);}
      lettering(120,104,c,audio_load_peak_q8>230u?gold:silver,0);audio_load_peak_q8=audio_load_peak_q8*63u/64u;}
#endif
    const char *status=page==MIDI?(beat_ms && knob[MIDI][2]?"CLOCK LOCKED":"MIDI INPUT"):
                       page==CHARACTER?"TAPE CHARACTER":page==LIGHTS?(latch_armed?"HOLD ON":"KEYS AND HOLD"):"SYSTEM MIDI PORTS";
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
    lettering(120,118,vib_on?"SEL / VIBRATO ON":"SEL / VIBRATO OFF",silver,0);
  } else {
  static int gap;
  int target = bound(((int)length-4*(int)TAPE_RATE)*14/(4*(int)TAPE_RATE),0,14);
  gap += bound(target-gap,-2,2);
  int start[2] = {17-gap,121+gap};
  unsigned clock = turnclock;
  if(knob[SOUND][2]==3) modular_deck(clock);
  else if(knob[SOUND][2]==1) eno_diagram(gap,clock);
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
          int uq=cx*128+dx*cs+dy*sn,vq=cy*128+dy*cs-dx*sn;
          int u=uq>>7,v=vq>>7,fx=(uq>>3)&15,fy=(vq>>3)&15;
          unsigned r=0,g=0,b=0;
          for(int j=0;j<2;j++)for(int k=0;k<2;k++){
            unsigned c=swap16(deck[(v+j)*101+u+k]);
            unsigned weight=(k?fx:16-fx)*(j?fy:16-fy);
            r+=(c>>11)*weight;g+=((c>>5)&63)*weight;b+=(c&31)*weight;
          }
          cv_px[(y+5)*240+start[i]+x]=swap16((r>>8)<<11 | (g>>8)<<5 | (b>>8));
          continue;
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
  if(!naming){cv_rect(211,1,28,13,ink);batt_h(214,3);}   /* top right of the screen, on a dark tab */
  cv_blit(0, 0);
  cv_begin(240, 94, ink);
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
  header_row(page==ART && !knob[SOUND][2]?"SEL TOUCH":page==ARP && !knob[SOUND][2]?"ARP RATE":pages[page],engines[knob[SOUND][2]],slot_id,(reverse && t==HOLD)?"REV":modes[t],latch_armed);
  int start=6,end=234;
  int width=length?position*(unsigned)(end-start)/length:0;
  cv_rect(start,19,end-start,2,metal);
  cv_rect(start,19,width,2,gold);
  for (int k = 0; k < 4; k++) {
    int x = 30 + 60 * k;                          /* every page has four knobs */
    int v = knob[page][k], lo = minimum[page][k], hi = page==ARP && !k && knob[SOUND][2]?4:page==SOUND && !k && knob[SOUND][2]? (knob[SOUND][2]==1?ENO_VOICES-1:(knob[SOUND][2]==4?0:3)):maximum[page][k];
    if(page==SOUND && !k && knob[SOUND][2]==1)v=eno_menu_pos(v);
    if(page==SPEED && !k && !knob[SPEED][1])hi=3;
    vintage_knob(x,41,v-lo,hi-lo);
    char val[8];
    panel_text(x,60,value_text(k,val),RGB(235,231,220),56,95);
    static const char *const piano_fx[]={"Dir","Tone","Damp","Trem"};
    static const char *const manual[]={"Gliss","MS","Pick %","Curve"};
    const char *label=page==ART && !knob[SOUND][2]?manual[k]:page==MOTION && k==0 && voice_kind()<=BRIGHT?"BODY":page==TIMBRE?timbre_labels[voice_kind()-PIANO][k]:page==NOTE_FX?piano_fx[k]:names[page][k];
    panel_text(x,76,label,RGB(215,192,159),56,0);
  }
  cv_blit(0, 146);
}
static int key_notes[27], key_channels[27];
static void keyboard_out(unsigned status,unsigned note,unsigned velocity) {
  midi_send(status,note,velocity);
#if !defined(GRANA_WEB) && !defined(DINGHY_HOST)
  if(knob[BT][2])bt_send(status,note,velocity);   /* BLE MIDI Keys Out */
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
  int manual_glide = (knob[ARP][0]!=0) ? 0 : knob[ART][0];
  if (panic || last_glide != manual_glide) {
    last_glide = manual_glide;
    held_size = 0;
    for (int j = 0; j < 128; j++) held_counts[j] = 0;
    for (int j = 0; j < 4; j++)
      strings[j].gate = 0;
    if (panic) {pedal=0;reader=writer;phrase_size=0;for(int i=0;i<128;i++)phrase_count[i]=0;}
    panic = 0;
  }
  int picking=knob[SOUND][2]!=4 && knob[ARP][0]!=0;
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
/* Global settings: 0 key light, 1-4 TAPE CHARACTER, 5-6 SPEED; Lab 8: 7 Hold Key, 8 key Velocity,
   9 Hold time, 10 BLE Keys Out, 11 BLE Thru, 12 ERASE Depth, 13 ERASE Soft, 14 SPEED Glide, 15 SPEED Fine (+50). */
static unsigned char globals[16]={2,32,0,0,0,2,0,0,110,10,1,0,100,0,0,50};
static const unsigned char global_page[16][2]={{LIGHTS,0},{CHARACTER,0},{CHARACTER,1},{CHARACTER,2},{CHARACTER,3},{SPEED,0},{SPEED,1},
  {LIGHTS,1},{LIGHTS,2},{LIGHTS,3},{BT,2},{BT,3},{ERASE,2},{ERASE,3},{SPEED,2},{SPEED,3}};
static int globals_changed;
static unsigned light_changed;
static uint16_t midi_lit[128];

static void update_leds(unsigned keys) {
  static const unsigned char page_button[PAGE_COUNT] = {
    BTN_HOME, BTN_EDIT, BTN_SEL, BTN_FX, BTN_SEL, BTN_FX, BTN_FX, BTN_FX,
    BTN_ENV, BTN_FX, BTN_LFO, BTN_GLO, BTN_GLO, BTN_ARP, BTN_FX, BTN_GLO, BTN_GLO, BTN_FX, BTN_FX, BTN_EDIT, BTN_ARP, BTN_GLO, BTN_GLO, BTN_EDIT, BTN_EDIT, BTN_EDIT, BTN_LFO, BTN_LFO, BTN_ARP, BTN_LFO, BTN_FX, BTN_EDIT, BTN_LFO, BTN_LFO
  };
  static unsigned previous_keys, previous_buttons;
  unsigned buttons = 1u << page_button[page];
  unsigned t = transport;
  if (vib_on) buttons |= 1u << BTN_LFO;
  if (t==HOLD && reverse) buttons |= 1u << BTN_SEQ;
  if (knob[ARP][0]!=0) buttons |= 1u << BTN_ARP;
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
  eno_open();
  for(unsigned i=0;i<1024;i++)scope_ring[i]=0;
  unsigned char stored[16];
  if(!settings_load(stored,16)){                  /* Lab 8 */
    for(int i=0;i<16;i++){int pg=global_page[i][0],k=global_page[i][1],v=i==15?stored[i]-50:stored[i];
      if(v>=minimum[pg][k] && v<=maximum[pg][k])globals[i]=stored[i];}
    if(globals[6]>1 || globals[5]>(globals[6]?96:3)){globals[5]=2;globals[6]=0;}
    for(int i=7;i<16;i++)knob[global_page[i][0]][global_page[i][1]]=i==15?globals[i]-50:globals[i];
    knob[SPEED][0]=globals[5];knob[SPEED][1]=globals[6];
  }
  else if(!settings_load(stored,7) && stored[6]<=1 && stored[5]<=(stored[6]?96:3)){
    globals[5]=knob[SPEED][0]=stored[5];globals[6]=knob[SPEED][1]=stored[6];
  }
  if(!settings_load(stored,16)) {}
  else if((!settings_load(stored,7) || !settings_load(stored,5)) && stored[0]<=2) {
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
  if(page==SOUND && k==2) { switch_engine(bound(v,0,4)); return; }
  if(page==MATRIX){                               /* K1 picks a slot; K2-K4 edit that slot */
    v=bound(v,minimum[MATRIX][k],maximum[MATRIX][k]);knob[MATRIX][k]=v;
    MatrixSlot *m=&mtx.slot[knob[MATRIX][0]];
    if(k==0){knob[MATRIX][1]=m->src;knob[MATRIX][2]=m->dst;knob[MATRIX][3]=m->amount;}
    else if(k==1)m->src=(signed char)v;else if(k==2)m->dst=(signed char)v;else m->amount=(signed char)v;
    return;
  }
  if(page==BT && k==0) {
#if !defined(GRANA_WEB) && !defined(DINGHY_HOST)
    v=radio_power(v!=0);
#else
    v=0;
#endif
  }
  if((page==LIGHTS && k) || (page==BT && k>=2) || (page==ERASE && k>=2) || (page==SPEED && k>=2)){
    int nv=bound(v,minimum[page][k],maximum[page][k]);knob[page][k]=nv;
    for(int i=7;i<16;i++)if(global_page[i][0]==page && global_page[i][1]==k)globals[i]=(unsigned char)(i==15?nv+50:nv);
    globals_changed=1;light_changed=fm1_ms;return;
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
  
  int old_kind=voice_kind();
  knob[page][k]=bound(v,minimum[page][k],page==ARP && !k && knob[SOUND][2]?4:page==SOUND && !k && knob[SOUND][2]? (knob[SOUND][2]==1?11:(knob[SOUND][2]==4?0:3)):maximum[page][k]);
  if(page==SOUND && !k && voice_kind()!=old_kind){  /* a different voice: its extra TIMBRE knobs start from their defaults */
    if(new_timbre_kind(voice_kind()))knob[TIMBRE][2]=knob[TIMBRE][3]=0;
    else if(new_timbre_kind(old_kind))knob[TIMBRE][2]=knob[TIMBRE][3]=4;
  }
  if(knob[SOUND][2] && page==LFO && k<2) knob[ART][3-k]=knob[LFO][k];
  if(knob[SOUND][2] && page==ART && k>=2) knob[LFO][3-k]=knob[ART][k];

}
static unsigned last_tick,beat_start,ticks;
static void midi_input(unsigned src,unsigned st,unsigned a,unsigned b) {
  unsigned ch=st&15, cmd=st&240;
  if(st<248)midi_seen=fm1_ms;
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
  if(a==120||a==123){pedal=0;for(int i=0;i<128;i++){unsigned char *n=&midi_notes[src][ch][i];if(*n){midi_lit[*n-1]--;post(EV_OFF,*n-1,0);*n=0;}}return;}
  if(!knob[MIDI][1])return;
  int v=(b*100+63)/127;
  switch(a){
    case 64:pedal=b>=64;break;
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
/* Keyboard HOLD. A 1 s press on HOME arms it: the keys down at that moment and
   every note played afterwards keep sounding until HOME is held again.
   Notes are latched by pitch, so an octave change cannot strand one. */
enum { LATCH_MAX = 32 };
static uint8_t latch_note[LATCH_MAX], latch_channel[LATCH_MAX];
static unsigned latch_count, key_latched;
static int latch_find(int note,int channel){
  for(unsigned i=0;i<latch_count;i++)if(latch_note[i]==note && latch_channel[i]==channel)return (int)i;
  return -1;
}
static void latch_off(unsigned i){
  keyboard_out(128+latch_channel[i],latch_note[i],0);
  post(EV_OFF,latch_note[i],0);
}
static void latch_add(int note,int channel){
  if(latch_find(note,channel)>=0)return;
  if(latch_count==LATCH_MAX){           /* full: the oldest held note makes room */
    latch_off(0);
    for(unsigned i=1;i<LATCH_MAX;i++){latch_note[i-1]=latch_note[i];latch_channel[i-1]=latch_channel[i];}
    latch_count--;
  }
  latch_note[latch_count]=(uint8_t)note;latch_channel[latch_count]=(uint8_t)channel;latch_count++;
}
static void release_hold(unsigned keys){
  for(unsigned i=0;i<latch_count;i++){
    int still=0;                        /* a key still down keeps its own note */
    for(unsigned k=0;k<27;k++)
      if((keys&(1u<<k)) && key_notes[k]==latch_note[i] && key_channels[k]==latch_channel[i])still=1;
    if(!still)latch_off(i);
  }
  latch_count=0;key_latched=0;latch_armed=0;latch_keys=0;
}
static void arm_hold(unsigned keys){
  latch_armed=1;latch_count=0;key_latched=0;
  for(unsigned k=0;k<27;k++)if(keys&(1u<<k)){latch_add(key_notes[k],key_channels[k]);key_latched|=1u<<k;}
}
static unsigned latched_keys(void){       /* keys whose current pitch is held, for the LEDs */
  unsigned bits=0;
  if(!latch_count)return 0;
  for(int k=0;k<27;k++){
    int note=scale_note(bound(53+k+octave*12,0,127));
    if(latch_find(note,knob[MIDI][0]?knob[MIDI][0]-1:0)>=0)bits|=1u<<k;
  }
  return bits;
}
static void app_step(void) {
#if !defined(GRANA_WEB) && !defined(DINGHY_HOST)
  fm1_app_ctx=(unsigned)page|(unsigned)knob[SOUND][2]<<8|(unsigned)knob[SOUND][0]<<12|(unsigned)(knob[UNCERT][0]!=0)<<20|(unsigned)transport<<24|(unsigned)(eno_version&15)<<28;
#endif
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
  static unsigned home_started;
  static int home_pending;
  if(edge&(1u<<BTN_HOME)){home_started=fm1_ms;home_pending=1;}
  if(home_pending && fm1_ms-home_started>=(unsigned)knob[LIGHTS][3]*100u){   /* KEYS Hold s */
    home_pending=0;
    if(latch_armed)release_hold(key_prev);
    else arm_hold(key_prev);
  }
  if(rel&(1u<<BTN_HOME)){if(home_pending)page=HOME;home_pending=0;}
  if (edge & (1u << BTN_LFO)) { int rad=knob[SOUND][2]==3;   /* RADIGUE: the matrix follows LFO 2 */
    page=page==LFO?LFO2:page==LFO2?(rad?MATRIX:UNCERT):page==MATRIX?MLFO:page==MLFO?UNCERT:page==UNCERT?ROUTE:LFO; }
  if (edge & (1u << BTN_EDIT))
    page = knob[SOUND][2]==4 ? (page==TIMBRE ? RING : TIMBRE) : knob[SOUND][2] && page==SOUND ? TIMBRE :
           knob[SOUND][2] && page==TIMBRE ? MOTION :
           knob[SOUND][2]==1 && page==MOTION ? NOTE_FX :
           knob[SOUND][2]==1 && page==NOTE_FX ? HEAD2 : SOUND;
  if (edge & (1u << BTN_SEL)){if(page==LFO)vib_on=!vib_on;else page=page==ART?SCALE:ART;}
  if (edge & (1u << BTN_GLO)) page = page==CHARACTER ? SPEED : page==SPEED ? ERASE : page==ERASE ? MIDI : page==MIDI ? BT : page==BT ? LIGHTS : CHARACTER;
  if(edge & (1u<<BTN_ARP)) {
    if(voice_kind()==AIR){air_freeze();page=TIMBRE;}
    else if(knob[SOUND][2]==0 && page==ARP){page=PICK_ART;}
    else if(knob[SOUND][2]==0 && page==PICK_ART){page=ARP;}
    else if(knob[SOUND][2])page=page==ARP?FEEL:ARP;      /* ARP, then ARP FEEL */
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
    page = page==COMP ? FUZZ : page==FUZZ ? WAH : page==WAH ? AMP : page==AMP ? EQ : page==EQ ? FILTER : page==FILTER ? MODFX : page==MODFX ? OFF_TAPE : page==OFF_TAPE ? PLATE : COMP;
  if (edge & (1u << BTN_ENV)) page = ENV;
  int d = fm1_enc_take(ENC_SELECT);
  if(d)intro=0;
  if (d) naming=0;
  if (d)
    {
      int last=knob[SOUND][2]>=2 ? HEAD2 : HEAD2+1;
      int count=last+2;
      int index=page==TIMBRE || page==MOTION || page==NOTE_FX?SOUND:page==SPEED?last:page==ERASE?last+1:page==PICK_ART?HEAD2:page;
      index=((index+d)%count+count)%count;
      page=index==last+1?ERASE:index==last?SPEED:index==HEAD2 && knob[SOUND][2]==0 ? PICK_ART : index;
    }
  d = fm1_enc_take(ENC_ALGORITHM);
  if(d)intro=0;
  if (d && !naming) switch_engine(bound(knob[SOUND][2]+d,0,4));
  d = fm1_enc_take(ENC_PRESETS);
  if(d)intro=0;
  if (d)
    { if(naming)start_name(bound(destination+d,0,PROGRAM_COUNT-1));
      else recall_program(bound(program+d,0,PROGRAM_COUNT-1)); }
  for (int k = 0; k < 4; k++) {
    d = fm1_enc_take(ENC_KNOB1 + k);
    if(d && naming) edit_name(k,d);
    else if(d && page==SOUND && !k && knob[SOUND][2]==1)set_control(k,eno_menu[bound(eno_menu_pos(knob[SOUND][0])+d,0,ENO_VOICES-1)]);   /* ENO: K1 walks the voice menu */
    else if(d) set_control(k,knob[page][k]+d);
  }
  for (int k = 0; k < 27; k++)
    if ((keys ^ key_prev) & (1u << k)) {
      if (keys & (1u << k)) {
        key_notes[k] = scale_note(bound(53 + k + octave * 12, 0, 127));
        key_channels[k]=knob[MIDI][0]?knob[MIDI][0]-1:0;
        int velocity=knob[LIGHTS][2];
        if(latch_armed){
          key_latched|=1u<<k;
          int held=latch_find(key_notes[k],key_channels[k]);
          if(held>=0){                           /* a held note again: KEYS Hold Key decides, as in GRANA */
            keyboard_out(128+key_channels[k],key_notes[k],0);post(EV_OFF,key_notes[k],0);
            if(knob[LIGHTS][1]){                 /* RELEASE: the note ends, lifting the finger does nothing */
              for(unsigned i=held+1;i<latch_count;i++){latch_note[i-1]=latch_note[i];latch_channel[i-1]=latch_channel[i];}
              latch_count--;continue;
            }
          }else latch_add(key_notes[k],key_channels[k]);   /* RESTART (default): it starts again */
        }
        keyboard_out(144+key_channels[k],key_notes[k],velocity);
        post(EV_NOTE, key_notes[k], velocity);
      } else {
        if(key_latched&(1u<<k)){key_latched&=~(1u<<k);continue;}
        keyboard_out(128+key_channels[k],key_notes[k],0);
        post(EV_OFF, key_notes[k], 0);
      }
    }
  key_prev = keys;
  uint8_t st, a, b;
  while(midi_get(&st,&a,&b)){
#if !defined(GRANA_WEB) && !defined(DINGHY_HOST)
    if(knob[BT][3])bt_send(st,a,b);               /* BLE MIDI Thru: USB/TRS to Bluetooth */
#endif
    midi_input(0,st,a,b);
  }
#if !defined(GRANA_WEB) && !defined(DINGHY_HOST)
  radio_poll();
  while(bti_r!=bti_w){unsigned pkt=bt_in_q[bti_r++%BTQ];
    if(knob[BT][3])midi_send((pkt>>8)&255,(pkt>>16)&127,(pkt>>24)&127);   /* and Bluetooth to USB/TRS */
    midi_input(1,(pkt>>8)&255,(pkt>>16)&127,(pkt>>24)&127);}
  if(bt_lost){bt_lost=0;for(int ch=0;ch<16;ch++)for(int n=0;n<128;n++)midi_input(1,128+ch,n,0);}
#endif
  if(!knob[MIDI][2] || (last_tick && fm1_ms-last_tick>1000)){last_tick=ticks=beat_ms=0;}
  air_step(voice_kind()==AIR || knob[UNCERT][0],fm1_ms);
  latch_keys=latched_keys();
  update_leds(keys|latch_keys);
  if(globals_changed && fm1_ms-light_changed>=2000u) {   /* 1.20: after 2 s of quiet, one page program */
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
static void web_bank_changed(void) { eno_open(); }
WEB_EXPORT("eno_extended") int web_eno_extended(void){return eno_extended;}
static uint32_t web_state(void) {
  return ((naming?destination:program) << 16) | (knob[SOUND][2] << 12) | (naming << 11) | (page & 15) | (transport << 4) | ((knob[ARP][0]!=0) << 8) | (vib_on << 9) | (reverse << 10);
}
WEB_EXPORT("rf_meter") int web_rf_meter(void){return air_level;}
WEB_EXPORT("rf_count") int web_rf_count(void){return air_count;}
WEB_EXPORT("rf_frozen") int web_rf_frozen(void){return air_frozen;}
WEB_EXPORT("voice") int web_voice(void){return voice_kind();}
WEB_EXPORT("arp") int web_arp(void){return phrase_step | phrase_size<<8 | phrase_left<<16;}
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
