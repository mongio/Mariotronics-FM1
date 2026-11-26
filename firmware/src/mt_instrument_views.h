/* All drawing coordinates are in the upper 240 x 146 instrument window. */
static void dot_at(int x,int y,int radius,uint16_t color){
  for(int dy=-radius;dy<=radius;dy++){
    int dx=radius;
    while(dx*dx+dy*dy>radius*radius)dx--;
    cv_rect(x-dx,y+dy,2*dx+1,1,color);
  }
}
#include "mt_faces.h"
static void photo_region(int face,int x,int y,int width,int height){
  for(int row=0;row<height;row++)for(int col=0;col<width;col++){
    int src=(row*104/height)*240+col*240/width;
    unsigned bit=(unsigned)src*5, offset=bit>>3;
    unsigned pair=faces[face][offset] | faces[face][offset+1]<<8,index=(pair>>(bit&7))&31;
    cv_px[(y+row)*240+x+col]=swap16(face_palettes[face][index]);
  }
}
static void vintage_knob(int x,int y,int position,int range){
  for(int row=0;row<37;row++)for(int col=0;col<37;col++)
    cv_pset(x+col-18,y+row-18,knob_cap[row*37+col]);
  unsigned angle=0xa0000000u+(0xc0000000u/256)*(range?position*256/range:0);
  int dx=sinus(angle),dy=-sinus(angle+0x40000000u);
  for(int j=-12;j<=12;j++)for(int i=-12;i<=12;i++){
    int along=i*dx+j*dy,side=absolute(i*dy-j*dx);
    if(along<380 || along>1450 || side>=145)continue;
    int alpha=bound(180-side,0,128);
    uint16_t bg=knob_cap[(j+18)*37+i+18];
    int red=((bg>>11)*(128-alpha)+30*alpha)>>7;
    int green=(((bg>>5)&63)*(128-alpha)+56*alpha)>>7;
    int blue=((bg&31)*(128-alpha)+22*alpha)>>7;
    cv_pset(x+i,y+j,(red<<11)|(green<<5)|blue);
  }
}
static void fretboard(void){
  static const int open[]={64,59,55,50,45,40};
  lettering(78,2,mode_names[knob[SCALE][0]],silver,0);
  lettering(151,2,"ROOT",silver,0);
  lettering(190,2,tonic_names[knob[SCALE][1]],gold,0);
  cv_rect(32,26,202,90,RGB(61,40,32));
  for(int f=0;f<=12;f++)cv_rect(32+16*f,26,f?1:3,90,metal);
  for(int i=0;i<6;i++){
    int y=32+i*15;
    lettering(10,y-8,tonic_names[open[i]%12],silver,0);
    cv_line(19,y,233,y,i>2?gold:silver);
    for(int f=0;f<=12;f++){
      int note=open[i]+f;
      if(scale_note(note)==note)dot_at(f?26+16*f:23,y,note%12==knob[SCALE][1]?5:3,note%12==knob[SCALE][1]?gold:silver);
    }
  }
  static const int markers[]={3,5,7,9,12};
  for(int j=0;j<5;j++){int f=markers[j];char n[5];number(n,f);lettering(26+16*f,119,n,silver,0);}
}
static void modular_deck(unsigned clock){
  cv_rect(7,20,148,102,RGB(93,66,43));cv_rect(11,23,140,96,metal);
  for(int m=0;m<7;m++){
    int x=13+m*20;
    cv_rect(x,25,18,91,ink);
    for(int j=0;j<6;j++){
      cv_rect(x+2,29+3*j,14,1,metal);
      cv_rect(x+3+2*((m+j)%5),28+3*j,2,3,j&1?gold:silver);
    }
    for(int j=0;j<6;j++){
      int cx=x+5+(j&1)*8,cy=57+(j/2)*10;
      dot_at(cx,cy,3,metal);dot_at(cx,cy,2,RGB(28,31,30));
      cv_line(cx,cy,cx+((j+m)%3-1),cy-2,silver);
    }
    for(int j=0;j<7;j++){cv_rect(x+2,91+j*3,14,1,metal);cv_rect(x+2+2*((m+2*j)%6),90+j*3,3,2,j%3?silver:gold);}
  }
  cv_rect(160,23,75,95,RGB(92,65,41));cv_rect(163,26,69,89,metal);
  photo_region(5,10,23,142,96);
  for(int k=0;k<2;k++){
    int x=181+32*k,y=49;dot_at(x,y,15,silver);dot_at(x,y,12,ink);
    for(int a=0;a<3;a++){unsigned phase=clock*65536u+a*1431655765u;
      cv_line(x,y,x+sinus(phase)*12/128,y+sinus(phase+0x40000000u)*12/128,silver);}
    dot_at(x,y,3,gold);
  }
  cv_line(168,56,179,74,gold);cv_line(179,74,216,74,gold);cv_line(216,74,228,56,gold);
  for(int i=0;i<2;i++){int x=168+i*29;cv_rect(x,82,27,11,ink);cv_rect(x+1,83,25,8,RGB(214,185,130));cv_line(x+13,90,x+5+bound(vu_out/1200,0,15),84,ink);}
  for(int i=0;i<5;i++)cv_rect(171+11*i,96,7,9,silver);
  lettering(81,123,"ARP 2500",gold,0);lettering(198,123,"TAPE",silver,0);
}
#ifdef UI_PROP
static void pattern_view(void){
  static const unsigned char paths[][8]={{0,1,2,3,4,5,6,7},{0,1,2,3,2,1,0,1},{0,2,1,3,1,4,2,5},{0,3,1,4,2,5,1,3}};
  static unsigned glow[8];                       /* each step fades after it plays */
  int mode=knob[ARP][0], now=mode && phrase_size?(int)((phrase_step+7)&7):-1;
  lettering(120,9,patterns[mode],mode?gold:silver,0);
  cv_rect(14,112,212,1,RGB(40,42,38));
  for(int i=0;i<8;i++){
    int h=8+9*paths[bound(mode-1,0,3)][i], x=19+26*i;
    if(i==now)glow[i]=255;else if(glow[i]>12)glow[i]-=12;else glow[i]=0;
    unsigned g=glow[i];
    uint16_t body=RGB(60+(247-60)*g/255,62+(187-62)*g/255,58+(88-58)*g/255);
    if(!mode){cv_rect(x,112-h,18,1,metal);cv_rect(x,112-h,1,h,metal);cv_rect(x+17,112-h,1,h,metal);continue;}
    if(g>200)cv_rect(x-2,110-h,22,h+2,RGB(90,66,30));
    cv_rect(x,112-h,18,h,body);
    cv_rect(x,112-h,18,2,g?RGB(255,226,160):silver);
    disc(x+9,121,2,i==now?gold:RGB(50,52,48));
  }
}
#else
static void pattern_view(void){
  static const unsigned char paths[][8]={{0,1,2,3,4,5,6,7},{0,1,2,3,2,1,0,1},{0,2,1,3,1,4,2,5},{0,3,1,4,2,5,1,3}};
  int mode=knob[ARP][0];
  lettering(120,9,patterns[mode],gold,0);
  for(int i=0;i<8;i++){
    int h=8+9*paths[bound(mode-1,0,3)][i];
    cv_rect(19+26*i,112-h,18,h,mode && phrase_size && ((phrase_step+7)&7)==i?gold:metal);
  }
}
#endif
static void instrument_diagram(void){
  int kind=voice_kind(),a=knob[TIMBRE][0],b=knob[TIMBRE][1];
  if(page==HEAD2 || page==NOTE_FX){
    int back=page==NOTE_FX && knob[NOTE_FX][0];
    lettering(120,10,page==HEAD2?"SECOND PLAYBACK HEAD":back?"REVERSE NOTE":"FORWARD NOTE",gold,0);
    cv_line(18,100,222,100,metal);
    if(page==HEAD2){
      for(int i=0;i<2;i++){cv_rect(55+100*i,50,26,42,!i||knob[HEAD2][0]?silver:metal);cv_rect(65+100*i,70,5,29,ink);}
    }else for(int i=0;i<90;i++){int x=back?211-2*i:28+2*i;int h=(88-i)*(10+absolute(sinus(i*117000000u)))/240;cv_line(x,85-h,x,85+h,gold);}
    return;
  }
  if(kind==PIANO || kind==BRIGHT || kind>=EVENING){
    static const char *const titles[]={"ACOUSTIC PIANO","EVENING GLASS","DUSK CHOIR","TIDAL HAZE"};
    int face=kind>=EVENING?1+kind-EVENING:0;
    panel_text(-7,2,kind==BRIGHT?"BRIGHT PIANO":titles[face],RGB(237,236,228),226,95);
    photo_region(face,0,27,240,104);
    if(kind<=BRIGHT){
      int level=0; for(int i=0;i<4;i++)if(strings[i].active && strings[i].kind<=BRIGHT)level+=strings[i].level/4096;
      if(level)for(int x=104,last=72;x<229;x++){
        int y=72-(x-104)/8+sinus((unsigned)x*120000000u+fm1_ms*9000000u)*bound(level,0,10)/400;
        if(x>104)cv_line(x-1,last,x,y,RGB(218,163,96));
        last=y;
      }
    }else{
      int last=137;
      for(int x=0;x<216;x++){
        unsigned ph=(unsigned)x*(43000000u+a*3000000u);
        int y=137-(sinus(ph)+sinus(ph+fm1_ms*(unsigned)(10000+a*30000)))*(b+2)/420;
        if(kind==DUSK)y=138-absolute(sinus(ph+(unsigned)a*93000000u))*(4+b)/160;
        if(kind==TIDAL)y=136-(sinus(ph)*b+sinus(ph*17u+fm1_ms*1900000u)*a)/300;
        if(x)cv_line(11+x,last,12+x,y,gold);
        last=y;
      }
    }
    return;
  }
  lettering(120,6,kind>=BEATING?drones[kind-BEATING]:kind>=ORGAN?riley_voices[kind-ORGAN]:ambient_voices[kind-PIANO],gold,0);
  if(kind==ORGAN || kind==REED_ORGAN){
    photo_region(4,0,31,240,82);
    for(int i=0;i<2;i++){
      int x=68+104*i,y=37+7*(i?b:a);
      cv_rect(x-14,34,28,74,RGB(48,34,19));cv_rect(x-6,34,12,74,ink);
      cv_rect(x-16,y+2,32,14,metal);cv_rect(x-15,y,30,12,RGB(48,44,34));cv_rect(x-2,y,3,12,gold);
    }
  }else if(kind==HARMONIUM || kind==REED){
    for(int i=0;i<12;i++){int x=24+i*(9+a)+sinus(fm1_ms*600000u)*(strings[0].active?2:0)*i/1536;cv_line(x,92,x+4,46-b,silver);cv_line(x+4,46-b,x+8,92,gold);}
  }else if(kind==PIANO || kind==BRIGHT || kind==RHODES){
    for(int i=0;i<7;i++){int x=30+i*27;cv_line(x,94,x+15,52-a,silver);dot_at(x+15,49-a,4+b/3,gold);cv_line(x,38,x+20,38+i,metal);}
  }else{
    int last=78;
    for(int x=0;x<197;x++){unsigned ph=(unsigned)x*60000000u;if(kind>=BEATING)ph+=fm1_ms*80000u;int y=78-(sinus(ph)*5+sinus(ph*2u)*(a-4)+sinus(kind>=BEATING?ph+fm1_ms*(unsigned)(a+1)*17000u:ph*3u)*b/2)/32;
      if(x)cv_line(21+x,last,22+x,y,silver);last=y;}
  }
  lettering(70,119,timbre_labels[kind-PIANO][0],silver,0);lettering(172,119,timbre_labels[kind-PIANO][1],silver,0);
}

