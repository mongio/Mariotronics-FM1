/* Preset transfer over USB MIDI. A complete incoming bank is validated in
 * external RAM before the inactive flash sectors are touched. */
typedef struct {
  __typeof__(bank) programs;
  char names[PROGRAM_COUNT][NAME_LEN+1];
  TonePatch tones[PROGRAM_COUNT];
} BackupBank;
static BackupBank transfer_bank POOLMEM;
enum { CHUNK=80, CHUNKS=(sizeof(BackupBank)+CHUNK-1)/CHUNK };
static unsigned transfer_index, transfer_mode, transfer_generation;
static void snapshot_bank(void) {
  transfer_bank.programs=bank;
  for(int i=0;i<PROGRAM_COUNT;i++){
    for(int j=0;j<=NAME_LEN;j++)transfer_bank.names[i][j]=names_saved[i][j];
    transfer_bank.tones[i]=tone_bank[i];
  }
}
static int valid_backup(void) {
  for(int i=0;i<PROGRAM_COUNT;i++){
    const Patch *p=&transfer_bank.programs.patch[i];
    if(transfer_bank.programs.custom[i]>EMPTY_SLOT || transfer_bank.names[i][NAME_LEN])return 0;
    for(int j=0;j<NAME_LEN && transfer_bank.names[i][j];j++){
      unsigned char c=transfer_bank.names[i][j];
      if(c<32 || c>126)return 0;
    }
    if(transfer_bank.programs.custom[i]!=1)continue;
    for(unsigned row=0;row<sizeof patch_pages;row++)for(int k=0;k<4;k++){
      int pg=patch_pages[row],v=p->values[row][k],hi=maximum[pg][k];
      if((pg==SCALE && k>=2)||(pg==OFF_TAPE && k>0))continue;
      if(pg==ARP && !k && p->values[1][2]>=2)hi=4;
      if(pg==SOUND && k==0 && p->values[1][2])hi=p->values[1][2]==1?5:3;
      if(!(p->vibrato&128) && ((pg==SOUND && k==2)||(pg==HOME && k==0)))continue;
      if(v<minimum[pg][k] || v>hi)return 0;
    }
  }
  return 1;
}
static int restore_backup(void) {
  if(!valid_backup() || transfer_generation!=bank_generation)return 0;
  unsigned generation=bank_generation+1, hash=2166136261u^generation;
  const unsigned char *src=(const unsigned char *)&transfer_bank;
  for(unsigned i=0;i<sizeof transfer_bank;i++)hash=(hash^src[i])*16777619u;
  int target=bank_sector==0?1:0;unsigned off=target*4096;
  BankHeader header={TONE_MAGIC,generation,hash};
  if(user_erase(off)||user_erase(8192+off)||
     user_write(off+sizeof header,&transfer_bank.programs,sizeof bank)||
     user_write(8192+off,transfer_bank.names,sizeof names_saved)||
     user_write(8192+off+sizeof names_saved,transfer_bank.tones,sizeof tone_bank)||
     user_write(off,&header,sizeof header))return 0;
  bank=transfer_bank.programs;
  for(int i=0;i<PROGRAM_COUNT;i++){
    for(int j=0;j<=NAME_LEN;j++)names_saved[i][j]=transfer_bank.names[i][j];
    tone_bank[i]=transfer_bank.tones[i];
  }
  bank_sector=target;bank_generation=generation;recall_program(program);return 1;
}
static void app_sysex(const uint8_t *p,uint32_t n) {
  if(n<3 || p[0]!=0x4d)return;
  unsigned op=p[1],idx=p[2];
  uint8_t reply[9+2*CHUNK]={0xf0,0x7d,0x47,0x52,0x4d,op,idx,0};
  unsigned len=8;
  uint8_t *data=(uint8_t *)&transfer_bank;
  unsigned left=idx<CHUNKS?sizeof transfer_bank-idx*CHUNK:0;
  unsigned count=left>CHUNK?CHUNK:left;
  if((op==1 || op==3) && n==3 && idx==0){
    transfer_mode=op;transfer_index=0;transfer_generation=bank_generation;
    if(op==1)snapshot_bank();
    reply[len++]=CHUNKS;
  }else if(op==2 && transfer_mode==1 && count && n==3){
    for(unsigned i=0;i<count;i++){
      unsigned b=data[idx*CHUNK+i];reply[len++]=b>>4;reply[len++]=b&15;
    }
  }else if(op==4 && transfer_mode==3 && idx==transfer_index && count && n==3+count*2){
    for(unsigned i=0;i<count;i++){
      if((p[3+2*i]|p[4+2*i])>15){reply[7]=1;break;}
      data[idx*CHUNK+i]=p[3+2*i]*16+p[4+2*i];
    }
    if(!reply[7])transfer_index++;
  }else if(op==5 && n==3 && transfer_mode==3 && transfer_index==CHUNKS){
    transfer_mode=0;reply[7]=!restore_backup();
  }else reply[7]=1;
  reply[len++]=0xf7;sysex_send(reply,len);
}
