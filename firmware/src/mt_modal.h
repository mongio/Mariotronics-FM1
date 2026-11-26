/* Damped rotating modes. An impulse adds velocity to each mode;
   the displacement (imaginary component) is the acoustic output.

   Fixed point throughout: the FM-1's pi32v2 has no FPU in this build, so every
   float operation was a library call (about 60 per voice per sample) and four
   held notes overran the audio deadline - heard as crackle and distortion.
   Coefficients are Q30, states carry 6 extra fraction bits, and each rotation
   is two 32x32->64 multiply-accumulates.

   Excitation is limited by the energy already in the body, so dense Wi-Fi
   traffic or a long Ring cannot pile the modes up into the output stage. */
enum { MODE_COUNT = 6, MODE_FRAC = 6, MODE_LIMIT = 12000 };
typedef long long modal_i64;
typedef struct { int x, y, cosine, sine, gain; } AirMode;
typedef struct { AirMode mode[MODE_COUNT]; unsigned next_hit, tuned_pitch; int material, ring, room, gain_sum; } AirBody;
static AirBody air_bodies[4];

static inline __attribute__((always_inline)) int modal_mul(int a, int b) { return (int)(((modal_i64)a * b + (1 << 29)) >> 30); }

/* sin(phase) in Q30, phase as a full 32-bit turn. Fold into +/- pi/2 and
   evaluate the odd Taylor polynomial (error below 1e-6). */
static AUDIO_FAST int modal_sine(unsigned phase) {
    unsigned quarter = phase & 0x3fffffffu;
    if (phase & 0x40000000u) quarter = 0x40000000u - quarter;
    int x = (int)(((modal_i64)quarter * 1686629713) >> 30);   /* quarter turn -> radians, Q30 */
    int x2 = (int)(((modal_i64)x * x + (1 << 31)) >> 32);         /* Q28: (pi/2)^2 does not fit Q30 */
    static const int taylor[] = { -27, 2959, -213045, 8947849, -178956971 };   /* -1/11!, 1/9!, -1/7!, 1/5!, -1/3! in Q30 */
    int y = 0;
    for (int i = 0; i < 5; i++) y = taylor[i] + (int)(((modal_i64)x2 * y + (1 << 27)) >> 28);
    y = x + modal_mul(x, (int)(((modal_i64)x2 * y + (1 << 27)) >> 28));
    return phase & 0x80000000u ? -y : y;
}

static AUDIO_FAST void tune_body(AirBody *body, String *s) {
    static const int plate16[] = { 65536, 104202, 153354, 190710, 270664, 364380 }; /* 1, 1.59, 2.34, 2.91, 4.13, 5.56 */
    int material = rf_timbre(knob[TIMBRE][2]), ring = knob[TIMBRE][3];
    if (body->tuned_pitch == s->increment && body->material == material && body->ring == ring) return;
    body->tuned_pitch = s->increment;
    body->material = material;
    body->ring = ring;
    int gain_sum = 0;
    for (int i = 0; i < MODE_COUNT; ++i) {
        AirMode *m = &body->mode[i];
        int harmonic16 = (i + 1) * 65536;
        int ratio16 = harmonic16 + material * (plate16[i] - harmonic16) / 8;
        unsigned long long freq = ((unsigned long long)s->increment * (unsigned)ratio16) >> 16;
        if (freq >= 2000000000ull) {
            m->gain = m->x = m->y = m->cosine = m->sine = 0;
            continue;
        }
        unsigned ph = (unsigned)freq;
        int a = modal_sine(ph + 0x40000000u), b = modal_sine(ph);
        /* Renormalise to unit length (one Newton step for 1/sqrt), then apply the decay. */
        int norm = modal_mul(a, a) + modal_mul(b, b);
        int scale = (int)((3ll * (1 << 30) - norm) / 2);
        /* Amplitude e-fold time varies by material, mode and ring setting. */
        int k16 = 65536 + i * (24904 - 1884 * material);               /* 1 + i*(0.38 - 0.23*material/8) */
        int decay = (1 << 30) - (int)(((unsigned)k16 << 14) / (unsigned)(2400 + 1000 * ring * ring));
        scale = modal_mul(scale, decay);
        m->cosine = modal_mul(a, scale);
        m->sine = modal_mul(b, scale);
        m->gain = (int)(0x80000000u / (unsigned)(65536 + i * (55706 - 3686 * material)));   /* Q15: 1/(1 + i*(0.85 - 0.45*material/8)) */
        gain_sum += m->gain;
    }
    body->gain_sum = gain_sum ? gain_sum : 1;
}

static inline __attribute__((always_inline)) int mode_magnitude(const AirMode *m) {        /* |x + iy| within about 12% */
    int x = m->x < 0 ? -m->x : m->x, y = m->y < 0 ? -m->y : m->y;
    return (x > y ? x + y / 2 : y + x / 2) >> MODE_FRAC;
}

static AUDIO_FAST int air_modal_tick(String *s) {
    AirBody *body = &air_bodies[s-strings];
    unsigned clock = s->pitch_clock++;
    if (!(clock & 15)) {
        update_pitch(s);
        int amplitude = 0;
        for (int i = 0; i < MODE_COUNT; ++i) amplitude += mode_magnitude(&body->mode[i]);
        body->room = MODE_LIMIT - amplitude;            /* how much more the body can take */
    }
    if (!(clock & 63)) tune_body(body, s);
    int force = 0;
    if (s->gate && body->room > 0) {
        int noise = rnd();
        if (!body->next_hit) {
            force = clock ? (900 + air_level * 8 + air_noise * 5) : 3800;
            body->next_hit = (unsigned)(18000 + (noise + 32768) / 2) / (1u + (unsigned)air_noise / 4u);
            /* An impulse raises the summed mode amplitude by force*gain_sum. */
            int cap = (body->room << 15) / body->gain_sum;              /* room <= 12000: fits */
            if (force > cap) force = cap;
        }
        --body->next_hit;
        /* Friction: 0.0005 + traffic*(32+level)*9e-8 per unit of noise, faded as the body fills. */
        int friction = (int)(((modal_i64)noise * (8389 + air_noise * (32 + air_level) * 3 / 2)) >> 24);
        force += friction * body->room / MODE_LIMIT;
    } else if (s->gate && body->next_hit) --body->next_hit;
    int out = 0;
    for (int i = 0; i < MODE_COUNT; ++i) {
        AirMode *m = &body->mode[i];
        int x = m->x, y = m->y;
        if (force) x += (force * m->gain) >> (15 - MODE_FRAC);
        m->x = (int)(((modal_i64)x * m->cosine - (modal_i64)y * m->sine + (1 << 29)) >> 30);
        m->y = (int)(((modal_i64)x * m->sine + (modal_i64)y * m->cosine + (1 << 29)) >> 30);
        out += m->y;
    }
    out >>= MODE_FRAC;
    if (out > 3 * MODE_LIMIT) out = 3 * MODE_LIMIT; else if (out < -3 * MODE_LIMIT) out = -3 * MODE_LIMIT;
    /* Linear output: the old instantaneous compressor intermodulated the inharmonic modes. */
    return out * (s->strength * 9 / 64) / 1000;                   /* strength*9/64000, in 32 bits */
}
