/*  Komyo Core Library */
/*  Platform-independent vocal synthesis engine */
/*  v1.1 */
/*  by Leo Kuroshita, Hügelton Instruments, Kōbe, Japan. */
/*  License: MIT */

#ifndef KOMYO_CORE_H
#define KOMYO_CORE_H

#include <cmath>
#include <cstdint>
#include <vector>

namespace Komyo {

constexpr float SAMPLE_RATE = 48000.0f;
constexpr float PI = 3.14159265358979323846f;

struct FormantPreset {
    float f1, f2, f3;  // Formant frequencies (Hz)
    float g1, g2, g3;  // Formant gains
};

class BiquadBPF {
    float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0.0f, z2 = 0.0f;

public:
    void setCoefficients(float freq, float q) {
        float w0 = 2.0f * PI * freq / SAMPLE_RATE;
        float alpha = std::sin(w0) / (2.0f * q);
        float a0 = 1.0f + alpha;

        b0 = alpha / a0;
        b1 = 0.0f;
        b2 = -alpha / a0;
        a1 = (-2.0f * std::cos(w0)) / a0;
        a2 = (1.0f - alpha) / a0;
    }

    float process(float in) {
        float out = b0 * in + b1 * z1 + b2 * z2 - a1 * z1 - a2 * z2;
        z2 = z1;
        z1 = out;
        return out;
    }

    void clear() {
        z1 = z2 = 0.0f;
    }
};

class Komyo {
    float phase = 0.0f;
    float lfoPhase = 0.0f;
    BiquadBPF filter1, filter2, filter3;

    // 11 formant presets (Japanese vowels + special phonemes)
    static const FormantPreset V_A;   // A (Ah)
    static const FormantPreset V_I;   // I (Ee)
    static const FormantPreset V_U;   // U (Oo)
    static const FormantPreset V_E;   // E (Eh)
    static const FormantPreset V_O;   // O (Oh)
    static const FormantPreset V_M;   // M (Mm)
    static const FormantPreset V_N;   // N (Nn)
    static const FormantPreset V_AE;  // AE (Ash)
    static const FormantPreset V_Y;   // Y (Yue)
    static const FormantPreset V_R;   // R (Ruh)
    static const FormantPreset V_W;   // W (Wu)

    FormantPreset currentFormant = V_A;
    FormantPreset targetFormant = V_A;

    // Parameters
    float portamentoSpeed = 0.3f;
    float formantOffset = 0.0f;       // Semitone units
    float pitchOffset = 0.0f;         // Semitone units
    float pitchPortamentoSpeed = 0.1f;
    float baseFreq = 130.0f;
    float currentFreq = 130.0f;       // Current frequency (for pitch portamento)
    float targetFreq = 130.0f;        // Target frequency

    // Vibrato parameters
    float vibratoDepth = 50.0f;       // Cents
    float vibratoSpeed = 5.5f;        // Hz
    float vibratoDelay = 0.1f;        // Seconds
    float vibratoTimer = 0.0f;        // Time since note on (seconds)

    // Master volume
    float masterVolume = 0.5f;        // 0.0 - 1.0
    float resonanceQ = 18.0f;         // Filter Q value

    // Note on/off management
    bool noteIsActive = false;
    float noteOnTimer = 0.0f;         // Time since note on
    bool noteOffRequested = false;

    // DC blocking filter (AC coupling)
    float dcX1 = 0.0f;                // Previous input
    float dcY1 = 0.0f;                // Previous output
    constexpr static float DC_R = 0.90f;  // DC cutoff coefficient

    // Fade envelope
    float fadeGain = 0.0f;            // Current fade gain (0.0 - 1.0)
    bool isFadingIn = false;
    bool isFadingOut = false;
    constexpr static int FADE_SAMPLES = 480;  // 10ms @ 48kHz

    float lerp(float a, float b, float t) { return a + t * (b - a); }

    void interpolateFormant(float speed) {
        currentFormant.f1 = lerp(currentFormant.f1, targetFormant.f1, speed);
        currentFormant.f2 = lerp(currentFormant.f2, targetFormant.f2, speed);
        currentFormant.f3 = lerp(currentFormant.f3, targetFormant.f3, speed);
        currentFormant.g1 = lerp(currentFormant.g1, targetFormant.g1, speed);
        currentFormant.g2 = lerp(currentFormant.g2, targetFormant.g2, speed);
        currentFormant.g3 = lerp(currentFormant.g3, targetFormant.g3, speed);
    }

