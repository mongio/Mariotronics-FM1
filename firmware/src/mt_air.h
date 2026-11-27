/* Passive Wi-Fi control source. Only broadcast beacon names and signal
 * statistics feed the instrument. No network association is needed. */
typedef struct {
    uint8_t id[6],channel;
    char name[19];
    int dbm;
    unsigned last,traffic;
} AirStation;
static AirStation air_stations[8];
static unsigned air_count,air_channel,air_scan_at,air_sample_at;
static int air_started,air_pick,air_locked,air_frozen;
static unsigned air_tail,air_used,air_play;
static uint8_t air_history[160][2];
static volatile int air_level,air_noise;
static int air_filtered;
static int air_heat[14];
static int air_same(const uint8_t *a,const uint8_t *b){
    for(unsigned i=0;i<6;i++)if(a[i]!=b[i])return 0;
    return 1;
}
static void air_receive(const uint8_t *frame,unsigned length,int dbm,unsigned channel,unsigned now){
    if(length<24 || dbm>=0 || dbm < -120)return;
    if(channel>=1 && channel<=13 && dbm+100>air_heat[channel])air_heat[channel]=dbm+100;
    int beacon=frame[0]==0x80 && length>=36;
    for(unsigned i=0;i<air_count;i++){
        AirStation *s=&air_stations[i];
        if(air_same(frame+10,s->id)){
            if(s->traffic<255)s->traffic++;
            s->dbm=(3*s->dbm+dbm)/4;s->last=now;
            /* A beacon can be heard on an adjacent channel. Keep its advertised channel. */
            return;
        }
    }
    if(!beacon)return;
    const uint8_t *name=0;
    unsigned name_len=0;
    for(unsigned pos=36;pos+2<=length;){
        unsigned tag=frame[pos++],n=frame[pos++];
        if(n>length-pos)return;
        if(tag==0 && n<=32){name=frame+pos;name_len=n;}
        if(tag==3 && n==1 && frame[pos]>=1 && frame[pos]<=13)channel=frame[pos];
        pos+=n;
    }
    if(!name)return;
    unsigned slot=air_count;
    if(slot==8){
        for(slot=0;slot<8;slot++)
            if((int)slot!=(knob[UNCERT][0]?knob[UNCERT][2]:knob[TIMBRE][0])-1 && now-air_stations[slot].last>10000)break;
        if(slot==8)return;
    }else air_count++;
    AirStation *s=&air_stations[slot];
    __builtin_memset(s,0,sizeof *s);
    for(int i=0;i<6;i++)s->id[i]=frame[10+i];
    for(unsigned i=0;i<name_len && i<18;i++){
        unsigned c=name[i];s->name[i]=c>=32 && c<127?(char)c:'?';
    }
    s->channel=channel;s->dbm=dbm;s->last=now;s->traffic=1;
}
#if !defined(GRANA_WEB) && !defined(DINGHY_HOST)
#include "fm1_wifi.h"
#include "fm1_wifi_wlc.h"
/* WL82 DMA needs a word-aligned buffer in low SRAM. */
static uint32_t air_dma[(FM1_WIFI_RX_MAX+8)/4];
static int air_hw_start(void){
    if(!bt_up){
        if(!radio_power(1))return 0;
        radio_power(0);
    }
    if(!radio_on && bt_up)fm1_ble_irqs_hold(1);   /* 1.20: no BLE interrupts while Wi-Fi owns the radio */
    fm1_wifi_up(fm1_wlc_hi,fm1_wlc_c,FM1_WLC_N);
    fm1_wifi_mac_wr(FM1_WIFI_RX_FILTER,FM1_WIFI_RX_FILTER_ALL);
    return 1;
}
static void air_tune(unsigned ch){fm1_wifi_set_channel(ch);}
static void air_stop(void){
    fm1_wifi_wr(FM1_WL_EN,fm1_wifi_rd(FM1_WL_EN)&~FM1_WL_EN_WIFI);
}
static void air_poll(unsigned now){
    for(unsigned i=0;i<2;i++){                    /* 1.20: two frames per pass, the main loop stays quick */
        int n=fm1_wifi_rx(air_dma,FM1_WIFI_RX_MAX);
        if(n<(int)FM1_WIFI_RXWI)break;
        const uint8_t *rx=(const uint8_t *)(air_dma+1);
        unsigned length=fm1_wifi_rx_mpdu_len(rx);
        if(length<=(unsigned)n-FM1_WIFI_RXWI)
            air_receive(rx+FM1_WIFI_RXWI,length,fm1_wifi_rx_rssi(rx),air_channel,now);
    }
}
#else
/* Explicit simulator fixture. The browser cannot read the FM-1 antenna.
   Tests can make it louder and busier, like a home router next to the FM-1. */
