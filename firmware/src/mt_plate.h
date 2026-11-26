/* Compact figure-eight plate. The tank runs at 22050 Hz, after the tape mix. */
typedef struct { int offset, size, head; } PlateLine;
static int16_t plate_memory[19000];
static PlateLine lines[13];
static void plate_init(void) {
  static const int lengths[] = {106,79,281,206,505,3300,1335,2756,680,3123,1970,2345,1765};
  int offset = 0;
  for (int i=0; i<13; i++) {
    lines[i]=(PlateLine){offset,lengths[i],0};
    offset += lengths[i];
  }
}
static inline __attribute__((always_inline)) int tap(int id, int age) {
  PlateLine *p=&lines[id];
  int pos=p->head-age;
  if(pos<0) pos+=p->size;
  return plate_memory[p->offset+pos];
}
static inline __attribute__((always_inline)) void push(int id,int v) {
  PlateLine *p=&lines[id];
  plate_memory[p->offset+p->head]=(int16_t)bound(v,-32760,32760);
  if(++p->head==p->size) p->head=0;
}
static AUDIO_FAST int diffuse(int id,int v,int gain,int delay_q8) {
  int delay=delay_q8>>8, frac=delay_q8&255;
  int old=tap(id,delay);
  if(frac) old+=(tap(id,delay+1)-old)*frac/256;
  int out=old-v*gain/256;
  push(id,v+out*gain/256);
  return out;
}
static AUDIO_FAST int delay_line(int id,int v) {
  int old=tap(id,lines[id].size);
  push(id,v);
  return old;
}
static int tank_end[2], tank_lp[2], wet_l,wet_r,prev_l,prev_r,plate_half,plate_input;
static unsigned plate_phase;
static AUDIO_FAST void plate_tick(int sample,int decay,int damp,int pre) {
  if(pre) {
    int delayed=tap(12,pre);
    push(12,sample);
    sample=delayed;
  } else push(12,sample);
  for(int i=0;i<4;i++) sample=diffuse(i,sample,i<2?192:160,lines[i].size*256);
  plate_phase+=117200u;
  int previous=tank_end[1];
  for(int i=0;i<2;i++) {
    int j=4+i*4;
    int v=sample+(i?tank_end[0]:previous)*decay/256;
    int wobble=sinus(plate_phase+(i?0x40000000u:0u))*5;
    v=diffuse(j,v,-170,(lines[j].size-5)*256+wobble);
    v=delay_line(j+1,v);
    tank_lp[i]+=(v-tank_lp[i])*damp/256;
    v=diffuse(j+2,tank_lp[i]*decay/256,140,lines[j+2].size*256);
    tank_end[i]=delay_line(j+3,v);
  }
  prev_l=wet_l; prev_r=wet_r;
  wet_l=(tap(5,198)+tap(5,2203)-tap(6,142)+tap(7,1478)-tap(9,1474)-tap(11,790))/3;
  wet_r=(tap(9,261)+tap(9,2690)-tap(10,907)+tap(11,1981)-tap(5,1563)-tap(7,246))/3;
}