    float applyFormantOffset(float freq) {
        float ratio = std::pow(2.0f, formantOffset / 12.0f);
        return freq * ratio;
    }

    float dcFilter(float input) {
        float output = input - dcX1 + DC_R * dcY1;
        dcX1 = input;
        dcY1 = output;
        return output;
    }

    float applyFade(float sample) {
        if (isFadingIn) {
            fadeGain += 1.0f / FADE_SAMPLES;
            if (fadeGain >= 1.0f) {
                fadeGain = 1.0f;
                isFadingIn = false;
            }
        } else if (isFadingOut) {
            fadeGain -= 1.0f / FADE_SAMPLES;
            if (fadeGain <= 0.0f) {
                fadeGain = 0.0f;
                isFadingOut = false;
                noteIsActive = false;  // Actually stop note after fade completes
            }
        }
        return sample * fadeGain;
    }

public:
    Komyo() {
        currentFreq = baseFreq;
        targetFreq = baseFreq;
        currentFormant = V_A;
        targetFormant = V_A;
    }

    // Vowel selection (0=A, 1=I, 2=U, 3=E, 4=O, 5=M, 6=N, 7=AE, 8=Y, 9=R, 10=W)
    void setVowel(int vowelIndex) {
        switch (vowelIndex) {
            case 0: targetFormant = V_A; break;
            case 1: targetFormant = V_I; break;
            case 2: targetFormant = V_U; break;
            case 3: targetFormant = V_E; break;
            case 4: targetFormant = V_O; break;
            case 5: targetFormant = V_M; break;
            case 6: targetFormant = V_N; break;
            case 7: targetFormant = V_AE; break;
            case 8: targetFormant = V_Y; break;
            case 9: targetFormant = V_R; break;
            case 10: targetFormant = V_W; break;
            default: targetFormant = V_A; break;
        }
    }

    // Parameter setters
    void setPortamento(float speed) { portamentoSpeed = speed; }
    void setFormantOffset(float offset) { formantOffset = offset; }
    void setPitchOffset(float offset) { pitchOffset = offset; }
    void setPitchPortamento(float speed) { pitchPortamentoSpeed = speed; }
    void setVibratoDepth(float depth) { vibratoDepth = depth; }
    void setVibratoSpeed(float speed) { vibratoSpeed = speed; }
    void setVibratoDelay(float delay) { vibratoDelay = delay / 1000.0f; }  // ms to seconds
    void setMasterVolume(float vol) { masterVolume = vol; }
    void setQ(float q) { resonanceQ = q; }
    void setBaseFreq(float freq) { baseFreq = freq; }

    // Parameter getters
    float getPortamento() const { return portamentoSpeed; }
    float getPitchPortamento() const { return pitchPortamentoSpeed; }

    // Note input
    void noteOn(float midiNote) {
        float newFreq = 440.0f * std::pow(2.0f, (midiNote - 69.0f) / 12.0f);

        float freqDiff = std::abs(newFreq - currentFreq);
        if (freqDiff < 10.0f || currentFreq < 1.0f) {
            currentFreq = newFreq;
            targetFreq = newFreq;
        } else {
            targetFreq = newFreq;
        }

        noteIsActive = true;
        noteOnTimer = 0.0f;
        vibratoTimer = 0.0f;

        // Start fade in
        isFadingOut = false;
        isFadingIn = true;
        fadeGain = 0.0f;

        // Clear DC filter state to prevent pops
        dcX1 = 0.0f;
        dcY1 = 0.0f;
    }

    void noteOff() {
        if (!noteIsActive) return;
        isFadingIn = false;
        isFadingOut = true;
        noteOffRequested = true;
    }

    bool isNoteOffRequested() const {
        return noteOffRequested;
    }

    void clearNoteOffRequest() {
        noteOffRequested = false;
    }

