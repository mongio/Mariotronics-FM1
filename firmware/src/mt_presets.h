/* 64 guitar/tape programs. Two alternating flash sectors keep the previous
 * complete set available until the new payload and its checksum are committed. */
enum { PROGRAM_COUNT = 64 };
static void global_offtape(void);
static const unsigned char patch_pages[] = {
  HOME, SOUND, ART, FUZZ, SCALE, WAH, AMP, PLATE, ENV, COMP, LFO, ARP, OFF_TAPE
};
typedef struct { unsigned char values[sizeof patch_pages][4], vibrato; } Patch;
static Patch initial_patch;
static struct { unsigned char custom[PROGRAM_COUNT]; Patch patch[PROGRAM_COUNT]; } bank;
typedef struct { unsigned magic, generation, checksum; } BankHeader;
_Static_assert(sizeof bank + sizeof(BankHeader) <= 4096, "program bank exceeds flash sector");
static int bank_sector = -1, program;
static unsigned bank_generation, program_notice_time;
static const char *program_notice;
static const char *const tone_names[] = {"Singing","Hollow","Velvet","Glass","Fuzz","Reed","Copper","Wire"};
static const char *const space_names[] = {"Tape","Swell","Dusk","Echo","Bloom","Pulse","Mist","Drift"};

/* Voice, pickup drive, fuzz drive/mix, amp gain/treble, wah mix. */
static const unsigned char tones[][7] = {
  {0,55,54,42,32,42,0},{1,55,54,42,32,42,0},{0,26,18,12,17,27,0},{1,29,22,14,20,81,0},
  {0,78,86,77,58,49,0},{1,59,46,33,38,52,48},{0,64,50,37,47,65,20},{1,75,70,59,54,77,12}
};
/* Attack, feedback, tape seconds, plate mix/decay, off-tape level, glide, vibrato cents. */
static const unsigned char spaces[][8] = {
  {38,81,4,19,72,25,24,22},{62,75,6,28,82,15,38,15},{34,87,7,24,76,10,32,12},
  {12,67,4,10,49,58,8,7},{76,89,8,34,89,18,52,26},{5,69,5,8,46,36,4,5},
  {66,82,8,43,91,8,44,19},{46,86,6,30,84,32,58,33}
};

enum { NAME_LEN=16, EMPTY_SLOT=2 };
#include "mt_factory.h"
/* 1.20: P01-P08 the eight FRIPP tones on Tape, P09-P16 each tone with another space. */
static int factory_space(int slot){return slot<8?0:1+(slot%8)%7;}
static char names_saved[PROGRAM_COUNT][NAME_LEN+1], draft[NAME_LEN+1];
static int naming, destination, cursor, deleting;
static unsigned name_seed=937u;
static const char alphabet[]=" ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-'";
static void copy_name(char *dst,const char *src) {
  int i=0;
  while(i<NAME_LEN && src[i]) { dst[i]=src[i]; i++; }
  dst[i]=0;
}
static void slot_name(int slot,char *dst) {
  if(names_saved[slot][0]) {copy_name(dst,names_saved[slot]); return;}
  if(bank.custom[slot]==EMPTY_SLOT) {copy_name(dst,"Empty");return;}
  if(slot>=16){copy_name(dst,factory[slot-16].name);return;}   /* 1.20: P17-P64 use the other engines */
  const char *tone=tone_names[slot%8], *space=space_names[factory_space(slot)];
  while(*tone) *dst++=*tone++;
  *dst++=' ';
  copy_name(dst,space);
}
static void start_name(int slot) {
  naming=1; destination=slot; deleting=0; cursor=0;
  if(bank.custom[slot]==EMPTY_SLOT) copy_name(draft,"New Tape");
  else slot_name(slot,draft);
}
static void edit_name(int k,int d) {
  int n=0;
  while(draft[n]) n++;
  if(k==3 && d>0){deleting=1;return;}
  deleting=0;
  if(k==0) cursor=bound(cursor+d,0,n<NAME_LEN?n:NAME_LEN-1);
  if(k==1){
    int at=0, count=sizeof alphabet-1;
    for(int i=0;i<count;i++) if(alphabet[i]==draft[cursor]) at=i;
    draft[cursor]=alphabet[(at+d%count+count)%count];
    if(cursor==n)draft[n+1]=0;
  }
  if(k==2){
    static const char *const first[]={"Amber","Silver","Velvet","Distant","Faded","Midnight","Copper","Slow"};
    static const char *const last[]={"Spool","Head","Return","Bloom","Ferric","Echo","Swell","Reel"};
    char next[NAME_LEN+1]; unsigned prev=name_seed;
    name_seed=name_seed*1664525u+1013904223u+fm1_ms;
    unsigned a=name_seed>>29, b=(name_seed>>16)&7;
    if((prev>>29)==a && ((prev>>16)&7)==b) {name_seed^=1u<<29;a=name_seed>>29;}
    const char *x=first[a], *y=last[b]; int i=0;
    while(*x)next[i++]=*x++;
    next[i++]=' ';while(*y)next[i++]=*y++;
    next[i]=0;copy_name(draft,next);cursor=0;
  }
  if(k==3 && d<0 && cursor<n) for(int i=cursor;i<n;i++)draft[i]=draft[i+1];
}
typedef struct { unsigned char eq[4],filter[4]; } TonePatch;
static const TonePatch neutral_tone={{12,12,4,12},{0,77,0,100}};
static TonePatch tone_bank[PROGRAM_COUNT],tone_working[5];
/* Store the route in the unused upper bits of the existing EQ/filter bytes.
   Legacy records have zero in those bits and recall with Wi-Fi off. */
