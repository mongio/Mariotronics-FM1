/* Replaceable piano samples, read in place from the user flash.
 *
 * Twelve slots: six roots (C2 A2 F#3 D#4 C5 A5) x soft and hard. Each slot is
 *   0 factory   the 1.78 s recording in the firmware,
 *   1 sustain   the factory recording continued by tail blocks in the bank,
 *   2 replaced  a whole recording from the bank (a WAV prepared by the editor).
 * Blocks are the factory format: 64 frames of IMA ADPCM at 16 kHz in 35 bytes.
 *
 * Bank v2 ("MTEP2-SAMPLES"): a 128-byte header (signature, length, FNV-1a hash
 * of everything after the header, twelve slot records) followed by the blocks.
 * The Lab 5-7 sustain bank (v1, "MTEP1-SAL-V3") is still recognised: all
 * twelve slots as sustain. Another firmware's bank (GRANA, VIRGO) is never
 * erased unless the editor asks with the replace flag, after the player agreed. */
typedef struct { unsigned offset; unsigned short blocks; unsigned char mode, spare; } EnoSlot;
static EnoSlot eno_slots[12] POOLMEM;
static const uint8_t *eno_flash;
static int eno_extended, eno_version;            /* version: 0 empty, 1 v1, 2 v2, 3 foreign */
static const uint8_t eno_signature[16]="MTEP1-SAL-V3";
static const uint8_t eno_signature2[16]="MTEP2-SAMPLES";
enum { ENO_HEADER=128, ENO_BLOCK=35, ENO_MAX_BLOCKS=2048 };
static unsigned eno_u32(const uint8_t *p){return p[0]|p[1]<<8|p[2]<<16|(unsigned)p[3]<<24;}
static int eno_owns_bank(void){
    return eno_flash && (!__builtin_memcmp(eno_flash,eno_signature,sizeof eno_signature) ||
                         !__builtin_memcmp(eno_flash,eno_signature2,sizeof eno_signature2));
}
static int eno_blank(void){
    for(unsigned i=0;i<UB_LEN;i+=64)if(eno_flash[i]!=255 || eno_flash[i+17]!=255)return 0;
    return 1;
}
/* Frames available in a slot. */
static inline __attribute__((always_inline)) unsigned eno_frames(unsigned zone){
    const EnoSlot *d=&eno_slots[zone];
    return d->mode==2?d->blocks*64u:PIANO_FRAMES+(d->mode==1?d->blocks*64u:0u);
}
static void eno_open(void){
    eno_extended=0;eno_version=0;
    for(int z=0;z<12;z++){eno_slots[z].mode=0;eno_slots[z].blocks=0;eno_slots[z].offset=0;}
#ifdef GRANA_WEB
    web_ub_init();eno_flash=web_ubank;
#elif !defined(DINGHY_HOST)
    if(!flash_ok)return;
    fl_plain_window_init();eno_flash=fm1_xip_ptr(UB_AT);
#endif
    if(!eno_flash)return;
    if(!__builtin_memcmp(eno_flash,eno_signature,sizeof eno_signature)){
        unsigned hash=2166136261u;
        for(unsigned i=0;i<TAIL_BYTES;i++)hash=(hash^eno_flash[i])*16777619u;
        if(hash!=TAIL_HASH){eno_version=3;return;}
        for(int z=0;z<12;z++){eno_slots[z].mode=1;eno_slots[z].blocks=TAIL_FRAMES/64;eno_slots[z].offset=16+ENO_BLOCK*z*(TAIL_FRAMES/64);}
        eno_version=1;eno_extended=1;return;
    }
    if(__builtin_memcmp(eno_flash,eno_signature2,sizeof eno_signature2)){eno_version=eno_blank()?0:3;return;}
    unsigned length=eno_u32(eno_flash+16),want=eno_u32(eno_flash+20),hash=2166136261u;
    if(length<ENO_HEADER || length>UB_LEN){eno_version=3;return;}
    for(unsigned i=ENO_HEADER;i<length;i++)hash=(hash^eno_flash[i])*16777619u;
    if(hash!=want){eno_version=3;return;}
    EnoSlot read[12];
    for(int z=0;z<12;z++){
        const uint8_t *r=eno_flash+32+8*z;
        read[z].offset=eno_u32(r);read[z].blocks=(unsigned short)(r[4]|r[5]<<8);read[z].mode=r[6];read[z].spare=0;
        if(read[z].mode>2 || (read[z].mode && (!read[z].blocks || read[z].blocks>ENO_MAX_BLOCKS ||
           read[z].offset<ENO_HEADER || read[z].offset+ENO_BLOCK*read[z].blocks>length))){eno_version=3;return;}
    }
    for(int z=0;z<12;z++){eno_slots[z]=read[z];if(read[z].mode)eno_extended=1;}
    eno_version=2;
}

