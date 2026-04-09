/*  Komyo */
/*  Buddhist chanter vocal synthesis engine */
/*  v1.2 */
/*  by Leo Kuroshita, Hügelton Instruments, Kōbe, Japan. */
/*  License: MIT */

#ifndef KOMYO_CORE_H
#define KOMYO_CORE_H

#include <cmath>
#include <cstdint>

namespace Komyo {

constexpr float SAMPLE_RATE = 48000.0f;
constexpr float INV_SAMPLE_RATE = 1.0f / SAMPLE_RATE;
constexpr float PI = 3.14159265358979323846f;
constexpr float LN2_OVER_1200 = 0.69314718056f / 1200.0f; // For fast vibrato ratio approximation

struct FormantPreset {
    float f1, f2, f3;  // Formant frequencies (Hz)
    float g1, g2, g3;  // Formant gains
};

class BiquadBPF {
    float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0.0f, z2 = 0.0f;

public:
    void setCoefficients(float freq, float q) {
        float w0 = 2.0f * PI * freq * INV_SAMPLE_RATE;
        float cos_w0 = std::cos(w0);
        float sin_w0 = std::sin(w0);
        float alpha = sin_w0 / (2.0f * q);
        float a0_inv = 1.0f / (1.0f + alpha);

        b0 = alpha * a0_inv;
        b1 = 0.0f;
        b2 = -b0; // -alpha / a0 is same as -b0
        a1 = (-2.0f * cos_w0) * a0_inv;
        a2 = (1.0f - alpha) * a0_inv;
    }