static const unsigned char tone_bits[]={5,5,4,5,2,7,7,7};
static void capture_tone(TonePatch *t) {
  int amount=knob[ROUTE][1];
  unsigned packed=(absolute(amount)*2-(amount<0));
  packed=(packed<<2)|knob[ROUTE][0];
  packed=(packed<<4)|knob[UNCERT][3];
  packed=(packed<<4)|knob[UNCERT][2];
  packed=(packed<<2)|knob[UNCERT][1];
  packed=(packed<<1)|knob[UNCERT][0];
  packed|=(unsigned)(knob[FILTER][0]>2)<<21;      /* 1.20: EQ and FILTER after the tape */
  for(int i=0;i<8;i++){
    unsigned n=tone_bits[i];
    ((unsigned char *)t)[i]=(i==4?knob[FILTER][0]%3:knob[i<4?EQ:FILTER][i%4])|(packed<<n);
    packed>>=8-n;
  }
}
static void apply_tone(const TonePatch *t) {
  unsigned packed=0;
  for(int i=7;i>=0;i--){
    int pg=i<4?EQ:FILTER,k=i%4;
    unsigned n=tone_bits[i],v=((const unsigned char *)t)[i];
    knob[pg][k]=bound(v&((1u<<n)-1),0,maximum[pg][k]);
    packed=(packed<<(8-n))|(v>>n);
  }
  knob[UNCERT][0]=packed&1;knob[UNCERT][1]=bound((packed>>1)&3,0,2);
  knob[UNCERT][2]=bound((packed>>3)&15,0,8);knob[UNCERT][3]=bound((packed>>7)&15,0,8);
  knob[ROUTE][0]=(packed>>11)&3;
  knob[FILTER][0]=bound(knob[FILTER][0],0,2)+3*((packed>>21)&1);
  int z=bound((packed>>13)&255,0,200);knob[ROUTE][1]=(z&1)?-(z+1)/2:z/2;
  rf_amount=rf_amount2=rf_source=0;
}
/* Lab 8 controls stored in ExtraPatch.more (zero = default). */
/* more[]: each control stored as its distance from the default, so zeros recall the defaults. */
static void capture_more(signed char *m){
  int air=voice_kind()==AIR;
  m[0]=knob[SCALE][2];m[1]=knob[SCALE][3];
  m[2]=100-knob[OFF_TAPE][1];m[3]=knob[OFF_TAPE][2]-50;m[4]=180-knob[OFF_TAPE][3];
  m[5]=100-knob[HEAD2][1];m[6]=knob[HEAD2][2]-73;m[7]=knob[HEAD2][3];
  if(air){m[8]=knob[RING][0];m[9]=knob[RING][1]-2;m[10]=knob[RING][2];m[11]=knob[RING][3];}
  else{m[8]=new_timbre_kind(voice_kind())?knob[TIMBRE][2]:0;m[9]=new_timbre_kind(voice_kind())?knob[TIMBRE][3]:0;m[10]=knob[MOTION][2];m[11]=knob[MOTION][3];}
  m[12]=knob[LFO2][0]-12;m[13]=knob[LFO2][1];m[14]=knob[LFO2][2]-4;m[15]=knob[LFO2][3];
  m[16]=knob[MODFX][0];m[17]=knob[MODFX][1]-30;m[18]=knob[MODFX][2]-50;m[19]=knob[MODFX][3]-50;
  m[20]=m[21]=m[22]=0;
}
static void apply_more(const signed char *m){
  static const signed char zero[23];
  if(!m)m=zero;
  int kind=voice_kind();
  knob[SCALE][2]=bound(m[0],-12,12);knob[SCALE][3]=bound(m[1],-50,50);
  knob[OFF_TAPE][1]=bound(100-m[2],0,100);knob[OFF_TAPE][2]=bound(50+m[3],0,100);knob[OFF_TAPE][3]=bound(180-m[4],20,180);
  knob[HEAD2][1]=bound(100-m[5],0,100);knob[HEAD2][2]=bound(73+m[6],50,95);knob[HEAD2][3]=bound(m[7],0,1);
  if(kind==AIR){knob[RING][0]=bound(m[8],0,8);knob[RING][1]=bound(2+m[9],0,8);knob[RING][2]=bound(m[10],0,2);knob[RING][3]=bound(m[11],0,8);knob[MOTION][2]=knob[MOTION][3]=0;}
  else{
    if(new_timbre_kind(kind)){knob[TIMBRE][2]=bound(m[8],0,8);knob[TIMBRE][3]=bound(m[9],0,8);}
    knob[MOTION][2]=bound(m[10],0,3);knob[MOTION][3]=bound(m[11],0,8);
    knob[RING][0]=0;knob[RING][1]=2;knob[RING][2]=knob[RING][3]=0;
  }
  knob[LFO2][0]=bound(12+m[12],0,40);knob[LFO2][1]=bound(m[13],0,6);knob[LFO2][2]=bound(4+m[14],0,10);knob[LFO2][3]=bound(m[15],-100,100);
  knob[MODFX][0]=bound(m[16],0,8);knob[MODFX][1]=bound(30+m[17],0,100);knob[MODFX][2]=bound(50+m[18],0,100);knob[MODFX][3]=bound(50+m[19],0,100);
}
/* Lab 7 settings beside each program: ARP FEEL and the full Wi-Fi routing.
   A record without the tag (older banks, older backups padded with zeros) recalls the defaults. */
