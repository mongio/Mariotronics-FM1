/* Three RBJ equalizer sections followed by a resonant two-pole filter. */
#include "mt_tone.h"
typedef struct { float b0,b1,b2,a1,a2; } ToneCoefficients;
typedef struct { int b0,b1,b2,a1,a2,x1,x2,y1,y2; } ToneSection;
typedef struct { int low, mid, frequency, high, mode, cutoff, resonance, mix; } ToneControls;

static ToneControls tone_controls(void) {
  return (ToneControls){knob[EQ][0],knob[EQ][1],knob[EQ][2],knob[EQ][3],
                        knob[FILTER][0]%3,knob[FILTER][1],knob[FILTER][2],knob[FILTER][3]};   /* 1.20: Type 3-5 = POST */
}
static ToneCoefficients tone_coeff(ToneControls t,int i) {
  float a0,a1,a2,b0,b1,b2;
  if(i==3) {
    float c=cut_cos[t.cutoff], alpha=cut_sin[t.cutoff]/(2*(0.7071f+0.043f*t.resonance));
    b0=b2=t.mode==2?(1+c)/2:(1-c)/2;
    b1=t.mode==2?-2*b0:2*b0;
    a0=1+alpha;a1=-2*c;a2=1-alpha;
  } else {
    int gain=i==0?t.low:i==1?t.mid:t.high;
    int f=i==0?0:i==1?t.frequency+2:1;
    float a=eq_gain[gain], c=band_cos[f], sn=band_sin[f];
    if(i==1) {
      float alpha=sn/2;
      b0=1+alpha*a;b1=a1=-2*c;b2=1-alpha*a;a0=1+alpha/a;a2=1-alpha/a;
    } else {
      int sign=i==0?1:-1;
      float p=a+1,m=a-1,beta=1.41421356f*eq_sqrt_gain[gain]*sn;
      b0=a*(p-sign*m*c+beta);b1=2*a*(sign*m-p*c);b2=a*(p-sign*m*c-beta);
      a0=p+sign*m*c+beta;a1=-2*(sign*m+p*c);a2=p+sign*m*c-beta;
    }
  }
  return (ToneCoefficients){b0/a0,b1/a0,b2/a0,a1/a0,a2/a0};
}
static ToneSection tone_sections[4];
static unsigned tone_active;
static void tone_prepare(ToneControls t) {
  static ToneControls prev;
  static int ready;
  if(ready && t.low==prev.low && t.mid==prev.mid && t.high==prev.high &&
     t.frequency==prev.frequency && t.mode==prev.mode && t.cutoff==prev.cutoff &&
     t.resonance==prev.resonance && t.mix==prev.mix) return;
  ready=1;prev=t;
  unsigned mask=(t.low!=12)|((t.mid!=12)<<1)|((t.high!=12)<<2)|((!!t.mode && !!t.mix)<<3);
  for(int i=0;i<4;i++) {
    ToneSection *s=&tone_sections[i];
    if(!(mask&(1u<<i))) { s->x1=s->x2=s->y1=s->y2=0;continue; }
    ToneCoefficients c=tone_coeff(t,i);
    s->b0=c.b0*16777216;s->b1=c.b1*16777216;s->b2=c.b2*16777216;
    s->a1=c.a1*16777216;s->a2=c.a2*16777216;
  }
  tone_active=mask;
}
static AUDIO_FAST int tone_process(int x,int mix) {
  if(!tone_active) return x;
  for(int i=0;i<4;i++) if(tone_active&(1u<<i)) {
    ToneSection *s=&tone_sections[i];
    int64_t sum=(int64_t)s->b0*x+(int64_t)s->b1*s->x1+(int64_t)s->b2*s->x2
                -(int64_t)s->a1*s->y1-(int64_t)s->a2*s->y2;
    int y=bound((int)((sum+8388608)>>24),-524288,524287);
    s->x2=s->x1;s->x1=x;s->y2=s->y1;s->y1=y;
    x=i==3?x+(y-x)*mix/100:y;
  }
  return bound(x,-32768,32767);
}
static float tone_power(ToneCoefficients c,int j,float wet) {
  float co=plot_cos[j],si=plot_sin[j],co2=2*co*co-1,si2=2*si*co;
  float dr=1+c.a1*co+c.a2*co2, di=c.a1*si+c.a2*si2;
  float nr=(1-wet)*dr+wet*(c.b0+c.b1*co+c.b2*co2);
  float ni=(1-wet)*di+wet*(c.b1*si+c.b2*si2);
  return (nr*nr+ni*ni)/(dr*dr+di*di);
}