    inline float process(float in) {
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
    static const FormantPreset V_A;
    static const FormantPreset V_I;
    static const FormantPreset V_U;
    static const FormantPreset V_E;
    static const FormantPreset V_O;
    static const FormantPreset V_M;
    static const FormantPreset V_N;
    static const FormantPreset V_AE;
    static const FormantPreset V_Y;
    static const FormantPreset V_R;
    static const FormantPreset V_W;

    FormantPreset currentFormant = V_A;
    FormantPreset targetFormant = V_A;

    // Parameters
    float portamentoSpeed = 0.3f;
    float formantOffset = 0.0f;
    float formantRatio = 1.0f;        // Cached multiplier
    float pitchOffset = 0.0f;
    float pitchRatio = 1.0f;          // Cached multiplier
    float pitchPortamentoSpeed = 0.1f;
    float baseFreq = 130.0f;
    float currentFreq = 130.0f;
    float targetFreq = 130.0f;

    // Vibrato parameters
    float vibratoDepth = 50.0f;
    float vibratoSpeed = 5.5f;
    float vibratoDelay = 0.1f;
    float vibratoTimer = 0.0f;

    // Master volume & Filter
    float masterVolume = 0.5f;
    float resonanceQ = 18.0f;
    float driveAmount = 1.5f; // soft clip or Otoware

    // Note on/off management
    bool noteIsActive = false;
    float noteOnTimer = 0.0f;
    bool noteOffRequested = false;

    // DC blocking filter (AC coupling)
    float dcX1 = 0.0f;
    float dcY1 = 0.0f;
    constexpr static float DC_R = 0.90f;

    // Fade envelope
    float fadeGain = 0.0f;
    bool isFadingIn = false;
    bool isFadingOut = false;
    constexpr static int FADE_SAMPLES = 480;
    constexpr static float INV_FADE_SAMPLES = 1.0f / FADE_SAMPLES;

    inline float lerp(float a, float b, float t) { return a + t * (b - a); }

    void interpolateFormant(float speed) {
        currentFormant.f1 = lerp(currentFormant.f1, targetFormant.f1, speed);
        currentFormant.f2 = lerp(currentFormant.f2, targetFormant.f2, speed);
        currentFormant.f3 = lerp(currentFormant.f3, targetFormant.f3, speed);
        currentFormant.g1 = lerp(currentFormant.g1, targetFormant.g1, speed);
        currentFormant.g2 = lerp(currentFormant.g2, targetFormant.g2, speed);
        currentFormant.g3 = lerp(currentFormant.g3, targetFormant.g3, speed);
    }

    inline float dcFilter(float input) {
        float output = input - dcX1 + DC_R * dcY1;
        dcX1 = input;
        dcY1 = output;
        return output;
    }

    inline float applyFade(float sample) {
        if (isFadingIn) {
            fadeGain += INV_FADE_SAMPLES;
            if (fadeGain >= 1.0f) {
                fadeGain = 1.0f;
                isFadingIn = false;
            }
        } else if (isFadingOut) {
            fadeGain -= INV_FADE_SAMPLES;
            if (fadeGain <= 0.0f) {
                fadeGain = 0.0f;
                isFadingOut = false;
                noteIsActive = false;
            }
        }
        return sample * fadeGain;
    }

    // Fast soft clipping to replace std::tanh
    inline float softClip(float x) {
        float ax = (x < 0.0f) ? -x : x;
        return x / (1.0f + ax);
    }

public:
    Komyo() {
        currentFreq = baseFreq;
        targetFreq = baseFreq;
        currentFormant = V_A;
        targetFormant = V_A;
    }

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

    void setPortamento(float speed) { portamentoSpeed = speed; }
    void setFormantOffset(float offset) { 
        formantOffset = offset; 
        formantRatio = std::exp2(offset / 12.0f); // Pre-calculate ratio
    }
    void setPitchOffset(float offset) { 
        pitchOffset = offset; 
        pitchRatio = std::exp2(offset / 12.0f);   // Pre-calculate ratio
    }
    void setPitchPortamento(float speed) { pitchPortamentoSpeed = speed; }
    void setVibratoDepth(float depth) { vibratoDepth = depth; }
    void setVibratoSpeed(float speed) { vibratoSpeed = speed; }
    void setVibratoDelay(float delay) { vibratoDelay = delay * 0.001f; } // ms to seconds via multiplication
    void setMasterVolume(float vol) { masterVolume = vol; }
    void setDrive(float drive) { driveAmount = drive; }
    void setQ(float q) { resonanceQ = q; }
    void setBaseFreq(float freq) { baseFreq = freq; }

    float getPortamento() const { return portamentoSpeed; }
    float getPitchPortamento() const { return pitchPortamentoSpeed; }

    void noteOn(float midiNote) {
        float newFreq = 440.0f * std::exp2((midiNote - 69.0f) / 12.0f);

        float freqDiff = newFreq > currentFreq ? (newFreq - currentFreq) : (currentFreq - newFreq);
        if (freqDiff < 10.0f || currentFreq < 1.0f) {
            currentFreq = newFreq;
            targetFreq = newFreq;
        } else {
            targetFreq = newFreq;
        }

        noteIsActive = true;
        noteOnTimer = 0.0f;
        vibratoTimer = 0.0f;

        isFadingOut = false;
        isFadingIn = true;
        fadeGain = 0.0f;

        dcX1 = 0.0f;
        dcY1 = 0.0f;
    }

    void noteOff() {
        if (!noteIsActive) return;
        isFadingIn = false;
        isFadingOut = true;
        noteOffRequested = true;
    }

    bool isNoteOffRequested() const { return noteOffRequested; }
    void clearNoteOffRequest() { noteOffRequested = false; }

    float process() {
        if (!noteIsActive && !isFadingOut) {
            return 0.0f;
        }

        noteOnTimer += INV_SAMPLE_RATE;

        // Pitch portamento
        currentFreq = lerp(currentFreq, targetFreq, pitchPortamentoSpeed);

        // Apply pre-calculated pitch ratio
        float workingFreq = currentFreq * pitchRatio;

        // Update vibrato
        vibratoTimer += INV_SAMPLE_RATE;

        // Fast Triangle Wave LFO without trigonometric functions
        lfoPhase += vibratoSpeed * INV_SAMPLE_RATE;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        float triangle = (lfoPhase < 0.5f) ? (4.0f * lfoPhase - 1.0f) : (3.0f - 4.0f * lfoPhase);

        float vibratoEnv = (vibratoTimer >= vibratoDelay) ? 1.0f : 0.0f;

        // Fast vibrato application using linear approximation
        float vibratoCents = triangle * vibratoDepth * vibratoEnv;
        float vibratoRatioMulti = 1.0f + (vibratoCents * LN2_OVER_1200);
        workingFreq *= vibratoRatioMulti;

        // Source waveform
        phase += workingFreq * INV_SAMPLE_RATE;
        if (phase >= 1.0f) phase -= 1.0f;
        float rawSaw = (phase * 2.0f) - 1.0f;
        float glottalSource = rawSaw * rawSaw * rawSaw;

        // Formant interpolation
        interpolateFormant(portamentoSpeed);

        // Apply filters using pre-calculated formantRatio
        filter1.setCoefficients(currentFormant.f1 * formantRatio, resonanceQ);
        filter2.setCoefficients(currentFormant.f2 * formantRatio, resonanceQ);
        filter3.setCoefficients(currentFormant.f3 * formantRatio, resonanceQ);

        float out = 0.0f;
        out += filter1.process(glottalSource) * currentFormant.g1;
        out += filter2.process(glottalSource) * currentFormant.g2;
        out += filter3.process(glottalSource) * currentFormant.g3;

        // Apply master volume and fast soft clipping (instead of tanh)
        out *= masterVolume;
        out = softClip(out * driveAmount); // default is 1.5f

        // Filters and Envelopes
        out = dcFilter(out);
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
        dcX1 = dcY1 = 0.0f;
    }
};

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
