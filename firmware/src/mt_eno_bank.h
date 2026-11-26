/* Optional read-in-place sustain samples. A foreign bank is never erased. */
static const uint8_t *eno_flash;
static int eno_extended;
static const uint8_t eno_signature[16]="MTEP1-SAL-V3";
static int eno_owns_bank(void){
    return eno_flash && !__builtin_memcmp(eno_flash,eno_signature,sizeof eno_signature);
}
static void eno_open(void){
    eno_extended=0;
#ifdef GRANA_WEB
    web_ub_init();eno_flash=web_ubank;
#elif !defined(DINGHY_HOST)
    if(!flash_ok)return;
    fl_plain_window_init();eno_flash=fm1_xip_ptr(UB_AT);
#endif
    if(!eno_owns_bank())return;
    unsigned hash=2166136261u;
    for(unsigned i=0;i<TAIL_BYTES;i++)hash=(hash^eno_flash[i])*16777619u;
    eno_extended=hash==TAIL_HASH;
}

#ifndef DINGHY_HOST
/* 128-byte pieces, indexed by two MIDI-safe bytes. The editor can resume a
   partial transfer. Each page is read back. An alien bank is read-only. */
static int eno_transfer(const uint8_t *p,unsigned n){
    static unsigned next;
    static int accepting;
    if(!eno_flash || n<3)return 1;
    unsigned op=p[1];
    if(op==10 && n==3){
        accepting=0;next=0;
        if(eno_extended)return 3;
        if(!eno_owns_bank()){
            for(unsigned i=0;i<UB_LEN;i++)if(eno_flash[i]!=255)return 2;
        }
        accepting=1;return 0;
    }
    if(!accepting)return 1;
    if(op==12 && n==3 && next==TAIL_BYTES){
        accepting=0;eno_open();return eno_extended?0:1;
    }
    if(op!=11 || n<4 || p[2]+128u*p[3]!=next/128u || next>=TAIL_BYTES)return 1;
    unsigned count=TAIL_BYTES-next;if(count>128)count=128;
    if(n!=4+2*count)return 1;
    uint8_t data[128];
    for(unsigned i=0;i<count;i++){
        if((p[4+2*i]|p[5+2*i])>15)return 1;
        data[i]=16*p[4+2*i]+p[5+2*i];
        if(!next && i<16 && data[i]!=eno_signature[i])return 1;
        if((eno_flash[next+i]&data[i])!=data[i])return 1;
    }
    if(__builtin_memcmp(eno_flash+next,data,count) &&
       (ub_write(next,data,count) || __builtin_memcmp(eno_flash+next,data,count)))return 1;
    next+=count;return 0;
}
#endif