typedef struct { unsigned char tag, swing, accent, human, repeats, target_a, target_b; signed char amount_b; unsigned char arp_slow; signed char more[23]; } ExtraPatch;
enum { EXTRA_TAG=0xA7 };
static ExtraPatch extra_bank[PROGRAM_COUNT] POOLMEM, extra_working[5] POOLMEM;
static const ExtraPatch no_extra;
/* 1.20: the RADIGUE MATRIX rides in the unused high bits of the Lab 7/8 record (50 bits), so
   banks and backups keep their size. All zero bits recall an empty matrix with default LFOs. */
static const unsigned char mtx_bits_at[10][3]={{1,6,2},{2,2,6},{3,7,1},{4,3,5},{5,4,4},{6,4,4},{8,4,4},{29,0,8},{30,0,8},{31,0,8}};
static void mtx_pack(ExtraPatch *e){
  unsigned long long v=0;int n=0;
  for(int i=0;i<4;i++){const MatrixSlot *m=&mtx.slot[i];
    v|=(unsigned long long)(m->src&3)<<n;n+=2;v|=(unsigned long long)(m->dst&15)<<n;n+=4;v|=(unsigned long long)(m->amount&15)<<n;n+=4;}
  v|=(unsigned long long)((knob[MLFO][0]^3)&7)<<n;n+=3;v|=(unsigned long long)((knob[MLFO][1]^2)&7)<<n;n+=3;
  v|=(unsigned long long)((knob[MLFO][2]^2)&7)<<n;n+=3;v|=(unsigned long long)(knob[MLFO][3]&1)<<n;
  unsigned char *b=(unsigned char *)e;
  for(int f=0;f<10;f++){unsigned at=mtx_bits_at[f][0],lo=mtx_bits_at[f][1],w=mtx_bits_at[f][2],mask=((1u<<w)-1u)<<lo;
    b[at]=(unsigned char)((b[at]&~mask)|((unsigned)(v&((1u<<w)-1u))<<lo));v>>=w;}
}
static void mtx_unpack(const ExtraPatch *e){
  unsigned long long v=0;int n=0;const unsigned char *b=(const unsigned char *)e;
  if(e && e->tag==EXTRA_TAG)for(int f=0;f<10;f++){unsigned at=mtx_bits_at[f][0],lo=mtx_bits_at[f][1],w=mtx_bits_at[f][2];
    v|=(unsigned long long)((b[at]>>lo)&((1u<<w)-1u))<<n;n+=w;}
  for(int i=0;i<4;i++){MatrixSlot *m=&mtx.slot[i];int a;
    m->src=(signed char)(v&3);v>>=2;m->dst=(signed char)bound((int)(v&15),0,MT_COUNT-1);v>>=4;a=(int)(v&15);v>>=4;m->amount=(signed char)bound(a>7?a-16:a,-7,7);}
  knob[MLFO][0]=(int)((v&7)^3);v>>=3;knob[MLFO][1]=(int)((v&7)^2);v>>=3;knob[MLFO][2]=(int)((v&7)^2);v>>=3;knob[MLFO][3]=(int)(v&1);
  { const MatrixSlot *m=&mtx.slot[knob[MATRIX][0]&3];knob[MATRIX][1]=m->src;knob[MATRIX][2]=m->dst;knob[MATRIX][3]=m->amount; }
}
static void capture_extra(ExtraPatch *e) {
  e->tag=EXTRA_TAG;e->swing=knob[FEEL][0];e->accent=knob[FEEL][1];e->human=knob[FEEL][2];e->repeats=knob[FEEL][3];
  e->target_a=knob[ROUTE][0];e->target_b=knob[ROUTE][2];e->amount_b=(signed char)knob[ROUTE][3];
  e->arp_slow=knob[ARP][1]<3?(unsigned char)(3-knob[ARP][1]):0;   /* the patch keeps 3 Hz */
  capture_more(e->more);
  mtx_pack(e);
}
static void apply_extra(const ExtraPatch *src) {
  ExtraPatch clean=*src,*e=&clean;
  mtx_unpack(src);
  clean.swing&=63;clean.accent&=3;clean.human&=127;clean.repeats&=7;clean.target_a&=15;clean.target_b&=15;clean.arp_slow&=15;
  clean.more[20]=clean.more[21]=clean.more[22]=0;
  if(e->tag!=EXTRA_TAG){knob[FEEL][0]=0;knob[FEEL][1]=1;knob[FEEL][2]=0;knob[FEEL][3]=1;knob[ROUTE][2]=4;knob[ROUTE][3]=0;apply_more(0);return;}
  knob[FEEL][0]=bound(e->swing,0,50);knob[FEEL][1]=bound(e->accent,0,3);knob[FEEL][2]=bound(e->human,0,100);knob[FEEL][3]=bound(e->repeats,1,4);
  knob[ROUTE][0]=bound(e->target_a,0,10);knob[ROUTE][2]=bound(e->target_b,0,10);knob[ROUTE][3]=bound(e->amount_b,-100,100);
  if(e->arp_slow && knob[ARP][1]==3)knob[ARP][1]=3-bound(e->arp_slow,1,12);
  apply_more(e->more);
}
static void capture_patch(Patch *p) {
  for (unsigned i=0;i<sizeof patch_pages;i++)
    for(int k=0;k<4;k++) p->values[i][k]=knob[patch_pages[i]][k];
  /* The high bit distinguishes tenths-of-a-second patches from legacy SWELL patches. */
  p->values[12][1]=knob[HEAD2][0]; /* spare OFF_TAPE byte in existing patches */
  if(knob[ARP][1]<3)p->values[11][1]=3;   /* slow rates live in the Lab record */
  /* Unused SCALE and OFF_TAPE controls keep the stored patch size compatible. */
  for(int k=0;k<4;k++)p->values[k<2?4:12][2+(k%2)]=knob[PICK_ART][k];
  p->vibrato=224|vib_on;
  if(knob[SOUND][2]){
    p->vibrato|=16|(knob[MOTION][1]<<1);
    p->values[4][2]=knob[MOTION][0]*81+knob[TIMBRE][0]*9+knob[TIMBRE][1];
    p->values[4][3]=knob[ARP][0]*16+(knob[ARP][1]<3?3:knob[ARP][1])-3;
    p->values[12][2]=knob[ARP][2];p->values[12][3]=knob[ARP][3];
    if(voice_kind()==AIR){p->values[1][0]=0;p->values[1][2]=3;p->values[12][1]=128+knob[TIMBRE][2]*9+knob[TIMBRE][3];}
    if(knob[SOUND][2]==1)p->values[2][2]=128+knob[TIMBRE][2]*9+knob[TIMBRE][3];
    if(knob[SOUND][2]==1)for(int k=0;k<4;k++)p->values[11][k]=knob[NOTE_FX][k];
  }
}
static Patch working[5];
static int working_valid[5];

