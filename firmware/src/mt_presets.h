/* 64 guitar/tape programs. Two alternating flash sectors keep the previous
 * complete set available until the new payload and its checksum are committed. */
enum { PROGRAM_COUNT = 64 };
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
  const char *tone=tone_names[slot%8], *space=space_names[slot/8];
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
static TonePatch tone_bank[PROGRAM_COUNT],tone_working[2];
static void capture_tone(TonePatch *t) {
  for(int k=0;k<4;k++){t->eq[k]=knob[EQ][k];t->filter[k]=knob[FILTER][k];}
}
static void apply_tone(const TonePatch *t) {
  for(int k=0;k<4;k++){
    knob[EQ][k]=bound(t->eq[k],0,maximum[EQ][k]);
    knob[FILTER][k]=bound(t->filter[k],0,maximum[FILTER][k]);
  }
}
static void capture_patch(Patch *p) {
  for (unsigned i=0;i<sizeof patch_pages;i++)
    for(int k=0;k<4;k++) p->values[i][k]=knob[patch_pages[i]][k];
  /* The high bit distinguishes tenths-of-a-second patches from legacy SWELL patches. */
  p->values[12][1]=knob[HEAD2][0]; /* spare OFF_TAPE byte in existing patches */
  p->vibrato=128|vib_on;
}
static Patch working[2];
static int working_valid[2];

static void apply_patch(const Patch *p) {
  for (unsigned i=0;i<sizeof patch_pages;i++)
    for(int k=0;k<4;k++) knob[patch_pages[i]][k]=p->values[i][k];
  knob[HEAD2][0]=p->values[12][1]==1;
  vib_on=p->vibrato & 1;
  if (!(p->vibrato & 128)) {
    knob[HOME][0]=bound((knob[SOUND][2]+1)*10,40,80);
    knob[SOUND][2]=0;
  }
}

static void switch_engine(int next) {
  int current=knob[SOUND][2];
  if (current==next) return;
#if !defined(GRANA_WEB) && !defined(DINGHY_HOST)
  unsigned flags=irq_save();
#endif
  capture_patch(&working[current]);capture_tone(&tone_working[current]);
  working_valid[current]=1;
  if (working_valid[next]) {apply_patch(&working[next]);apply_tone(&tone_working[next]);} else apply_tone(&neutral_tone);
  if(!working_valid[next] && !next) knob[SOUND][0]=0;
  if(!working_valid[next] && next) {
    knob[ARP][0]=0;knob[ARP][1]=10;knob[ARP][2]=48;knob[ARP][3]=0;
    knob[HEAD2][0]=0;knob[SOUND][0]=0;knob[ART][0]=0;
    knob[FUZZ][3]=0;knob[AMP][0]=0;knob[WAH][3]=0;vib_on=0;
  }
  knob[SOUND][2]=next;
  if(page==HEAD2 && !next) page=ARP;
#if !defined(GRANA_WEB) && !defined(DINGHY_HOST)
  irq_restore(flags);
#endif
}

