# Mariotronics release notes

## 1.19 (FM-1_878)

The test builds ENO Studio Lab 5–7 lead to this release; the name is no longer used.

* **Four knobs on every page.** SCALES: Transpose, Tune. BLE MIDI: Keys Out, Thru (USB/TRS and Bluetooth forwarded both ways). OFF-TAPE: Tone, Pan, Time (20–180 ms). KEYS (was LIGHTS): Hold Key, Velocity, Hold time. HEAD 2: Level, Length, Reverse. ERASE: Depth, Soft. SPEED: Glide, Fine. TIMBRE: four controls for every voice. MOTION: Shape, Delay.
* **Engines** (FRIPP unchanged; every new control starts off, so existing programs sound as before). ENO Glass, Choir, Rhodes and Bowed Glass: Octave and Sympathetic. RILEY: Percussion (third harmonic and key click) and Leslie (slow chorale 1–4, fast tremolo 5–8). RADIGUE: Sub (an octave below) and Drift (the beating wanders). STOCKHAUSEN: a RING MOD page (EDIT from TIMBRE) with Ring, Ratio, Excite (strike, bow, scrape) and Drift.
* **LFO 2** (LFO, second press): 0.02–20 Hz, seven shapes, eleven targets, signed depth. It adds to the vibrato and to the Wi-Fi routes.
* **ENSEMBLE** (in the FX cycle after FILTER): string ensemble, chorus, phaser and flanger in stereo, before the tape. Wi-Fi and LFO 2 can move its rate and depth.
* **Keyboard HOLD** as in GRANA: a held note pressed again restarts (RESTART, default) or ends (RELEASE). The long press is set on KEYS (0.5–2.0 s, 1.0 s default).
* **Arpeggio** down to one step every 20 seconds (twelve slower rates below 3 Hz).
* **Piano samples** in the editor: twelve slots (six notes, soft and hard), each Factory, Sustain or your own audio file. Clear messages; another firmware's samples are erased only after confirmation. The earlier sustain bank keeps working.
* **Display:** new MIDI and KEYS icons drawn on the screen itself, and views for LFO 2, ENSEMBLE and RING MOD.
* Preset backups are staged in the tape memory while restoring (the loop is erased); backups from Lab 6 and Lab 7 are converted by the editor.

## Earlier test builds (ENO Studio Lab)

### Lab 7 (1.18-eno7, FM-1_877)

* **Display.** Roboto Condensed throughout. Labels are never squeezed: the status row places page, engine, preset and transport by measured width and moves to the small face when needed. The battery sits on a dark tab at the top right of the screen. FIELD RECEIVER has a moving-needle meter with ballistics and a bar for each Wi-Fi channel; the ARP view lights the playing step and lets it fade.
* **ARP FEEL** (ENO, RILEY, RADIGUE: press ARP a second time). Swing from straight (default) to 75 % on the first of each pair; Accent OFF, 3+3+2, 4 or 2; Human loosens timing and touch; Repeat plays each step up to four times. ENO no longer forces the swing of PHASE and ORBIT. FRIPP is unchanged.
* **Wi-Fi modulation.** ROUTING has two routes, each with a target and a signed amount. Targets: the engine's own timbre, Color, Level (down to silence), Pitch (up to a fifth), Filter (opens a low-pass even with FILTER off), Tape (wow and flutter), Plate (send and decay), Arp Rate (up to four times faster or slower) and Density (negative: rests; positive: repeats).
* ARP FEEL and both routes are stored with each program and in preset backups. Programs and backups from earlier versions load with the defaults; the editor pads older backup files.
* The startup artwork is stored with PackBits: 25 KB of flash freed, same picture.

### Lab 6 (1.18-eno6, FM-1_876)

* **Audio from RAM.** The per-sample audio path (voices, effects, tape, plate) and its tables now run from RAM, as in VIRGO. From the flash it shared the XIP cache with the piano recordings, and dense piano polyphony could overrun the audio deadline. At the extremes of the velocity range the piano decodes only the layer it plays. The sound is bit-identical to Lab 5 in FRIPP, ENO, RILEY and RADIGUE.
* **STOCKHAUSEN in fixed point.** The modal resonator used float arithmetic, which the FM-1 build performs in software (about 60 library calls per voice per sample), and four held notes overran the audio deadline, heard as crackle and distortion. It now uses Q30 integer rotations. Excitation is limited by the energy already in the body, so dense Wi-Fi traffic or a long Ring no longer drives the modes into the output stage. The old instantaneous compressor that intermodulated the modes is removed.
* **Keyboard HOLD.** Hold HOME for 1 second. The keys held at that moment and every note played afterwards keep sounding until HOME is held again for 1 second. A key still held at release keeps its note until lifted. Notes are latched by pitch, so octave changes are safe.
* **Arpeggio.** In ENO, RILEY and RADIGUE, adding or lifting a note no longer restarts the pattern. FRIPP is unchanged.

Install the firmware from the offline editor. ENO Piano and Bright play recorded hammer attacks in six registers and two velocity layers. The optional **Install ENO piano sustain bank** button extends those recordings from 1.78 to 4.44 seconds at their original key. Both versions support reverse, the sustain pedal and the existing envelope, soundboard and damper controls. The simulator loads the extended version automatically.

The extension occupies 279,316 bytes of the shared 276 KiB sample area. Installation runs separately after the firmware update. An existing GRANA or VIRGO bank is preserved and ENO uses the internal recordings. An interrupted transfer can be repeated. The complete bank is checked before playback.

Piano audio adapted from **Salamander Grand Piano v3** by Alexander Holm, CC BY 3.0. Source: https://github.com/sfzinstruments/SalamanderGrandPiano/tree/3382bf9496bba2486f5ab0de55a264d1dfc38404 . Twelve recordings were trimmed, mixed to mono, resampled to 16 kHz and block-encoded for the FM-1. The license is included in assets/eno/LICENSE in the source archive.