static void apply_patch(const Patch *p) {
  for (unsigned i=0;i<sizeof patch_pages;i++)
    for(int k=0;k<4;k++) knob[patch_pages[i]][k]=p->values[i][k];
  knob[HEAD2][0]=p->values[12][1]==1;
  static const unsigned char defaults[]={12,36,1,7};
  for(int k=0;k<4;k++)
    knob[PICK_ART][k]=(p->vibrato&64)?bound(p->values[k<2?4:12][2+k%2],0,maximum[PICK_ART][k]):defaults[k];
  if(!(p->vibrato&32) && !knob[SOUND][2]){knob[ART][2]=40;knob[ART][3]=9;}
  if(knob[SOUND][2]){
    knob[TIMBRE][0]=knob[TIMBRE][1]=knob[TIMBRE][2]=knob[TIMBRE][3]=4;knob[MOTION][0]=1;knob[MOTION][1]=0;
    if(knob[SOUND][2]==1){
      if(p->values[2][2]>=128 && p->values[2][2]<=208){int extra=p->values[2][2]-128;knob[TIMBRE][2]=extra/9;knob[TIMBRE][3]=extra%9;knob[ART][2]=knob[LFO][1];}
      for(int k=0;k<4;k++)knob[NOTE_FX][k]=knob[ARP][k];
      knob[ARP][0]=0;knob[ARP][1]=6;knob[ARP][2]=64;knob[ARP][3]=15;
    }
    if(knob[SOUND][2]==3 && !knob[SOUND][0] && p->values[12][1]>=128 && p->values[12][1]<=208){knob[SOUND][2]=4;int n=p->values[12][1]-128;knob[TIMBRE][2]=n/9;knob[TIMBRE][3]=n%9;}
    if(p->vibrato&16){
      knob[TIMBRE][0]=p->values[4][2]%81/9;knob[TIMBRE][1]=p->values[4][2]%9;
      knob[MOTION][0]=bound(p->values[4][2]/81,0,2);knob[MOTION][1]=(p->vibrato>>1)&7;
      knob[ARP][0]=bound(p->values[4][3]/16,0,4);knob[ARP][1]=bound(3+p->values[4][3]%16,3,16);
      knob[ARP][2]=bound(p->values[12][2],20,95);knob[ARP][3]=bound(p->values[12][3],0,100);
    }
  }
  vib_on=p->vibrato & 1;
  if (!(p->vibrato & 128)) {
    knob[HOME][0]=bound((knob[SOUND][2]+1)*10,40,80);
    knob[SOUND][2]=0;
  }
}

