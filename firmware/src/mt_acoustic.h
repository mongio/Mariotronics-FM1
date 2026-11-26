/* Fractional string delays are excited by the rendered hammer transient.
 * Two slightly detuned courses exchange energy at the bridge. */
typedef struct {
    int16_t course[2048], decoded[65];
    unsigned cursor, age;
    int low[2];
} PianoStrings;
static PianoStrings piano_strings[4] POOLMEM;
static int pedal;
static int piano_bridge(String *s,int hammer) {
    PianoStrings *p=&piano_strings[s-strings];
    int sum=0, loss=s->gate || pedal ? 15 : 410+180*knob[TIMBRE][2];
    for(int i=0;i<2;i++){
        int16_t *line=i?p->course:s->line;
        unsigned lag=bound(s->delay+(i?s->delay/520:0)-128,256,2046*256);
        unsigned pos=p->cursor-(lag>>8);
        int a=line[pos&2047], b=line[(pos-1)&2047];
        int wave=a+((b-a)*(int)(lag&255)>>8);
        int reflected=(wave*3+p->low[i])/4;
        p->low[i]=wave;
        int coupled=(p->low[1-i]-reflected)/96;
        int excitation=p->age<768 ? hammer*(768-(int)p->age)/1536 : 0;
        line[p->cursor&2047]=bound(reflected*(32768-loss)/32768+coupled+excitation,-30000,30000);
        sum+=wave;
    }
    p->cursor++;if(p->age<768)p->age++;
    return sum/4;
}
/* A common bridge excites short, lossy soundboard modes. With the sustain
 * pedal down, the extended feedback also models the open string bed. */
static int board_tick(int excitation,int resonance){
    static unsigned remaining;
    if(excitation)remaining=88200;
    else if(!remaining)return 0;
    else remaining--;
    static int16_t board[3][512];
    static unsigned index;
    static const unsigned periods[]={409,217,131};
    int wet=0;
    for(int i=0;i<3;i++){
        unsigned tap=(index-periods[i])&511;
        int y=(board[i][tap]*3+board[i][(tap-1)&511])/4;
        board[i][index&511]=bound(excitation/9+y*(209+2*resonance+12*pedal)/256,-20000,20000);
        wet+=y;
    }
    /* Unstruck open courses respond selectively to matching harmonics. */
    static int16_t open_strings[4][512] POOLMEM;
    static const unsigned open_period[]={401,301,200,134};
    for(int i=0;i<4;i++){
        unsigned tap=(index-open_period[i])&511;
        int ring=(3*open_strings[i][tap]+open_strings[i][(tap-1)&511])/4;
        open_strings[i][index&511]=pedal?bound(excitation/30+ring*253/256,-14000,14000):ring*3/4;
        wet+=ring/2;
    }
    index++;
    return wet*(resonance+1)/24;
}
static int pad_tick(String *s){
    if(!(s->pitch_clock++&15))update_pitch(s);
    int a=knob[TIMBRE][0],b=knob[TIMBRE][1],c=knob[TIMBRE][2],d=knob[TIMBRE][3];
    s->phase+=s->increment;
    s->motion_phase+=2300u+(s->kind==TIDAL?d:c)*4200u;
    unsigned detune=s->kind==EVENING?(a+1)*16000u:35000u;
    s->sample_wave+=s->increment+detune+sinus(s->motion_phase)*c*36;
    int main=sinus(s->phase), twin=sinus(s->sample_wave), wave;
    int air=rnd()/256;
    if(s->kind==EVENING){
        int harmonic=s->increment<0x14000000u?sinus(s->sample_wave*3u):0;
        wave=main*22+twin*(11+d)+harmonic*b;
    }else if(s->kind==DUSK){
        unsigned formant=2+(unsigned)a/2;
        int drift=sinus(s->motion_phase)*c/16;
        int vowel=s->increment<0x40000000u/(formant+1)?
            (sinus(s->phase*formant)*(128+drift)+sinus(s->phase*(formant+1))*(128-drift))/128:0;
        wave=main*22+twin*9+vowel*(2+b)+air*d/2;
    }else{
        s->neck+=(air*32-s->neck)/(12-b);
        s->bloom=bound(s->bloom+1,0,65536);
        int swell=bound(s->bloom/(8+16*c),0,256);
        wave=(main*(12-b/2)+twin*(14-a)+s->neck*a/10)*swell/256;
    }
    return wave*s->strength/4096;
}
