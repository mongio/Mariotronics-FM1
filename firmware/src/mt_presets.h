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

static void capture_patch(Patch *p) {
  for (unsigned i=0;i<sizeof patch_pages;i++)
    for(int k=0;k<4;k++) p->values[i][k]=knob[patch_pages[i]][k];
  p->vibrato=vib_on;
}
static void recall_program(int slot) {
  Patch p = bank.custom[slot] ? bank.patch[slot] : initial_patch;
#if !defined(GRANA_WEB) && !defined(DINGHY_HOST)
  unsigned flags=irq_save();
#endif
  for (unsigned i=0;i<sizeof patch_pages;i++)
    for(int k=0;k<4;k++) knob[patch_pages[i]][k]=p.values[i][k];
  vib_on=p.vibrato;
  if (!bank.custom[slot]) {
    const unsigned char *t=tones[slot%8], *s=spaces[slot/8];
    knob[SOUND][0]=t[0]; knob[SOUND][1]=t[1]; knob[FUZZ][0]=t[2]; knob[FUZZ][3]=t[3];
    knob[AMP][0]=t[4]; knob[AMP][2]=t[5]; knob[WAH][3]=t[6];
    knob[HOME][0]=knob[ENV][0]=s[0]; knob[HOME][1]=s[1]; knob[SOUND][2]=s[2]-1;
    knob[PLATE][0]=s[3]; knob[PLATE][1]=s[4]; knob[OFF_TAPE][0]=s[5];
    knob[ART][1]=s[6]; knob[LFO][1]=knob[ART][2]=s[7];
  }
#if !defined(GRANA_WEB) && !defined(DINGHY_HOST)
  irq_restore(flags);
#endif
  program=slot;
  program_notice=bank.custom[slot]?"USER PRESET":"FACTORY PRESET";
  program_notice_time=fm1_ms;
}
static unsigned bank_hash(unsigned generation) {
  const unsigned char *bytes=(const unsigned char *)&bank;
  unsigned hash=2166136261u ^ generation;
  for(unsigned i=0;i<sizeof bank;i++) hash=(hash^bytes[i])*16777619u;
  return hash;
}
#define PRESET_MAGIC 0x3150544du
static void programs_init(void) {
  capture_patch(&initial_patch);
  BankHeader h[2]={{0}};
  for (int i=0;i<2;i++) if(user_read(i*4096,&h[i],sizeof h[i])) h[i].magic=0;
  int first=h[1].magic==PRESET_MAGIC && (h[0].magic!=PRESET_MAGIC || (int)(h[1].generation-h[0].generation)>0);
  for(int j=0;j<2;j++) {
    int i=first^j;
    if(h[i].magic==PRESET_MAGIC && !user_read(i*4096+sizeof h[i],&bank,sizeof bank)
       && bank_hash(h[i].generation)==h[i].checksum) {
      bank_sector=i; bank_generation=h[i].generation; break;
    }
  }
  if(bank_sector<0) for(int i=0;i<PROGRAM_COUNT;i++) bank.custom[i]=0;
  recall_program(0);
  program_notice=0;
}
static void save_program(void) {
  Patch old=bank.patch[program];
  unsigned char custom=bank.custom[program];
  capture_patch(&bank.patch[program]); bank.custom[program]=1;
  int target=bank_sector==0?1:0;
  unsigned offset=target*4096, generation=bank_generation+1;
  BankHeader h={PRESET_MAGIC,generation,bank_hash(generation)};
  if(user_erase(offset) || user_write(offset+sizeof h,&bank,sizeof bank) || user_write(offset,&h,sizeof h)) {
    bank.patch[program]=old; bank.custom[program]=custom; program_notice="SAVE FAILED";
  } else {
    bank_sector=target; bank_generation=generation; program_notice="PRESET SAVED";
  }
  program_notice_time=fm1_ms;
}