static void switch_engine(int next) {
  int current=knob[SOUND][2];
  if (current==next) return;
  if(latch_armed)release_hold(fm1_in.notes);
#if !defined(GRANA_WEB) && !defined(DINGHY_HOST)
  unsigned flags=irq_save();
#endif
  /* 1.20: what follows the tape stays as it is when the engine changes: plate, tape delay, feedback and
     return, the second head, and the ensemble, EQ and filter when they are set to POST. */
  int keep_plate[4],keep_home[4],keep_head2[4],keep_fx[4],keep_eq[4],keep_filter[4];
  int fx_post_was=knob[MODFX][0]>4,tone_post_was=knob[FILTER][0]>2;
  for(int i=0;i<4;i++){keep_plate[i]=knob[PLATE][i];keep_home[i]=knob[HOME][i];keep_head2[i]=knob[HEAD2][i];
    keep_fx[i]=knob[MODFX][i];keep_eq[i]=knob[EQ][i];keep_filter[i]=knob[FILTER][i];}
  capture_patch(&working[current]);capture_tone(&tone_working[current]);capture_extra(&extra_working[current]);
  working_valid[current]=1;
  if (working_valid[next]) {apply_patch(&working[next]);apply_tone(&tone_working[next]);apply_extra(&extra_working[next]);} else {apply_tone(&neutral_tone);apply_extra(&no_extra);}
  if(!working_valid[next] && !next) {knob[SOUND][0]=0; knob[ART][2]=40; knob[ART][3]=9;}
  if(!working_valid[next] && next) {
    knob[ART][2]=knob[LFO][1];knob[ART][3]=knob[LFO][0];
    knob[TIMBRE][0]=knob[TIMBRE][1]=knob[TIMBRE][2]=knob[TIMBRE][3]=4;knob[MOTION][0]=1;knob[MOTION][1]=0;
    knob[NOTE_FX][0]=0;knob[NOTE_FX][1]=10;knob[NOTE_FX][2]=48;knob[NOTE_FX][3]=0;
    knob[ARP][0]=0;knob[ARP][1]=10;knob[ARP][2]=48;knob[ARP][3]=0;
    knob[ENV][0]=0;knob[ENV][1]=44;knob[ENV][2]=85;knob[ENV][3]=35;
    knob[HEAD2][0]=0;knob[SOUND][0]=0;knob[ART][0]=0;
    knob[FUZZ][3]=0;knob[AMP][0]=0;knob[WAH][3]=0;vib_on=0;
  }
  if(!working_valid[next] && next==2) {
    knob[ENV][0]=12; knob[ENV][2]=100;
    knob[OFF_TAPE][0]=0; knob[ARP][1]=9; knob[ARP][2]=72;
  }
  if(!working_valid[next] && next==3){knob[ENV][0]=88;knob[ENV][2]=100;knob[ENV][3]=85;}
  if(!working_valid[next] && next==4){knob[TIMBRE][0]=0;knob[ENV][0]=24;knob[ENV][2]=100;knob[ENV][3]=55;}
  knob[SOUND][2]=next;
  if(!working_valid[next] && new_timbre_kind(voice_kind()))knob[TIMBRE][2]=knob[TIMBRE][3]=0;   /* Lab 8 extras start off */
  { int fx_post_now=knob[MODFX][0]>4,tone_post_now=knob[FILTER][0]>2;
    for(int i=0;i<4;i++){knob[PLATE][i]=keep_plate[i];knob[HEAD2][i]=keep_head2[i];if(i!=2)knob[HOME][i]=keep_home[i];
      if(fx_post_was || fx_post_now)knob[MODFX][i]=keep_fx[i];
      if(tone_post_was || tone_post_now){knob[EQ][i]=keep_eq[i];knob[FILTER][i]=keep_filter[i];}} }
  global_offtape();
  if(page==RING && next!=4)page=TIMBRE;
  if((page==MATRIX || page==MLFO) && next!=3)page=LFO2;
  if(page==PICK_ART && next!=0)page=ARP;
  if(page==FEEL && (next==0 || next==4))page=ARP;
  if((page==NOTE_FX || page==HEAD2) && next!=1)page=SOUND;
  if((page==TIMBRE || page==MOTION) && !next)page=SOUND;
#if !defined(GRANA_WEB) && !defined(DINGHY_HOST)
  irq_restore(flags);
#endif
}

