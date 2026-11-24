# Mariotronics for M-VAVE FM-1

Mariotronics turns the FM-1 into a playable guitar and tape-delay instrument inspired by Frippertronics. On power-up, the MTXT logo sits inside the left reel while a yellow cursor makes two passes around the feedback loop. The lowercase italic *mariotronics* title fades in beneath it. Two animated Revox A77s show the tape moving between the record and playback heads.

**1.1 pre-release · Tested in emulation.** Installation, audio performance and BLE pairing on a physical FM-1 await verification.

## Try it

[Download the standalone editor and emulator (ZIP)](https://raw.githubusercontent.com/mongio/Mariotronics-FM1/main/downloads/Mariotronics_Editor_FM1.zip). Extract the ZIP, open **Mariotronics_1_1.html** in desktop Chrome or Edge and click **Power on**. The emulator works offline. Hold REC for 1.2 seconds to erase the tape. Powering off clears the tape in RAM.

## Install on FM-1

Connect one powered FM-1 with a USB data cable. Open the downloaded editor, select **Install Mariotronics**, and grant MIDI SysEx access. Installation replaces the existing firmware. Leave power and USB connected until the editor reports **Installed · FM-1_851**.

[Download the firmware package](https://github.com/mongio/Mariotronics-FM1/raw/refs/heads/main/downloads/mariotronics-1.1.fwsc).

## Play

- **EDIT** opens Instrument and selects the voice for the current algorithm.
- **LFO** sets vibrato rate, depth, delay and rise. **ARP** alternates two held notes with a 3+3+2 accent.
- **REC** records and overdubs. **SEQ** toggles reverse on a recorded loop. **REC** holds or resumes overdubbing on a recorded loop. HOME K1 sets Tape Delay. ALGORITHM selects FRIPP or ENO.
- **FX** opens compression, fuzz, wah, amplifier and stereo plate. The plate follows the tape return, so its tail stays out of the recording.
- **GLO** opens MIDI and Bluetooth MIDI. The editor includes the CC assignments and transport instructions.

[Build instructions](MARIOTRONICS.md) · [License](LICENSE) · [Third-party credits](LICENSING.md)

**Off-Tape Monitor.** Press FX until OFF-TAPE appears. K1 sets the level (0–100%, default 25%). In REC and DUB, the first head reads the newly written tape after 180 ms. Its level affects the monitor mix feeding the final plate.

**Panel lights.** GLO cycles through MIDI, BLE MIDI, LIGHTS and TAPE CHARACTER. On LIGHTS, K1 selects OFF, HALF or FULL idle key backlight (default FULL). A held key lights at peak brightness. REC flashes while recording or overdubbing. PLAY stays lit while the tape moves.

**Presets.** Turn PRESETS to recall P01–P64. Each group of eight combines Singing, Hollow, Velvet, Glass, Fuzz, Reed, Copper and Wire with a different tape and articulation setup: Tape, Swell, Dusk, Echo, Bloom, Pulse, Mist or Drift. Press SAVE to open SAVE. K1 moves the cursor, K2 edits the character, K3 generates a tape-inspired name, and K4 left erases a character. K4 right selects Delete Preset. Turn PRESETS to choose the destination. Press SAVE to commit or HOME to cancel. Tap SEQ to toggle forward/reverse playback and show the decks. Tap REC during overdub for HOLD. MIDI Program Change 0–63 recalls the same slots. Presets store instrument voice, articulation, scale, effects, tape controls and arpeggiator settings. Recorded audio and global controls are separate. The FM-1 stores edits in flash and the editor stores its own bank in the browser. Startup recalls P01.

**Tape Character.** From HOME, press GLO four times. K1 HF LOSS darkens each feedback pass, K2 SAT softens record peaks, K3 WOW and K4 FLUTTER modulate long-head playback (including HOLD and reverse). Global settings are saved after adjustment. The defaults 32/0/0/0 reproduce the previous tape response.

**Algorithms.** ALGORITHM switches live between FRIPP (guitar, Mk III decks) and ENO (Piano, Bright Piano, Glass Bell, Tape Choir and vintage decks). In ENO press ARP for Note FX: direction, tone, damping and tremolo. Press ARP again for Head 2. The optional second reading runs at the original pitch over 73.1% of the tape length and defaults to OFF. Both engines share the recorded audio and transport. HOME K1 sets internal tape delay from 4.0 to 8.0 seconds. ENV K1 sets attack. Algorithm and internal delay are saved per preset. External clock and manually closed LOOP recordings retain their own timing. Each engine remembers its live edits in RAM until another preset is recalled or power is switched off. SAVE writes the active engine to the selected preset. In HOLD, play the other engine over the existing tape. In overdub, its new notes join the recording.

**Pre-tape tone controls.** FX cycles through Comp, Fuzz, Wah, Amp, EQ, Filter, Off-Tape and Plate. EQ has low and high shelves (180 Hz and 4.2 kHz) and a sweepable mid band (250 Hz to 4 kHz), all ±12 dB. Filter offers Off, LP and HP, cutoff 40 Hz to 16 kHz, resonance and mix. Neutral by default, recalled per preset and per live engine.