    // Audio processing (call every sample at 48kHz)
    float process() {
        // Continue processing during fade out
        if (!noteIsActive && !isFadingOut) {
            return 0.0f;
        }

        noteOnTimer += 1.0f / SAMPLE_RATE;

        // Pitch portamento
        currentFreq = lerp(currentFreq, targetFreq, pitchPortamentoSpeed);

        // Apply pitch offset
        float pitchRatio = std::pow(2.0f, pitchOffset / 12.0f);
        float workingFreq = currentFreq * pitchRatio;

        // Update vibrato timer
        vibratoTimer += 1.0f / SAMPLE_RATE;

        // Triangle Wave LFO (vibrato)
        lfoPhase += (2.0f * PI * vibratoSpeed) / SAMPLE_RATE;
        if (lfoPhase >= 2.0f * PI) lfoPhase -= 2.0f * PI;

        float triangle = (2.0f / PI) * std::asin(std::sin(lfoPhase));

        // Vibrato delay envelope
        float vibratoEnv = 0.0f;
        if (vibratoTimer >= vibratoDelay) {
            vibratoEnv = 1.0f;
        }

        // Apply vibrato
        float vibratoCents = triangle * vibratoDepth * vibratoEnv;
        float vibratoRatio = std::pow(2.0f, vibratoCents / 1200.0f);
        workingFreq *= vibratoRatio;

        // Source waveform (sawtooth^3 for glottal source)
        phase += workingFreq / SAMPLE_RATE;
        if (phase >= 1.0f) phase -= 1.0f;
        float rawSaw = (phase * 2.0f) - 1.0f;
        float glottalSource = rawSaw * rawSaw * rawSaw;

        // Formant interpolation
        interpolateFormant(portamentoSpeed);

        // Apply filters
        filter1.setCoefficients(applyFormantOffset(currentFormant.f1), resonanceQ);
        filter2.setCoefficients(applyFormantOffset(currentFormant.f2), resonanceQ);
        filter3.setCoefficients(applyFormantOffset(currentFormant.f3), resonanceQ);

        float out = 0.0f;
        out += filter1.process(glottalSource) * currentFormant.g1;
        out += filter2.process(glottalSource) * currentFormant.g2;
        out += filter3.process(glottalSource) * currentFormant.g3;

        // Apply master volume and saturation
        out *= masterVolume;
        out = std::tanh(out * 1.5f);

        // Apply DC blocking filter (AC coupling)
        out = dcFilter(out);

        // Apply fade envelope
        out = applyFade(out);

        return out;
    }

    void clear() {
        phase = lfoPhase = 0.0f;
        filter1.clear();
        filter2.clear();
        filter3.clear();
        noteIsActive = false;
        noteOffRequested = false;
        isFadingIn = false;
        isFadingOut = false;
        fadeGain = 0.0f;
        dcX1 = 0.0f;
        dcY1 = 0.0f;
    }
};

// Static formant preset definitions
inline const FormantPreset Komyo::V_A  = { 700.0f, 1200.0f, 2500.0f, 1.0f, 0.6f, 0.2f };
inline const FormantPreset Komyo::V_I  = { 300.0f, 2300.0f, 3000.0f, 1.0f, 0.3f, 0.1f };
inline const FormantPreset Komyo::V_U  = { 350.0f, 1200.0f, 2100.0f, 1.0f, 0.4f, 0.1f };
inline const FormantPreset Komyo::V_E  = { 450.0f, 1800.0f, 2600.0f, 1.0f, 0.5f, 0.2f };
inline const FormantPreset Komyo::V_O  = { 500.0f,  800.0f, 2300.0f, 1.0f, 0.7f, 0.1f };
inline const FormantPreset Komyo::V_M  = { 250.0f, 1200.0f, 2200.0f, 1.0f, 0.1f, 0.05f };
inline const FormantPreset Komyo::V_N  = { 200.0f,  800.0f, 1800.0f, 1.0f, 0.15f, 0.05f };
inline const FormantPreset Komyo::V_AE = { 550.0f, 1700.0f, 2400.0f, 1.0f, 0.55f, 0.15f };
inline const FormantPreset Komyo::V_Y  = { 300.0f, 1700.0f, 2200.0f, 1.0f, 0.45f, 0.15f };
inline const FormantPreset Komyo::V_R  = { 400.0f, 1100.0f, 1700.0f, 1.0f, 0.5f, 0.1f };
inline const FormantPreset Komyo::V_W  = { 380.0f,  600.0f, 1900.0f, 1.0f, 0.35f, 0.1f };

} // namespace Komyo

#endif // KOMYO_CORE_H