static void apply_factory(const FactoryProgram *f){
  if(f->engine)switch_engine(f->engine);           /* the engine's own starting point (nothing is valid yet) */
  knob[SOUND][0]=f->voice;
  if(new_timbre_kind(voice_kind()))knob[TIMBRE][2]=knob[TIMBRE][3]=0;
  for(int i=0;i<f->count;i++){
    int pg=f->ops[3*i],k=f->ops[3*i+1],v=f->ops[3*i+2];
    if(pg>=200){MatrixSlot *m=&mtx.slot[(pg-200)&3];if(k==0)m->src=(signed char)v;else if(k==1)m->dst=(signed char)v;else m->amount=(signed char)v;continue;}
    if(pg<PAGE_COUNT && k<4)knob[pg][k]=bound(v,minimum[pg][k],pg==ARP && !k && knob[SOUND][2]?4:maximum[pg][k]);
  }
  if(knob[SOUND][2] && knob[LFO][0])knob[ART][3]=knob[LFO][0];
  { const MatrixSlot *m=&mtx.slot[knob[MATRIX][0]&3];knob[MATRIX][1]=m->src;knob[MATRIX][2]=m->dst;knob[MATRIX][3]=m->amount; }
}
static void recall_program(int slot) {
  if(latch_armed)release_hold(fm1_in.notes);
  naming=0;
  Patch p = bank.custom[slot]==1 ? bank.patch[slot] : initial_patch;
#if !defined(GRANA_WEB) && !defined(DINGHY_HOST)
  unsigned flags=irq_save();
#endif
  working_valid[0]=working_valid[1]=working_valid[2]=working_valid[3]=working_valid[4]=0;
  apply_patch(&p);apply_tone(bank.custom[slot]==1?&tone_bank[slot]:&neutral_tone);
  apply_extra(bank.custom[slot]==1?&extra_bank[slot]:&no_extra);
  if (!bank.custom[slot] && slot>=16) apply_factory(&factory[slot-16]);
  else if (!bank.custom[slot]) {
    const unsigned char *t=tones[slot%8], *s=spaces[factory_space(slot)];
    knob[SOUND][0]=t[0]; knob[SOUND][1]=t[1]; knob[FUZZ][0]=t[2]; knob[FUZZ][3]=t[3];
    knob[AMP][0]=t[4]; knob[AMP][2]=t[5]; knob[WAH][3]=t[6];
    knob[ENV][0]=s[0]; knob[HOME][1]=s[1]; knob[HOME][0]=10*s[2];
    knob[PLATE][0]=s[3]; knob[PLATE][1]=s[4]; knob[OFF_TAPE][0]=s[5];
    knob[ART][1]=s[6]; knob[LFO][1]=s[7];knob[ART][2]=40;knob[ART][3]=9;
  }
#if !defined(GRANA_WEB) && !defined(DINGHY_HOST)
  irq_restore(flags);
#endif
  if(page==PICK_ART && knob[SOUND][2]!=0)page=ARP;
  if((page==MATRIX || page==MLFO) && knob[SOUND][2]!=3)page=LFO2;
  if((page==NOTE_FX || page==HEAD2) && knob[SOUND][2]!=1)page=SOUND;
  if((page==TIMBRE || page==MOTION) && !knob[SOUND][2])page=SOUND;
  global_offtape();
  program=slot;
  program_notice=bank.custom[slot]==EMPTY_SLOT?"EMPTY SLOT":bank.custom[slot]?"USER PRESET":"FACTORY PRESET";
  program_notice_time=fm1_ms;
}
static unsigned bank_hash(unsigned generation,int with_names) {
  const unsigned char *bytes=(const unsigned char *)&bank;
  unsigned hash=2166136261u ^ generation;
  for(unsigned i=0;i<sizeof bank;i++) hash=(hash^bytes[i])*16777619u;
  if(with_names) {
    bytes=(const unsigned char *)names_saved;
    for(unsigned i=0;i<sizeof names_saved;i++) hash=(hash^bytes[i])*16777619u;
  }
  if(with_names>=2){bytes=(const unsigned char *)tone_bank;for(unsigned i=0;i<sizeof tone_bank;i++)hash=(hash^bytes[i])*16777619u;}
  if(with_names>=3){   /* Lab 7 wrote 8-byte records, Lab 8 writes 32 */
    unsigned n=with_names==3?PROGRAM_COUNT*8:sizeof extra_bank;
    bytes=(const unsigned char *)extra_bank;for(unsigned i=0;i<n;i++)hash=(hash^bytes[i])*16777619u;}
  return hash;
}
#define PRESET_MAGIC 0x3150544du
#define NAMED_MAGIC 0x3250544du
#define TONE_MAGIC 0x3350544du
#define EXTRA_MAGIC 0x3450544du
#define EXTRA2_MAGIC 0x3550544du
static int bank_magic(unsigned magic){return magic==PRESET_MAGIC || magic==NAMED_MAGIC || magic==TONE_MAGIC || magic==EXTRA_MAGIC || magic==EXTRA2_MAGIC;}
static int bank_level(unsigned magic){return magic==EXTRA2_MAGIC?4:magic==EXTRA_MAGIC?3:magic==TONE_MAGIC?2:magic==NAMED_MAGIC;}
static void widen_extras(void){   /* Lab 7 records (8 bytes) to Lab 8 (32 bytes), in place */
  unsigned char *b=(unsigned char *)extra_bank;
  for(int i=PROGRAM_COUNT-1;i>=0;i--){for(int k=31;k>=8;k--)b[i*32+k]=0;for(int k=7;k>=0;k--)b[i*32+k]=b[i*8+k];}
}
static void programs_init(void) {
  capture_patch(&initial_patch);
  BankHeader h[2]={{0}};
  for (int i=0;i<2;i++) if(user_read(i*4096,&h[i],sizeof h[i])) h[i].magic=0;
  int first=bank_magic(h[1].magic) && (!bank_magic(h[0].magic) || (int)(h[1].generation-h[0].generation)>0);
  for(int j=0;j<2;j++) {
    int i=first^j;
    if(bank_magic(h[i].magic) && !user_read(i*4096+sizeof h[i],&bank,sizeof bank)
       && (h[i].magic==PRESET_MAGIC || !user_read(8192+i*4096,names_saved,sizeof names_saved))
       && (bank_level(h[i].magic)<2 || !user_read(8192+i*4096+sizeof names_saved,tone_bank,sizeof tone_bank))
       && (bank_level(h[i].magic)<3 || !user_read(8192+i*4096+sizeof names_saved+sizeof tone_bank,extra_bank,bank_level(h[i].magic)==3?PROGRAM_COUNT*8:sizeof extra_bank))
       && bank_hash(h[i].generation,bank_level(h[i].magic))==h[i].checksum) {
      bank_sector=i; bank_generation=h[i].generation; break;
    }
  }
  if(bank_sector<0) for(int i=0;i<PROGRAM_COUNT;i++) bank.custom[i]=0;
  if(bank_sector<0 || h[bank_sector].magic==PRESET_MAGIC)
    for(int i=0;i<PROGRAM_COUNT;i++) names_saved[i][0]=0;
  if(bank_sector<0 || bank_level(h[bank_sector].magic)<2) for(int i=0;i<PROGRAM_COUNT;i++)tone_bank[i]=neutral_tone;
  if(bank_sector<0 || bank_level(h[bank_sector].magic)<3) for(int i=0;i<PROGRAM_COUNT;i++)extra_bank[i]=no_extra;
  else if(bank_level(h[bank_sector].magic)==3)widen_extras();
  recall_program(0);
  program_notice=0;
}
static void save_program(void) {
  int slot=destination;
  Patch old=bank.patch[slot];TonePatch old_tone=tone_bank[slot];ExtraPatch old_extra=extra_bank[slot];
  unsigned char custom=bank.custom[slot];
  char old_name[NAME_LEN+1]; copy_name(old_name,names_saved[slot]);
  if(deleting) {
    bank.custom[slot]=EMPTY_SLOT; names_saved[slot][0]=0;
  } else {
    capture_patch(&bank.patch[slot]);capture_tone(&tone_bank[slot]);capture_extra(&extra_bank[slot]); bank.custom[slot]=1;
    copy_name(names_saved[slot],draft[0]?draft:"Untitled");
  }
  int target=bank_sector==0?1:0;
  unsigned offset=target*4096, generation=bank_generation+1;
  BankHeader h={EXTRA2_MAGIC,generation,bank_hash(generation,4)};
  if(user_erase(offset) || user_erase(8192+offset) ||
     user_write(8192+offset,names_saved,sizeof names_saved) ||
     user_write(8192+offset+sizeof names_saved,tone_bank,sizeof tone_bank) ||
     user_write(8192+offset+sizeof names_saved+sizeof tone_bank,extra_bank,sizeof extra_bank) ||
     user_write(offset+sizeof h,&bank,sizeof bank) || user_write(offset,&h,sizeof h)) {
    tone_bank[slot]=old_tone;extra_bank[slot]=old_extra;bank.patch[slot]=old; bank.custom[slot]=custom; copy_name(names_saved[slot],old_name);
    program_notice="SAVE FAILED";
  } else {
    bank_sector=target; bank_generation=generation;
    if(!deleting) program=slot;
    else if(slot==program) recall_program(slot);
    program_notice=deleting?"PRESET DELETED":"PRESET SAVED";
  }
  naming=0;program_notice_time=fm1_ms;
}