static int air_fixture_dbm=-67,air_fixture_frames=1;
#ifdef GRANA_WEB
WEB_EXPORT("rf_fixture") void web_rf_fixture(int dbm,int frames){air_fixture_dbm=dbm;air_fixture_frames=frames<1?1:frames;}
#endif
static int air_hw_start(void){return 1;}
static void air_stop(void){}
static void air_tune(unsigned ch){(void)ch;}
static void air_poll(unsigned now){
    static unsigned last;
    if(now-last<65)return;
    last=now;
    if(air_channel!=1 && air_channel!=6 && air_channel!=11)return;
    uint8_t beacon[42]={0x80};
    beacon[10]=air_channel;beacon[37]=4;
    beacon[38]='D';beacon[39]='E';beacon[40]='M';beacon[41]='O';
    for(int i=0;i<air_fixture_frames;i++)
        air_receive(beacon,sizeof beacon,air_fixture_dbm+sinus(now*720000u+air_channel*30000000u+i*9000000u)/8,air_channel,now);
}
#endif
static void air_freeze(void){
    if(air_frozen){air_frozen=0;return;}
    if(!air_used)return;
    air_frozen=1;air_play=(air_tail+160-air_used)%160;
}
static void air_step(int on,unsigned now){
    if(!on){
        if(air_started==1)air_stop();
        rf_source=rf_amount=rf_amount2=0;air_started=0;air_frozen=0;air_level=air_noise=0;return;
    }
    if(!air_started){
        air_started=air_hw_start()?1:-1;
        air_count=air_tail=air_used=0;air_filtered=0;air_pick=-1;
        __builtin_memset(air_stations,0,sizeof air_stations);
        air_channel=1;air_scan_at=air_sample_at=now;
        if(air_started==1)air_tune(1);
    }
    if(air_started<0)return;
    int pick=(knob[UNCERT][0]?knob[UNCERT][2]:knob[TIMBRE][0])-1;
    air_locked=pick>=0 && pick<(int)air_count;
    if(air_locked && air_channel!=air_stations[pick].channel){
        air_channel=air_stations[pick].channel;air_tune(air_channel);
    }else if(!air_locked && now-air_scan_at>=180){
        air_channel=air_channel%13+1;air_tune(air_channel);air_scan_at=now;
    }
    air_poll(now);
    if(now-air_sample_at<50)return;
    air_sample_at=now;
    if(!air_locked){
        pick=-1;
        for(unsigned i=0;i<air_count;i++)
            if(now-air_stations[i].last<3000 && (pick<0 || air_stations[i].dbm>air_stations[pick].dbm))pick=i;
    }
    air_pick=pick;
    int target=0,noise=0;
    if(pick>=0 && now-air_stations[pick].last<(air_locked?800u:3000u)){
        target=bound((air_stations[pick].dbm+95)*4,0,255);
        noise=bound(air_stations[pick].traffic*13,0,255);
    }
    for(unsigned i=0;i<air_count;i++)air_stations[i].traffic=0;
    if(air_frozen){
        target=air_history[air_play][0];noise=air_history[air_play][1];
        air_play=(air_play+1)%160;
        if(air_play==air_tail)air_play=(air_tail+160-air_used)%160;
    }else{
        air_history[air_tail][0]=target;air_history[air_tail][1]=noise;
        air_tail=(air_tail+1)%160;if(air_used<160)air_used++;
    }
    air_filtered+=(target*256-air_filtered)/(1+4*(knob[UNCERT][0]?knob[UNCERT][3]:knob[TIMBRE][1]));
    air_level=air_filtered/256;air_noise=noise;
    static int previous;
    if(!knob[UNCERT][0] || !target)rf_source=0;
    else if(!knob[UNCERT][1])rf_source=bound(air_level*200/255-100,-100,100);
    else if(noise>previous)rf_source=knob[UNCERT][1]==1?bound((target+noise)*200/255-100,-100,100):bound(20+noise*2,0,100);
    else if(knob[UNCERT][1]==2)rf_source-=1+rf_source/(2+knob[UNCERT][3]);
    if(knob[UNCERT][1]==2 && rf_source<0)rf_source=0;
    previous=noise;rf_amount=rf_source*knob[ROUTE][1]/100;rf_amount2=rf_source*knob[ROUTE][3]/100;
}
