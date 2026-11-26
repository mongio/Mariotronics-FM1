/* Damped rotating modes. An impulse adds velocity to each mode;
   the displacement (imaginary component) is the acoustic output. */
enum { MODE_COUNT = 6 };
typedef struct { float x, y, cosine, sine, gain; } AirMode;
typedef struct { AirMode mode[MODE_COUNT]; unsigned next_hit, tuned_pitch; int material, ring; } AirBody;
static AirBody air_bodies[4];

/* Fold into +/- pi/2 before evaluating the odd sine polynomial. */
static float modal_sine(unsigned phase) {
    unsigned quarter = phase & 0x3fffffffu;
    if (phase & 0x40000000u) quarter = 0x40000000u - quarter;
    float x = (float)quarter * (1.5707963268f / 1073741824.0f);
    float x2 = x*x;
    float y = x*(1.0f + x2*(-0.16666667f + x2*(0.008333333f +
              x2*(-0.0001984127f + x2*0.00000275573f))));
    return phase & 0x80000000u ? -y : y;
}

static void tune_body(AirBody *body, String *s) {
    static const float plate[] = {1, 1.59f, 2.34f, 2.91f, 4.13f, 5.56f};
    if (body->tuned_pitch == s->increment && body->material == knob[TIMBRE][2] &&
        body->ring == knob[TIMBRE][3]) return;
    body->tuned_pitch = s->increment;
    body->material = knob[TIMBRE][2];
    body->ring = knob[TIMBRE][3];
    float material = knob[TIMBRE][2] / 8.0f;
    int ring = knob[TIMBRE][3];
    for (int i = 0; i < MODE_COUNT; ++i) {
        AirMode *m = &body->mode[i];
        float harmonic = i + 1;
        float ratio = harmonic + material * (plate[i] - harmonic);
        float freq = (float)s->increment * ratio;
        if (freq >= 2000000000.0f) {
            m->gain = m->x = m->y = m->cosine = m->sine = 0;
            continue;
        }
        unsigned ph = (unsigned)freq;
        float a = modal_sine(ph + 0x40000000u), b = modal_sine(ph);
        float norm = a*a + b*b;
        float scale = (3.0f - norm) * 0.5f;
        scale *= 1.5f - 0.5f * norm * scale * scale;
        /* Amplitude e-fold time varies by material, mode and ring setting. */
        scale *= 1.0f - (1.0f + i*(0.38f - material*0.23f))/(2400.0f + 1000.0f*ring*ring);
        m->cosine = a * scale;
        m->sine = b * scale;
        m->gain = 1.0f / (1.0f + i * (0.85f - material * 0.45f));
    }
}

static int air_modal_tick(String *s) {
    AirBody *body = &air_bodies[s-strings];
    unsigned clock = s->pitch_clock++;
    if (!(clock & 15)) update_pitch(s);
    if (!(clock & 63)) tune_body(body,s);
    float force = 0;
    if (s->gate) {
        int noise = rnd();
        if (!body->next_hit) {
            force = clock ? (900 + air_level*8 + air_noise*5) : 3800;
            body->next_hit = (unsigned)(18000 + (noise+32768)/2)/(1u+(unsigned)air_noise/4u);
        }
        --body->next_hit;
        force += noise * (0.0005f + air_noise * (32 + air_level) * 0.00000009f);
    }
    float out = 0;
    for (int i=0; i<MODE_COUNT; ++i) {
        AirMode *m=&body->mode[i];
        float x=m->x + force*m->gain, y=m->y;
        m->x = x*m->cosine - y*m->sine;
        m->y = x*m->sine + y*m->cosine;
        out += m->y;
    }
    /* Soft compression on dense traffic before mixing polyphonic voices. */
    float mag = out<0 ? -out : out;
    return (int)(out * (s->strength/6400.0f) / (1.0f + mag/8500.0f));
}
