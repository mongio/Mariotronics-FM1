# Mario Texture · Mariotronics for FM-1

A 44.1 kHz stereo guitar and tape instrument. Tape is stored at 14.7 kHz in RAM. The two guitar voices share the ADSR, delayed vibrato and pre-tape effects. The stereo plate receives the live instrument and the tape return after recording. Tap REC to record, overdub or hold. Hold REC for 1.2 seconds to erase.

## Panel

EDIT opens Instrument. LFO adjusts rate, depth, wait and rise. Press LFO again on its page to switch vibrato. SEL cycles through Phrasing and Scales. GLO cycles through MIDI and BLE MIDI. On hardware, enable the radio with K1 and confirm the K2 device row with OCT+. Row zero starts a scan. Select a system MIDI input in the browser emulator.

## Build

On Linux x86-64, use the JieLi toolchain and AC79 SDK for the firmware, and clang with wasm-ld for the emulator. Set JIELI_TOOLCHAIN and AC79_SDK before building.

```sh
./build.sh --release 1.7
clang --target=wasm32 -O2 -ffreestanding -nostdlib -fno-builtin -DGRANA_WEB -Ifirmware/src -Ifirmware/gen -Wl,--no-entry -Wl,--export-memory -Wl,-z,stack-size=262144 -Wl,--initial-memory=16777216 -o web/mariotronics.wasm firmware/src/mariotronics.c
python3 web/make_mariotronics.py
```

The FM-1 platform, USB/TRS MIDI and BLE central derive from VIRGO, GRANA, Felucca and Optimist. The guitar excitation and pickup derive from De-Pattern/Ravel_FM1 GUITAR. The Revox A77 display and MTXTronics intro use supplied reference artwork. See LICENSE, LICENSING.md and individual files for credits.

**Off-Tape Monitor.** Press FX until OFF-TAPE appears. K1 sets the level (0–100%, default 25%). In REC and DUB, the first head reads the newly written tape after 180 ms. Its level affects the monitor mix feeding the final plate.

**Panel lights.** GLO cycles through TAPE CHARACTER, TAPE SPEED, MIDI, BLE MIDI and LIGHTS. On LIGHTS, K1 selects OFF, HALF or FULL idle key backlight (default FULL). A held key lights at peak brightness. REC flashes while recording or overdubbing. PLAY stays lit while the tape moves.

**Presets.** Turn PRESETS to recall P01–P64. Each group of eight combines Singing, Hollow, Velvet, Glass, Fuzz, Reed, Copper and Wire with a different tape and articulation setup: Tape, Swell, Dusk, Echo, Bloom, Pulse, Mist or Drift. Press SAVE to open SAVE. K1 moves the cursor, K2 edits the character, K3 generates a tape-inspired name, and K4 left erases a character. K4 right selects Delete Preset. Turn PRESETS to choose the destination. Press SAVE to commit or HOME to cancel. Tap SEQ to toggle forward/reverse playback and show the decks. Tap REC during overdub for HOLD. MIDI Program Change 0–63 recalls the same slots. Presets store instrument voice, articulation, scale, effects, tape controls and arpeggiator settings. Recorded audio and global controls are separate. The FM-1 stores edits in flash and the editor stores its own bank in the browser. Startup recalls P01.

**Tape Character.** From HOME, press GLO once. K1 HF LOSS darkens each feedback pass, K2 SAT softens record peaks, K3 WOW and K4 FLUTTER modulate long-head playback (including HOLD and reverse). Global settings are saved after adjustment. The defaults 32/0/0/0 reproduce the previous tape response.

Press SEL twice for a six-string fretboard in standard tuning. K1 selects the scale and K2 its root. Gold dots mark the tonic at every octave. Press EDIT repeatedly to move from the voice scope to the active voice controls. Press EDIT again for soundboard response, shimmer, chorus, tremolo, rotary, breath, bellows or slow fade, depending on the active voice. ENO continues through Note FX (note direction, tone, damping and tremolo) and Second Head. ENO and RILEY ARP K1 selects Off, Climb, Fold, Phase or Orbit. Play a single note for a generated figure or hold a chord to sequence its notes. ENO uses wide intervals and adds alternating step lengths in Phase and Orbit. RILEY uses shorter modal intervals and an even pulse. RADIGUE layers slowly beating oscillators over the same tape. Its four drone voices are Beating, Sub, Veil and Fifth. EDIT controls Beat and Color.

**Pre-tape tone controls.** FX cycles through Comp, Fuzz, Wah, Amp, EQ, Filter, Off-Tape and Plate. EQ has low and high shelves (180 Hz and 4.2 kHz) and a sweepable mid band (250 Hz to 4 kHz), all ±12 dB. Filter offers Off, LP and HP, cutoff 40 Hz to 16 kHz, resonance and mix. Neutral by default, recalled per preset and per live engine.

In FRIPP, press ARP twice to reach ARP PICK. Wait delays the first pick and Rise fades in the phrase (0–1000 ms each). Legato connects each pair with one pick. Semi sets the second note 0–24 semitones above a single held key, snapped to the selected scale. Hold two keys for your own interval. SEL TOUCH shapes manual playing with glide, glide time, pick attack and slide curve. Press SEL again for scales.

EDIT shows a triggered pre-effects voice scope. TAPE CHARACTER shows output VU and peak after plate and master. HOME keeps the tape display. FRIPP ARP opens the interval view; press ARP again for ARP PICK. K1 on the ARP rate page switches picking on or off. The startup animation shows the mariotronics wordmark.

In 1.5, EDIT adds Rhodes and Bowed Glass to ENO, and Reed Organ and Harmonium Drone to RILEY. ENO acoustic and bright pianos use two velocity layers rendered from coupled stiff strings and hammer contact. GLO has cream vintage meter faces. Moving HOME K1 during internal tape playback bends the return. From HOME turn SELECT left once to reach ERASE. K1 ALL keeps the 1.2-second whole-tape erase. For RANGE arm K2, then hold REC over the moving tape and release to disarm.

**Tape Speed.** On the second GLOBAL page, K1 changes the transport speed. K2 selects STEP (4.75, 9.5, 19 and 38 cm/s) or VAR for fine adjustment. At 9.5 cm/s a loop recorded at 19 cm/s plays an octave lower and lasts twice as long. The live instrument keeps its tuning.