#ifndef DINGHY_HOST
/* Over USB SysEx, in 128-byte pieces indexed by two MIDI-safe bytes; each page is read back.
 *   10 begin   flags (bit 0: replace a foreign bank), image length in three 7-bit bytes
 *   11 piece   index lo, index hi, 128 bytes as nibbles (sectors are erased as they are reached)
 *   12 finish  verify and start using the bank
 *   13 status  reply: version, then the mode of each slot
 *   14 factory erase the bank (flags as in 10)
 *   15 read    64 bytes of the bank at index lo + 128*hi (the editor keeps the player's own files)
 *   16 slot    offset (three 7-bit bytes) and blocks (two) of slot lo */
static int eno_transfer(const uint8_t *p,unsigned n,uint8_t *reply,unsigned *len){
    static unsigned next,length,erased;
    static int accepting;
    if(!eno_flash || n<3)return 1;
    unsigned op=p[1];
    if(op==13){
        reply[(*len)++]=(uint8_t)eno_version;
        for(int z=0;z<12;z++)reply[(*len)++]=eno_slots[z].mode;
        return 0;
    }
    if(op==15 && n==4){
        unsigned at=(p[2]+128u*p[3])*64u;
        if(at+64>UB_LEN)return 1;
        for(unsigned i=0;i<64;i++){reply[(*len)++]=eno_flash[at+i]>>4;reply[(*len)++]=eno_flash[at+i]&15;}
        return 0;
    }
    if(op==16 && p[2]<12){
        const EnoSlot *d=&eno_slots[p[2]];
        reply[(*len)++]=d->offset&127;reply[(*len)++]=(d->offset>>7)&127;reply[(*len)++]=(d->offset>>14)&127;
        reply[(*len)++]=d->blocks&127;reply[(*len)++]=(d->blocks>>7)&127;
        return 0;
    }
    if(op==10 || op==14){
        int force=n>=3 && (p[2]&1);
        accepting=0;next=erased=0;
        if(eno_version==3 && !force)return 2;    /* GRANA/VIRGO samples: only with the player's consent */
        if(op==10){
            if(n<6)return 1;
            length=p[3]|p[4]<<7|(unsigned)p[5]<<14;
            if(length<ENO_HEADER || length>UB_LEN)return 1;
        }
        /* From here ENO plays its internal recordings until the new bank is verified. */
        eno_extended=0;for(int z=0;z<12;z++)eno_slots[z].mode=0;eno_version=0;
        if(op==14){
            for(unsigned off=0;off<UB_LEN;off+=4096){
                int dirty=0;for(unsigned i=0;i<4096;i++)if(eno_flash[off+i]!=255){dirty=1;break;}
                if(dirty && ub_erase(off))return 1;
            }
            eno_open();return 0;
        }
        accepting=1;return 0;
    }
    if(!accepting)return 1;
    if(op==12 && n==3 && next==length){
        accepting=0;eno_open();return eno_version==2?0:1;
    }
    if(op!=11 || n<4 || p[2]+128u*p[3]!=next/128u || next>=length)return 1;
    unsigned count=length-next;if(count>128)count=128;
    if(n!=4+2*count)return 1;
    uint8_t data[128];
    for(unsigned i=0;i<count;i++){
        if((p[4+2*i]|p[5+2*i])>15)return 1;
        data[i]=16*p[4+2*i]+p[5+2*i];
        if(!next && i<16 && data[i]!=eno_signature2[i])return 1;
    }
    while(erased<next+count){                   /* erase each 4 KB sector the first time it is reached */
        int dirty=0;for(unsigned i=0;i<4096 && erased+i<UB_LEN;i++)if(eno_flash[erased+i]!=255){dirty=1;break;}
        if(dirty && ub_erase(erased))return 1;
        erased+=4096;
    }
    if(ub_write(next,data,count) || __builtin_memcmp(eno_flash+next,data,count))return 1;
    next+=count;return 0;
}
#endif
