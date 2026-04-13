# Komyo

[光明] A platform-independent subtractive vocal synthesis engine for Japanese chant-style vocals.

# Demo
WASM Runtime on WebApp
[KoboTron](https://hugelton.com/apps/KoboTron/)

## Features

- **Rich Formant Synthesis**: 5-formant architecture (F1-F5) for rich vocal timbre
- **Equal Loudness**: All vowels have consistent volume across the vocal range (C2-C4)
- **11 Formant Presets**: Japanese vowels (A, I, U, E, O) plus special phonemes (M, N, AE, Y, R, W)
- **Waveform Selection**: 3 glottal source waveforms (saw^2, saw^3, raw saw)
- **Pitch Bend**: MIDI-standard pitch bend (±2 semitones)
- **Fricative Noise**: Add noise for consonants (s, sh, h, etc.)
- **Real-time Parameter Control**: Vibrato, portamento, pitch offset, formant offset, Q parameter
- **Note-based Interface**: MIDI-compatible `noteOn()`/`noteOff()` API
- **Header-only Library**: No compilation required, just include `komyo.h`
- **Cross-platform**: Works with any C++17 compiler (configurable sample rate)
- **Microcontroller Ready**: KOMYO_LIGHT_MODE with LUT optimization for RP2040/ESP32
- **Click-free Envelope**: 10ms fade in/out prevents clicks and pops at note on/off
- **DC Blocking**: AC coupling removes DC offset for clean output
- **Soft Clipping**: Adjustable saturation for warm distortion

## Quick Start

```cpp
#include "komyo.h"

using namespace Komyo;

// Create instance with sample rate (default 48000Hz)
Komyo chanter(48000.0f);

// Set parameters
chanter.setVowel(0);           // 0=A, 1=I, 2=U, 3=E, 4=O, 5=M, 6=N, 7=AE, 8=Y, 9=R, 10=W
chanter.setVibratoDepth(50.0f); // cents
chanter.setVibratoSpeed(5.5f);  // Hz
chanter.setMasterVolume(0.5f);

// In your audio callback:
void audioCallback(float* output, int numSamples) {
    for (int i = 0; i < numSamples; i++) {
        output[i] = chanter.process();  // Generate one sample
    }
}

// Trigger notes:
chanter.noteOn(60.0f);  // MIDI note 60 (C4)
chanter.noteOff();
```

## Advanced Examples

### Pitch Bending
```cpp
chanter.setPitchBend(0.5f);   // Bend up 1 semitone
chanter.setPitchBend(-0.5f);  // Bend down 1 semitone
```

### Waveform Selection
```cpp
chanter.setWaveformType(0);  // Squared sawtooth (default, balanced)
chanter.setWaveformType(1);  // Cubic (soft, mellow)
chanter.setWaveformType(2);  // Raw sawtooth (bright, harsh)
```

### Consonant Synthesis
```cpp
// Fricative consonants (s, sh, h)
chanter.setNoiseGain(0.2f);   // Add breath noise
chanter.setVowel(1);          // Target vowel
// ... after consonant
chanter.setNoiseGain(0.0f);   // Disable noise
```

### Buddhist Chanting
```cpp
chanter.setWaveformType(1);      // Soft waveform
chanter.setVibratoDepth(30.0f);  // Deep vibrato
chanter.setVibratoSpeed(4.5f);   // Slow, meditative
chanter.setVowel(2);            // U (deep vowel)
chanter.noteOn(43.0f);          // G2 - very low
```

### Dynamic Sample Rate Change
```cpp
// When DAW sample rate changes
chanter.setSampleRate(44100.0f);  // Change to 44.1kHz
chanter.setSampleRate(48000.0f);  // Change back to 48kHz
```

## Version History

### v2.1 (2026-04-12)
- Extended formant architecture (F4, F5) for richer timbre
- Pitch bend support (±2 semitones) with `setPitchBend()`
- Waveform type selection (saw^2, saw^3, raw saw) with `setWaveformType()`
- Fricative noise generation with `setNoiseGain()` for consonants
- Improved consonant-vowel coarticulation
- Buddhist chant and mantra synthesis capabilities

### v2.0 (2026-04-09)
- Equal loudness across vowels with normalization gains
- Fixed bandwidth filters (80Hz) for natural formant response
- Improved glottal source waveform (squared sawtooth)
- KOMYO_LIGHT_MODE for microcontrollers (RP2040/ESP32)
- Configurable sample rate via constructor
- Soft clipping saturation with `setDriveAmount()`
- DC blocking filter and fade envelope
- Performance optimizations

### v1.2
- DSP optimization for microcontrollers
- Adjustable drive amount

### v1.1
- DC blocking filter
- Fade envelope for click-free transitions

### v1.0
- Initial release

## API Reference

### Initialization
```cpp
Komyo(float sampleRate = 48000.0f);  // Constructor with sample rate (default 48kHz)

// Change sample rate at runtime
void setSampleRate(float newSampleRate);  // Update sample rate dynamically
```

### Vowel Selection
```cpp
void setVowel(int index);  // 0-10
```
- 0: A (Ah) - ア
- 1: I (Ee) - イ
- 2: U (Oo) - ウ
- 3: E (Eh) - エ
- 4: O (Oh) - オ
- 5: M (Mm) - 閉唇
- 6: N (Nn) - ン
- 7: AE (Ash) - æ
- 8: Y (Yue) - y
- 9: R (Ruh) - r
- 10: W (Wu) - w

### Formant Parameters
```cpp
void setPortamento(float speed);        // Formant transition speed (0-1)
void setFormantOffset(float st);        // Formant pitch shift in semitones (-12 to +12)
```

### Pitch Parameters
```cpp
void setPitchOffset(float st);          // Pitch offset in semitones (-12 to +12)
void setPitchPortamento(float speed);   // Pitch glide speed (0-1)
```

### Vibrato Parameters
```cpp
void setVibratoDepth(float cents);      // Vibrato depth in cents (0-200)
void setVibratoSpeed(float hz);         // Vibrato speed in Hz (1-15)
void setVibratoDelay(float ms);         // Vibrato onset delay in ms (0-1000)
```

### Master Parameters
```cpp
void setMasterVolume(float amp);        // Output level (0.0-1.0)
void setQ(float q);                     // Formant filter resonance (1-50)
void setBaseFreq(float freq);           // Base frequency in Hz
void setDrive(float drive);             // Adjustment drive amount for Clipping
```

### v2.1 New Features
```cpp
void setPitchBend(float bend);          // Pitch bend (±1.0 = ±2 semitones)
void setWaveformType(int type);         // 0=saw^2, 1=saw^3 (soft), 2=raw saw (bright)
void setNoiseGain(float gain);          // Fricative noise amount (0.0-1.0)
```

### Note Input
```cpp
void noteOn(float midiNote);            // Trigger note (MIDI note number)
void noteOff();                         // Stop note
```

### Audio Processing
```cpp
float process();                        // Generate one sample (call @ 48kHz)
void clear();                           // Reset internal state
```

## Technical Details

- **Sample Rate**: Configurable (default 48kHz)
- **Waveform**: Selectable (sawtooth^2 default, sawtooth^3, raw sawtooth)
- **Filters**: 5× Biquad bandpass filters (formants F1-F5, F4/F5 optional)
- **Filter Bandwidth**: Constant 80Hz scaled by Q parameter
- **Pitch Bend**: ±2 semitones (exp2-based smooth transition)
- **Noise**: White noise for fricative consonants
- **LFO**: Triangle wave vibrato with delay envelope
- **Saturation**: Fast soft clipping (x / (1 + |x|))
- **AC Coupling**: DC blocking filter (R=0.90)
- **Fade Envelope**: 10ms fade in/out for click-free transitions


## Formant Frequencies

| Vowel | F1 (Hz) | F2 (Hz) | F3 (Hz) | G1 | G2 | G3 |
|-------|---------|---------|---------|----|----|----|
| A     | 700     | 1200    | 2500    | 1.0| 0.6| 0.2|
| I     | 300     | 2300    | 3000    | 1.0| 0.3| 0.1|
| U     | 350     | 1200    | 2100    | 1.0| 0.4| 0.1|
| E     | 450     | 1800    | 2600    | 1.0| 0.5| 0.2|
| O     | 500     | 800     | 2300    | 1.0| 0.7| 0.1|
| M     | 250     | 1200    | 2200    | 1.0| 0.1| 0.05|
| N     | 200     | 800     | 1800    | 1.0| 0.15| 0.05|
| AE    | 550     | 1700    | 2400    | 1.0| 0.55| 0.15|
| Y     | 300     | 1700    | 2200    | 1.0| 0.45| 0.15|
| R     | 400     | 1100    | 1700    | 1.0| 0.5| 0.1|
| W     | 380     | 600     | 1900    | 1.0| 0.35| 0.1|

## Platform Integration
**Note:** Integration examples are conceptual and not fully tested; platform-specific adjustments may be required.


### Raspberry Pi Pico (Pico-SDK)
RP2350 recommended
```cpp
#include "komyo.h"

Komyo::Komyo chanter;

void audio_callback() {
    int16_t sample = chanter.process() * 32767.0f;
    // Output to PWM/I2S
}
```


## License

MIT License - Copyright (c) 2025 Hügelton Instruments, Kōbe, Japan

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Acknowledgement
- Delay Lama by [AudioNerdz](http://www.audionerdz.nl/)

## Author

**Leo Kuroshita** - Hügelton Instruments

- X: [@kurogedelic](https://x.com/kurogedelic)
