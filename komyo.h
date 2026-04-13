/*  Komyo */
/*  Buddhist chanter vocal synthesis engine */
/*  v2.1 */
/*  by Leo Kuroshita, Hügelton Instruments, Kōbe, Japan. */
/*  License: MIT */

#ifndef KOMYO_CORE_H
#define KOMYO_CORE_H

#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace Komyo {

constexpr float KOMYO_PI = 3.14159265358979323846f;
constexpr float LN2_OVER_1200 = 0.69314718056f / 1200.0f; // For fast vibrato ratio approximation
constexpr float DEFAULT_SAMPLE_RATE = 48000.0f;

#ifdef KOMYO_LIGHT_MODE
// Light Mode: LUT-based optimization for microcontrollers
constexpr int TRIG_LUT_SIZE = 256;
constexpr int EXP2_LUT_SIZE = 256;

// LUT storage (inline variables for header-only library)
inline float TRIG_SIN_LUT[TRIG_LUT_SIZE];
inline float TRIG_EXP2_LUT[EXP2_LUT_SIZE];
inline bool LUT_INITIALIZED = false;

inline void initLUTs() {
    if (LUT_INITIALIZED) return;

    // Initialize sine LUT (0 to π)
    for (int i = 0; i < TRIG_LUT_SIZE; i++) {
        float phase = (KOMYO_PI * i) / (TRIG_LUT_SIZE - 1);
        TRIG_SIN_LUT[i] = sinf(phase);
    }

    // Initialize exp2 LUT (-2 to +2 octaves, covers 4 octaves total)
    for (int i = 0; i < EXP2_LUT_SIZE; i++) {
        float x = -2.0f + (4.0f * i) / (EXP2_LUT_SIZE - 1);
        TRIG_EXP2_LUT[i] = exp2f(x);
    }

    LUT_INITIALIZED = true;
}

inline void ensureLUTsInitialized() {
    if (!LUT_INITIALIZED) {
        initLUTs();
    }
}

inline float lut_sin(float x) {
    // Wrap to 0-2π
    while (x < 0.0f) x += 2.0f * KOMYO_PI;
    while (x >= 2.0f * KOMYO_PI) x -= 2.0f * KOMYO_PI;

    // Map to 0-π and use symmetry
    if (x > KOMYO_PI) {
        x = 2.0f * KOMYO_PI - x;
    }

    // Linear interpolation
    float idx = x * (TRIG_LUT_SIZE - 1) / KOMYO_PI;
    int i = static_cast<int>(idx);
    float frac = idx - i;

    if (i >= TRIG_LUT_SIZE - 1) {
        return TRIG_SIN_LUT[TRIG_LUT_SIZE - 1];
    }

    return TRIG_SIN_LUT[i] + frac * (TRIG_SIN_LUT[i + 1] - TRIG_SIN_LUT[i]);
}

inline float lut_cos(float x) {
    return lut_sin(x + KOMYO_PI * 0.5f);
}

inline float lut_exp2(float x) {
    // Clamp to LUT range (-2 to +2 octaves)
    if (x < -2.0f) x = -2.0f;
    if (x > 2.0f) x = 2.0f;

    // Map to LUT index
    float idx = (x + 2.0f) * (EXP2_LUT_SIZE - 1) / 4.0f;
    int i = static_cast<int>(idx);
    float frac = idx - i;

    if (i >= EXP2_LUT_SIZE - 1) {
        return TRIG_EXP2_LUT[EXP2_LUT_SIZE - 1];
    }

    return TRIG_EXP2_LUT[i] + frac * (TRIG_EXP2_LUT[i + 1] - TRIG_EXP2_LUT[i]);
}

#define KOMYO_SIN(x) lut_sin(x)
#define KOMYO_COS(x) lut_cos(x)
#define KOMYO_EXP2(x) lut_exp2(x)

#else
// Normal mode: use standard math functions
#define KOMYO_SIN(x) sinf(x)
#define KOMYO_COS(x) cosf(x)
#define KOMYO_EXP2(x) exp2f(x)

inline void initLUTs() {}
inline void ensureLUTsInitialized() {}

#endif // KOMYO_LIGHT_MODE

struct FormantPreset {
    float f1, f2, f3, f4, f5;  // Formant frequencies (Hz), f4/f5 = 0 disables
    float g1, g2, g3, g4, g5;  // Formant gains, g4/g5 = 0 disables
    float normalizationGain;  // Gain to normalize loudness across vowels
};