static void recall_program(int slot) {
  naming=0;
  Patch p = bank.custom[slot]==1 ? bank.patch[slot] : initial_patch;
#if !defined(GRANA_WEB) && !defined(DINGHY_HOST)
  unsigned flags=irq_save();
#endif
  working_valid[0]=working_valid[1]=0;
  apply_patch(&p);apply_tone(bank.custom[slot]==1?&tone_bank[slot]:&neutral_tone);
  if (!bank.custom[slot]) {
    const unsigned char *t=tones[slot%8], *s=spaces[slot/8];
    knob[SOUND][0]=t[0]; knob[SOUND][1]=t[1]; knob[FUZZ][0]=t[2]; knob[FUZZ][3]=t[3];
    knob[AMP][0]=t[4]; knob[AMP][2]=t[5]; knob[WAH][3]=t[6];
    knob[ENV][0]=s[0]; knob[HOME][1]=s[1]; knob[HOME][0]=10*s[2];
    knob[PLATE][0]=s[3]; knob[PLATE][1]=s[4]; knob[OFF_TAPE][0]=s[5];
    knob[ART][1]=s[6]; knob[LFO][1]=knob[ART][2]=s[7];
  }
#if !defined(GRANA_WEB) && !defined(DINGHY_HOST)
  irq_restore(flags);
#endif
  if(page==HEAD2 && !knob[SOUND][2]) page=ARP;
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
  if(with_names==2){bytes=(const unsigned char *)tone_bank;for(unsigned i=0;i<sizeof tone_bank;i++)hash=(hash^bytes[i])*16777619u;}
  return hash;
}
#define PRESET_MAGIC 0x3150544du
#define NAMED_MAGIC 0x3250544du
#define TONE_MAGIC 0x3350544du
static int bank_magic(unsigned magic){return magic==PRESET_MAGIC || magic==NAMED_MAGIC || magic==TONE_MAGIC;}
static void programs_init(void) {
  capture_patch(&initial_patch);
  BankHeader h[2]={{0}};
  for (int i=0;i<2;i++) if(user_read(i*4096,&h[i],sizeof h[i])) h[i].magic=0;
  int first=bank_magic(h[1].magic) && (!bank_magic(h[0].magic) || (int)(h[1].generation-h[0].generation)>0);
  for(int j=0;j<2;j++) {
    int i=first^j;
    if(bank_magic(h[i].magic) && !user_read(i*4096+sizeof h[i],&bank,sizeof bank)
       && (h[i].magic==PRESET_MAGIC || !user_read(8192+i*4096,names_saved,sizeof names_saved))
       && (h[i].magic!=TONE_MAGIC || !user_read(8192+i*4096+sizeof names_saved,tone_bank,sizeof tone_bank))
       && bank_hash(h[i].generation,h[i].magic==TONE_MAGIC?2:h[i].magic==NAMED_MAGIC)==h[i].checksum) {
      bank_sector=i; bank_generation=h[i].generation; break;
    }
  }
  if(bank_sector<0) for(int i=0;i<PROGRAM_COUNT;i++) bank.custom[i]=0;
  if(bank_sector<0 || h[bank_sector].magic==PRESET_MAGIC)
    for(int i=0;i<PROGRAM_COUNT;i++) names_saved[i][0]=0;
  if(bank_sector<0 || h[bank_sector].magic!=TONE_MAGIC) for(int i=0;i<PROGRAM_COUNT;i++)tone_bank[i]=neutral_tone;
  recall_program(0);
  program_notice=0;
}
static void save_program(void) {
  int slot=destination;
  Patch old=bank.patch[slot];TonePatch old_tone=tone_bank[slot];
  unsigned char custom=bank.custom[slot];
  char old_name[NAME_LEN+1]; copy_name(old_name,names_saved[slot]);
  if(deleting) {
    bank.custom[slot]=EMPTY_SLOT; names_saved[slot][0]=0;
  } else {
    capture_patch(&bank.patch[slot]);capture_tone(&tone_bank[slot]); bank.custom[slot]=1;
    copy_name(names_saved[slot],draft[0]?draft:"Untitled");
  }
  int target=bank_sector==0?1:0;
  unsigned offset=target*4096, generation=bank_generation+1;
  BankHeader h={TONE_MAGIC,generation,bank_hash(generation,2)};
  if(user_erase(offset) || user_erase(8192+offset) ||
     user_write(8192+offset,names_saved,sizeof names_saved) ||
     user_write(8192+offset+sizeof names_saved,tone_bank,sizeof tone_bank) ||
     user_write(offset+sizeof h,&bank,sizeof bank) || user_write(offset,&h,sizeof h)) {
    tone_bank[slot]=old_tone;bank.patch[slot]=old; bank.custom[slot]=custom; copy_name(names_saved[slot],old_name);
    program_notice="SAVE FAILED";
  } else {
    bank_sector=target; bank_generation=generation;
    if(!deleting) program=slot;
    else if(slot==program) recall_program(slot);
    program_notice=deleting?"PRESET DELETED":"PRESET SAVED";
  }
  naming=0;program_notice_time=fm1_ms;
}