static void motion_view(void){
  int kind=voice_kind(),rate=knob[MOTION][0],depth=knob[MOTION][1];
  lettering(120,8,motion_titles[kind-PIANO],gold,0);
  for(int x=0,last=81;x<196;x++){
    unsigned phase=(unsigned)x*(21000000u<<rate);
    int y=81-sinus(phase)*depth/(kind<=BRIGHT?28+x/3:28);
    if(x)cv_line(21+x,last,22+x,y,gold);
    last=y;
  }
  lettering(120,120,kind<=BRIGHT?"BODY RESPONSE":"MODULATION",silver,0);
}

#ifdef UI_PROP
static void air_display(void){
  static int needle;
  lettering(120,2,"FIELD RECEIVER",gold,0);
  uint16_t face=RGB(214,190,138), print=RGB(70,52,30);
  cv_rect(16,24,208,74,face);cv_rect(16,24,208,1,RGB(240,226,190));cv_rect(16,97,208,1,RGB(150,128,88));
  int target=air_level*256+(rnd()&255)*air_noise/2;     /* ballistics: quick rise, slow fall */
  needle+=(target-needle)/(target>needle?3:9);
  for(int i=0;i<=20;i++){                               /* arc scale, -45..+45 degrees */
    unsigned a=0xE0000000u+(unsigned)i*0x03333333u;
    int s=sinus(a),c=sinus(a+0x40000000u),r0=i%5?58:52;
    cv_line(120+s*r0/127,96-c*r0/127,120+s*64/127,96-c*64/127,i>=16?RGB(170,50,30):print);
  }
  { unsigned a=0xE0000000u+(unsigned)(bound(needle/256,0,255))*0x00404040u;
    int s=sinus(a),c=sinus(a+0x40000000u);
    cv_line(120,96,120+s*66/127,96-c*66/127,RGB(150,40,24)); }
  cv_rect(16,88,208,10,RGB(58,44,30));cv_rect(16,88,208,1,RGB(120,96,64));
  lettering(40,26,"RF",print,0);
  for(int ch=1;ch<=13;ch++){                            /* activity per channel, the scanner's place */
    if(air_heat[ch]>0)air_heat[ch]-=1;
    int h=bound(air_heat[ch]*16/60,0,16),x=22+15*(ch-1);
    cv_rect(x,117-h,11,h,ch==(int)air_channel?gold:metal);
    cv_rect(x,118,11,1,RGB(40,42,38));
    if(ch==(int)air_channel)cv_rect(x+3,120,5,2,gold);
  }
  char status[20];
  if(air_started<0)lettering(120,126,"NO RF CALIBRATION",silver,0);
  else if(air_pick>=0 && air_pick<(int)air_count){
    AirStation *s=&air_stations[air_pick];
    lettering(90,126,s->name[0]?s->name:"HIDDEN",silver,0);
    if(fm1_ms-s->last>3000)lettering(190,126,"LOST",silver,0);
    else {status[0]='-';number(status+1,-s->dbm);lettering(190,126,status,gold,0);}
  }else lettering(120,126,air_frozen?"FROZEN":"SCANNING",silver,0);
}
#else
static void air_display(void){
    lettering(120,2,"FIELD RECEIVER",gold,0);
    cv_rect(18,28,204,50,RGB(208,183,131));
    for(int i=0;i<=10;i++)cv_line(27+18*i,36,27+18*i,36+(i%5?4:9),ink);
    cv_line(120,76,28+air_level*180/255,38,RGB(133,46,29));
    char status[20];
    if(air_started<0)lettering(120,82,"NO RF CALIBRATION",silver,0);
    else if(air_pick>=0 && air_pick<(int)air_count){
        AirStation *s=&air_stations[air_pick];
        lettering(93,82,s->name[0]?s->name:"HIDDEN",silver,0);
        if(fm1_ms-s->last>3000)lettering(183,82,"LOST",silver,0);
        else {status[0]='-';number(status+1,-s->dbm);lettering(183,82,status,gold,0); }
    }else lettering(120,82,"SCANNING",silver,0);
#if defined(GRANA_WEB) || defined(DINGHY_HOST)
    lettering(120,108,air_frozen?"DEMO / FROZEN":"DEMO / SIMULATED RF",gold,0);
#else
    lettering(120,108,air_frozen?"FROZEN":air_locked?"TUNED":"AUTO SCAN",gold,0);
#endif
}
#endif