class BiquadBPF {
    float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0.0f, z2 = 0.0f;

public:
    void setCoefficients(float freq, float q, float invSampleRate) {
        float w0 = 2.0f * KOMYO_PI * freq * invSampleRate;
        float cos_w0 = KOMYO_COS(w0);
        float sin_w0 = KOMYO_SIN(w0);
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
    // Sample rate configuration
    float sampleRate;
    float invSampleRate;
    int fadeSamples;
    float invFadeSamples;

    float phase = 0.0f;
    float lfoPhase = 0.0f;
    uint32_t sampleCounter = 0u;
    BiquadBPF filter1, filter2, filter3, filter4, filter5;

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
    float driveAmount = 0.8f; // reduced to prevent clipping

    // New v2.1 features
    float pitchBend = 0.0f;        // ±1.0 = ±2 semitones
    int waveformType = 0;          // 0=saw^2, 1=saw^3, 2=raw saw
    float noiseGain = 0.0f;        // Fricative noise amount


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

    inline float lerp(float a, float b, float t) { return a + t * (b - a); }

    void interpolateFormant(float speed) {
        currentFormant.f1 = lerp(currentFormant.f1, targetFormant.f1, speed);
        currentFormant.f2 = lerp(currentFormant.f2, targetFormant.f2, speed);
        currentFormant.f3 = lerp(currentFormant.f3, targetFormant.f3, speed);
        currentFormant.f4 = lerp(currentFormant.f4, targetFormant.f4, speed);
        currentFormant.f5 = lerp(currentFormant.f5, targetFormant.f5, speed);
        currentFormant.g1 = lerp(currentFormant.g1, targetFormant.g1, speed);
        currentFormant.g2 = lerp(currentFormant.g2, targetFormant.g2, speed);
        currentFormant.g3 = lerp(currentFormant.g3, targetFormant.g3, speed);
        currentFormant.g4 = lerp(currentFormant.g4, targetFormant.g4, speed);
        currentFormant.g5 = lerp(currentFormant.g5, targetFormant.g5, speed);
        currentFormant.normalizationGain = lerp(currentFormant.normalizationGain, targetFormant.normalizationGain, speed);
    }

    inline float dcFilter(float input) {
        float output = input - dcX1 + DC_R * dcY1;
        dcX1 = input;
        dcY1 = output;
        return output;
    }

    inline float applyFade(float sample) {
        if (isFadingIn) {
            fadeGain += invFadeSamples;
            if (fadeGain >= 1.0f) {
                fadeGain = 1.0f;
                isFadingIn = false;
            }
        } else if (isFadingOut) {
            fadeGain -= invFadeSamples;
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
    Komyo(float sampleRate = DEFAULT_SAMPLE_RATE)
        : sampleRate(sampleRate)
        , invSampleRate(1.0f / sampleRate)
        , fadeSamples(static_cast<int>(sampleRate * 0.01f))  // 10ms fade
        , invFadeSamples(1.0f / fadeSamples)
    {
        ensureLUTsInitialized();  // Auto-initialize LUTs in light mode
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
        formantRatio = KOMYO_EXP2(offset / 12.0f); // Pre-calculate ratio
    }
    void setPitchOffset(float offset) { 
        pitchOffset = offset; 
        pitchRatio = KOMYO_EXP2(offset / 12.0f);   // Pre-calculate ratio
    }
    void setPitchPortamento(float speed) { pitchPortamentoSpeed = speed; }
    void setVibratoDepth(float depth) { vibratoDepth = depth; }
    void setVibratoSpeed(float speed) { vibratoSpeed = speed; }
    void setVibratoDelay(float delay) { vibratoDelay = delay * 0.001f; } // ms to seconds via multiplication
    void setMasterVolume(float vol) { masterVolume = vol; }
    void setDrive(float drive) { driveAmount = drive; }
    void setQ(float q) { resonanceQ = q; }
    void setBaseFreq(float freq) { baseFreq = freq; }
    void setSampleRate(float newSampleRate) {
        sampleRate = newSampleRate;
        invSampleRate = 1.0f / sampleRate;
        fadeSamples = static_cast<int>(sampleRate * 0.01f);  // 10ms fade
        invFadeSamples = 1.0f / fadeSamples;
    }

    // New v2.1 features
    void setPitchBend(float bend) { pitchBend = bend; }  // ±1.0 = ±2 semitones
    void setWaveformType(int type) { waveformType = type; }  // 0=saw^2, 1=saw^3, 2=raw saw
    void setNoiseGain(float gain) { noiseGain = gain; }  // Fricative amount

    float getPortamento() const { return portamentoSpeed; }
    float getPitchPortamento() const { return pitchPortamentoSpeed; }

    void noteOn(float midiNote) {
        float newFreq = 440.0f * KOMYO_EXP2((midiNote - 69.0f) / 12.0f);

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

        ++sampleCounter;
        bool updateFilters = (sampleCounter & 0x03u) == 0u;  // Every 4 samples

        noteOnTimer += invSampleRate;

        // Pitch portamento
        currentFreq = lerp(currentFreq, targetFreq, pitchPortamentoSpeed);

        // Apply pre-calculated pitch ratio and pitch bend
        float workingFreq = currentFreq * pitchRatio;
        if (pitchBend != 0.0f) {
            workingFreq *= std::exp2(pitchBend / 12.0f);  // ±1.0 = ±2 semitones
        }

        // Update vibrato
        vibratoTimer += invSampleRate;

        // Fast Triangle Wave LFO without trigonometric functions
        lfoPhase += vibratoSpeed * invSampleRate;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        float triangle = (lfoPhase < 0.5f) ? (4.0f * lfoPhase - 1.0f) : (3.0f - 4.0f * lfoPhase);

        float vibratoEnv = (vibratoTimer >= vibratoDelay) ? 1.0f : 0.0f;

        // Fast vibrato application using linear approximation
        float vibratoCents = triangle * vibratoDepth * vibratoEnv;
        float vibratoRatioMulti = 1.0f + (vibratoCents * LN2_OVER_1200);
        workingFreq *= vibratoRatioMulti;

        // Source waveform
        phase += workingFreq * invSampleRate;
        if (phase >= 1.0f) phase -= 1.0f;
        float rawSaw = (phase * 2.0f) - 1.0f;

        // Waveform type selection
        float glottalSource;
        switch (waveformType) {
            case 1:  // Cubic (saw^3) - very soft, mellow
                glottalSource = rawSaw * rawSaw * rawSaw;
                break;
            case 2:  // Raw sawtooth - bright, harsh
                glottalSource = rawSaw;
                break;
            case 0:  // Default: squared sawtooth with polarity preservation
            default:
                glottalSource = rawSaw * (rawSaw < 0.0f ? -rawSaw : rawSaw);
                break;
        }

        // Add fricative noise if enabled
        if (noiseGain > 0.0f) {
            float noise = (rand() / (float)RAND_MAX) * 2.0f - 1.0f;
            glottalSource += noise * noiseGain;
        }

        // Formant interpolation (every sample for smooth transitions)
        interpolateFormant(portamentoSpeed);

        // Apply filters (update coefficients every 4 samples for performance)
        // Use constant bandwidth (Hz) scaled by user Q parameter for more natural formant response
        if (updateFilters) {
            constexpr float FORMANT_BANDWIDTH = 80.0f;  // Approximate human vocal tract bandwidth (Hz)
            // Scale bandwidth by user Q parameter (higher Q = narrower bandwidth = sharper resonance)
            float scaledBandwidth = FORMANT_BANDWIDTH / (resonanceQ / 18.0f);  // Normalize around default Q=18
            float q1 = (currentFormant.f1 * formantRatio) / scaledBandwidth;
            float q2 = (currentFormant.f2 * formantRatio) / scaledBandwidth;
            float q3 = (currentFormant.f3 * formantRatio) / scaledBandwidth;
            // Clamp Q to prevent instability or overly sharp filters
            q1 = (q1 < 2.0f) ? 2.0f : (q1 > 50.0f) ? 50.0f : q1;
            q2 = (q2 < 2.0f) ? 2.0f : (q2 > 50.0f) ? 50.0f : q2;
            q3 = (q3 < 2.0f) ? 2.0f : (q3 > 50.0f) ? 50.0f : q3;

            filter1.setCoefficients(currentFormant.f1 * formantRatio, q1, invSampleRate);
            filter2.setCoefficients(currentFormant.f2 * formantRatio, q2, invSampleRate);
            filter3.setCoefficients(currentFormant.f3 * formantRatio, q3, invSampleRate);

            // F4 and F5 (optional, only if gain > 0)
            if (currentFormant.g4 > 0.0f && currentFormant.f4 > 0.0f) {
                float q4 = (currentFormant.f4 * formantRatio) / scaledBandwidth;
                q4 = (q4 < 2.0f) ? 2.0f : (q4 > 50.0f) ? 50.0f : q4;
                filter4.setCoefficients(currentFormant.f4 * formantRatio, q4, invSampleRate);
            }
            if (currentFormant.g5 > 0.0f && currentFormant.f5 > 0.0f) {
                float q5 = (currentFormant.f5 * formantRatio) / scaledBandwidth;
                q5 = (q5 < 2.0f) ? 2.0f : (q5 > 50.0f) ? 50.0f : q5;
                filter5.setCoefficients(currentFormant.f5 * formantRatio, q5, invSampleRate);
            }
        }

        float out = 0.0f;
        out += filter1.process(glottalSource) * currentFormant.g1;
        out += filter2.process(glottalSource) * currentFormant.g2;
        out += filter3.process(glottalSource) * currentFormant.g3;
        // F4 and F5 (optional, only if gain > 0)
        if (currentFormant.g4 > 0.0f) {
            out += filter4.process(glottalSource) * currentFormant.g4;
        }
        if (currentFormant.g5 > 0.0f) {
            out += filter5.process(glottalSource) * currentFormant.g5;
        }

        // Apply vowel-specific normalization gain for equal loudness
        out *= currentFormant.normalizationGain;

        // Apply DC blocking filter (remove DC offset before clipping)
        out = dcFilter(out);

        // Apply master volume and fast soft clipping
        out *= masterVolume;
        out = softClip(out * driveAmount);

        // Apply fade envelope
        out = applyFade(out);

        return out;
    }

    void clear() {
        phase = lfoPhase = 0.0f;
        sampleCounter = 0u;
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

// Normalization gains optimized for C4 (primary vocal range)
// C4 represents the most common vocal frequency range
inline const FormantPreset Komyo::V_A  = { 700.0f, 1200.0f, 2500.0f, 0.0f, 0.0f, 1.0f, 0.6f, 0.2f, 0.0f, 0.0f, 1.000f };
inline const FormantPreset Komyo::V_I  = { 260.0f, 2800.0f, 3300.0f, 0.0f, 0.0f, 1.0f, 0.6f, 0.2f, 0.0f, 0.0f, 0.707f };
inline const FormantPreset Komyo::V_U  = { 350.0f, 1200.0f, 2100.0f, 0.0f, 0.0f, 1.0f, 0.4f, 0.1f, 0.0f, 0.0f, 0.788f };
inline const FormantPreset Komyo::V_E  = { 450.0f, 1800.0f, 2600.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.2f, 0.0f, 0.0f, 0.914f };
inline const FormantPreset Komyo::V_O  = { 500.0f,  800.0f, 2300.0f, 0.0f, 0.0f, 1.0f, 0.7f, 0.1f, 0.0f, 0.0f, 0.690f };
inline const FormantPreset Komyo::V_M  = { 250.0f, 1200.0f, 2200.0f, 0.0f, 0.0f, 1.0f, 0.1f, 0.05f, 0.0f, 0.0f, 0.500f };
inline const FormantPreset Komyo::V_N  = { 200.0f,  800.0f, 1800.0f, 0.0f, 0.0f, 1.0f, 0.15f, 0.05f, 0.0f, 0.0f, 0.420f };
inline const FormantPreset Komyo::V_AE = { 550.0f, 1700.0f, 2400.0f, 0.0f, 0.0f, 1.0f, 0.55f, 0.15f, 0.0f, 0.0f, 0.350f };
inline const FormantPreset Komyo::V_Y  = { 300.0f, 1700.0f, 2200.0f, 0.0f, 0.0f, 1.0f, 0.45f, 0.15f, 0.0f, 0.0f, 0.794f };
inline const FormantPreset Komyo::V_R  = { 400.0f, 1100.0f, 1700.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.1f, 0.0f, 0.0f, 0.930f };
inline const FormantPreset Komyo::V_W  = { 380.0f,  600.0f, 1900.0f, 0.0f, 0.0f, 1.0f, 0.35f, 0.1f, 0.0f, 0.0f, 0.849f };

} // namespace Komyo

#endif // KOMYO_CORE_H
