#pragma once
// Acoustic Control Corp. Model 260 - V2 behavioural model, derived from the 260/160 schematic.
// V2: cascaded preamp clipping, rebuilt fuzz, softer power-amp knee with sag + crossover grit.
// Pure C++17, no JUCE dependency.
//
// Signal path (follows the schematic):
//   Q1A input buffer -> Volume pot (L7A) + Bright (S1A) -> Q2A/Q3A gain stages (soft clip)
//   -> Q4A -> Treble/Bass stack (R18A/R19A, 25k) -> Q5A "Variamp" resonance (1.5H + C range)
//   -> Q6A -> Channel output Volume (R29A)
//   -> Fuzz (Q8/Q9/Q10, switchable) -> Spring reverb -> Tremolo (UJT osc + lamp/LDR)
//   -> solid-state power amp (clipping) -> 261-style 2x15 + horn cabinet

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace a260
{
constexpr double kPi = 3.14159265358979323846;

inline float dbToLin (float db) { return std::pow (10.0f, db / 20.0f); }
// Smooth saturating knee: approaches +/-t asymptotically. p=2 is gentle, p>=3 is harder.
inline float knee (float x, float tp, float tn, float p)
{
    const float t = (x >= 0.0f) ? tp : tn;
    const float r = std::fabs (x) / t;
    if (p == 2.0f) return x / std::sqrt (1.0f + r * r);
    return x / std::pow (1.0f + std::pow (r, p), 1.0f / p);
}
inline float taper (float v) { v = std::min (1.0f, std::max (0.0f, v)); return v * v; } // audio taper pot

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }

    inline float process (float x)
    {
        const double in = x;
        const double y = b0 * in + z1;
        z1 = b1 * in - a1 * y + z2;
        z2 = b2 * in - a2 * y;
        return (float) y;
    }

    void set (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }

    static double clampF (double fs, double f) { return std::min (std::max (f, 5.0), fs * 0.45); }

    void lowpass (double fs, double f, double Q)
    {
        f = clampF (fs, f);
        const double w = 2 * kPi * f / fs, c = std::cos (w), s = std::sin (w), al = s / (2 * Q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highpass (double fs, double f, double Q)
    {
        f = clampF (fs, f);
        const double w = 2 * kPi * f / fs, c = std::cos (w), s = std::sin (w), al = s / (2 * Q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void peaking (double fs, double f, double Q, double dB)
    {
        f = clampF (fs, f);
        const double A = std::pow (10.0, dB / 40.0);
        const double w = 2 * kPi * f / fs, c = std::cos (w), s = std::sin (w), al = s / (2 * Q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf (double fs, double f, double dB, double S = 0.8)
    {
        f = clampF (fs, f);
        const double A = std::pow (10.0, dB / 40.0);
        const double w = 2 * kPi * f / fs, c = std::cos (w), s = std::sin (w);
        const double al = s / 2 * std::sqrt ((A + 1 / A) * (1 / S - 1) + 2);
        const double beta = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + beta), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - beta),
             (A + 1) + (A - 1) * c + beta, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - beta);
    }
    void highShelf (double fs, double f, double dB, double S = 0.8)
    {
        f = clampF (fs, f);
        const double A = std::pow (10.0, dB / 40.0);
        const double w = 2 * kPi * f / fs, c = std::cos (w), s = std::sin (w);
        const double al = s / 2 * std::sqrt ((A + 1 / A) * (1 / S - 1) + 2);
        const double beta = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + beta), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - beta),
             (A + 1) - (A - 1) * c + beta, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - beta);
    }
};

// ---------------------------------------------------------------------------------------------
struct Params
{
    float inputTrimDb   = 0.0f;   // DAW-level trim ahead of the amp
    float volume        = 0.5f;   // L7A 50k "Volume" (0..1)
    bool  bright        = false;  // S1A
    float bass          = 0.5f;   // R19A 25k  (0.5 = flat)
    float treble        = 0.5f;   // R18A 25k  (0.5 = flat)
    int   variampRange  = 0;      // 0 = off, 1..4 = .18 / .047 / .012 / .0027 uF with 1.5H
    float variampEffect = 0.5f;   // R24A 50k "Variamp Effect"
    float level         = 0.7f;   // R29A 50k channel output volume
    bool  fuzzOn        = false;  // footswitch / S5
    float fuzzAttack    = 0.5f;   // R40 500k
    float fuzzGain      = 0.5f;   // R41 500k
    float reverb        = 0.0f;   // R59 50k
    float tremSpeed     = 0.4f;   // R78 (fine) / R79 (coarse)
    float tremIntensity = 0.0f;   // intensity pot
    float powerDriveDb  = 6.0f;   // push into the power amp
    float headroom      = 0.5f;   // "clipping adj" (R98) - supply headroom
    bool  cabOn         = true;
    float horn          = 0.5f;   // horn level
    float outputDb      = 0.0f;
};

// ---------------------------------------------------------------------------------------------
class Reverb
{
public:
    void prepare (double fs)
    {
        auto ms = [fs] (double t) { return std::max (4, (int) std::lround (t * 0.001 * fs)); };
        const double combMs[4] = { 29.7, 37.1, 41.1, 43.7 };
        const double dispMs[6] = { 1.1, 1.7, 2.3, 3.1, 3.7, 4.3 };
        for (int i = 0; i < 4; ++i) { comb[i].buf.assign ((size_t) ms (combMs[i]), 0.0f); comb[i].idx = 0; comb[i].lp = 0; }
        for (int i = 0; i < 6; ++i) { disp[i].buf.assign ((size_t) ms (dispMs[i]), 0.0f); disp[i].idx = 0; }
        inHP.highpass (fs, 220.0, 0.7);
        inLP.lowpass (fs, 3800.0, 0.7);
        outLP.lowpass (fs, 4200.0, 0.7);
        inHP.reset(); inLP.reset(); outLP.reset();
    }

    float process (float x)
    {
        float s = inLP.process (inHP.process (x));
        for (auto& d : disp) s = d.process (s, 0.55f);        // spring-style dispersion
        float acc = 0.0f;
        for (auto& c : comb) acc += c.process (s, 0.80f, 0.35f);
        return outLP.process (acc * 0.25f);
    }

private:
    struct Comb
    {
        std::vector<float> buf; int idx = 0; float lp = 0.0f;
        float process (float x, float fb, float damp)
        {
            const float y = buf[(size_t) idx];
            lp = y * (1.0f - damp) + lp * damp;
            buf[(size_t) idx] = x + lp * fb;
            if (++idx >= (int) buf.size()) idx = 0;
            return y;
        }
    };
    struct Allpass
    {
        std::vector<float> buf; int idx = 0;
        float process (float x, float g)
        {
            const float d = buf[(size_t) idx];
            const float out = -x + d;
            buf[(size_t) idx] = x + d * g;
            if (++idx >= (int) buf.size()) idx = 0;
            return out;
        }
    };
    Comb comb[4];
    Allpass disp[6];
    Biquad inHP, inLP, outLP;
};

// ---------------------------------------------------------------------------------------------
// Everything from the guitar input to the power-amp output. Run this at the (oversampled) rate.
class Amp
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        smoothCoef = 1.0f - std::exp (-1.0f / (float) (fs * 0.015));
        lampAtk = 1.0f - std::exp (-1.0f / (float) (fs * 0.012));
        lampRel = 1.0f - std::exp (-1.0f / (float) (fs * 0.045));
        envAtk  = 1.0f - std::exp (-1.0f / (float) (fs * 0.005));
        envRel  = 1.0f - std::exp (-1.0f / (float) (fs * 0.150));
        reverbUnit.prepare (fs);
        inHP.highpass (fs, 18.0, 0.6);
        fuzzHP.highpass (fs, 70.0, 0.7);
        fuzzDC.highpass (fs, 35.0, 0.7);
        fuzzLP.lowpass (fs, 6500.0, 0.7);
        dcBlock.highpass (fs, 12.0, 0.7);
        reset();
        setParams (params);
    }

    void reset()
    {
        for (Biquad* b : { &inHP, &bassShelf, &trebShelf, &brightShelf, &variampPk, &fuzzHP, &fuzzLP, &fuzzTight, &fuzzDC, &dcBlock })
            b->reset();
        phase = 0.0f; lamp = 0.0f; env = 0.0f;
        gVol = gLvl = gFuzzPre = gFuzzLvl = fuzzMix = gDrive = gHead = gReverb = gTremDepth = 0.0f;
        snapNext = true;
    }

    void setParams (const Params& p)
    {
        params = p;
        if (fs <= 0.0) return;

        tVol      = taper (p.volume);
        tLvl      = 2.0f * taper (p.level);
        tFuzzPre  = dbToLin (-6.0f + 24.0f * p.fuzzAttack);
        fuzzTight.highpass (fs, 100.0 + 1100.0 * p.fuzzAttack, 0.7);   // C26 / R40 attack network
        tFuzzLvl  = 0.15f + 1.2f * taper (p.fuzzGain);
        tFuzzMix  = p.fuzzOn ? 1.0f : 0.0f;
        tDrive    = dbToLin (p.powerDriveDb) * 0.25f;
        tHead     = 0.4f + 1.4f * p.headroom;
        tReverb   = 1.4f * taper (p.reverb);
        tTremDepth = 0.9f * p.tremIntensity;           // Acoustic quoted up to ~90% cut
        trimLin   = dbToLin (p.inputTrimDb);
        outLin    = dbToLin (p.outputDb);

        // tone stack (25k Baxandall-style, corner estimates)
        bassShelf.lowShelf (fs, 120.0, (p.bass - 0.5f) * 28.0f);
        trebShelf.highShelf (fs, 2500.0, (p.treble - 0.5f) * 28.0f);

        // bright switch: bypass cap across the volume pot, boost grows as the pot is turned down
        const float brightDb = p.bright ? 1.5f + 9.0f * (1.0f - p.volume) : 0.0f;
        brightShelf.highShelf (fs, 2200.0, brightDb);

        // Variamp: 1.5 H with .18 / .047 / .012 / .0027 uF -> 306 / 599 / 1186 / 2501 Hz
        static const double kVariampHz[4] = { 306.3, 599.4, 1186.3, 2500.9 };
        if (p.variampRange >= 1 && p.variampRange <= 4)
            variampPk.peaking (fs, kVariampHz[p.variampRange - 1], 2.0, 15.0 * p.variampEffect);
        else
            variampPk.peaking (fs, 1000.0, 1.0, 0.0);

        // tremolo speed: UJT relaxation osc, coarse + fine -> ~1.5..12 Hz
        trem_hz = 1.5f * std::pow (8.0f, p.tremSpeed);

        if (snapNext)
        {
            gVol = tVol; gLvl = tLvl; gFuzzPre = tFuzzPre; gFuzzLvl = tFuzzLvl; fuzzMix = tFuzzMix;
            gDrive = tDrive; gHead = tHead; gReverb = tReverb; gTremDepth = tTremDepth;
            snapNext = false;
        }
    }

    float process (float xin)
    {
        // one-pole smoothing of the pot-driven gains
        gVol += smoothCoef * (tVol - gVol);
        gLvl += smoothCoef * (tLvl - gLvl);
        gFuzzPre += smoothCoef * (tFuzzPre - gFuzzPre);
        gFuzzLvl += smoothCoef * (tFuzzLvl - gFuzzLvl);
        fuzzMix += smoothCoef * (tFuzzMix - fuzzMix);
        gDrive += smoothCoef * (tDrive - gDrive);
        gHead += smoothCoef * (tHead - gHead);
        gReverb += smoothCoef * (tReverb - gReverb);
        gTremDepth += smoothCoef * (tTremDepth - gTremDepth);

        // --- preamp: cascaded transistor stages on a 25 V rail ------------------------------------
        float x = inHP.process (xin * trimLin);
        x = brightShelf.process (x);
        x = knee (x * gVol * 14.0f, 2.4f, 1.9f, 2.0f);   // Q2A (gentle, asymmetric)
        x = knee (x * 3.0f,         3.2f, 2.5f, 2.0f);   // Q3A (second stage saturates progressively)
        x *= 0.5f;                                        // Q4A buffer into the tone stack
        x = bassShelf.process (x);
        x = trebShelf.process (x);
        x = variampPk.process (x);                        // Q5A + L1A
        x = knee (x * 1.5f, 3.0f, 3.0f, 3.0f);            // Q5A/Q6A output stage
        x *= gLvl;

        // --- fuzz: two cascaded asymmetric stages, interstage Attack network ------------------------
        if (fuzzMix > 0.0005f)
        {
            float f = fuzzHP.process (x);
            f = knee (f * gFuzzPre, 0.55f, 0.38f, 2.0f);   // Q8 (bias-starved, asymmetric)
            f = fuzzTight.process (f);                      // C26 / R40
            f = knee (f * 2.5f, 0.60f, 0.50f, 3.0f);        // Q9
            f = fuzzDC.process (f);
            f = fuzzLP.process (f) * gFuzzLvl * 5.0f;       // Q10 follower + Gain pot
            x = x * (1.0f - fuzzMix) + f * fuzzMix;
        }

        // --- reverb ----------------------------------------------------------------------
        if (gReverb > 0.0005f)
            x += reverbUnit.process (x) * gReverb;

        // --- tremolo: UJT relaxation oscillator -> lamp -> LDR --------------------------------
        if (gTremDepth > 0.0005f || lamp > 0.001f)
        {
            phase += trem_hz / (float) fs;
            if (phase >= 1.0f) phase -= 1.0f;
            const float drive = std::pow (phase, 0.6f);
            lamp += (drive > lamp ? lampAtk : lampRel) * (drive - lamp);
            const float ldr = std::pow (std::max (0.0f, lamp), 1.3f);
            x *= 1.0f - gTremDepth * ldr;
        }

        // --- solid-state power amp: soft-ish knee, rail sag, crossover grit ----------------------
        x *= gDrive;
        const float a = std::fabs (x);
        env += (a > env ? envAtk : envRel) * (a - env);
        const float L = std::max (0.2f, gHead * (1.0f - 0.18f * std::min (1.0f, env / gHead)));
        float y = knee (x, L, L, 3.0f);
        const float yn = y / L;
        y *= 1.0f - 0.10f * std::exp (-(yn * yn) / 0.0025f);   // crossover dead-zone
        y *= 0.5f / L;
        y = dcBlock.process (y);
        return y * outLin;
    }

private:
    static inline float softClipAsym (float x, float tp, float tn)
    {
        return (x >= 0.0f) ? tp * std::tanh (x / tp) : tn * std::tanh (x / tn);
    }

    Params params;
    double fs = 0.0;
    Biquad inHP, bassShelf, trebShelf, brightShelf, variampPk, fuzzHP, fuzzLP, fuzzTight, fuzzDC, dcBlock;
    Reverb reverbUnit;
    float smoothCoef = 0.01f, lampAtk = 0.01f, lampRel = 0.01f, envAtk = 0.01f, envRel = 0.001f;
    float tVol = 0, tLvl = 0, tFuzzPre = 1, tFuzzLvl = 0, tFuzzMix = 0, tDrive = 0, tHead = 1, tReverb = 0, tTremDepth = 0;
    float gVol = 0, gLvl = 0, gFuzzPre = 1, gFuzzLvl = 0, fuzzMix = 0, gDrive = 0, gHead = 1, gReverb = 0, gTremDepth = 0;
    float trimLin = 1, outLin = 1, trem_hz = 4.0f, phase = 0, lamp = 0, env = 0;
    bool snapNext = true;
};

// ---------------------------------------------------------------------------------------------
// 261-style cabinet: 2x15 woofers with a horn on top. Run at the host sample rate.
class Cabinet
{
public:
    void prepare (double fs)
    {
        sr = fs;
        hp.highpass (fs, 52.0, 0.8);
        boom.peaking (fs, 85.0, 1.0, 3.0);
        scoop.peaking (fs, 420.0, 0.9, -2.5);
        wooferLP.lowpass (fs, 3800.0, 0.7);
        wooferLP2.lowpass (fs, 3800.0, 0.7);
        hornHP.highpass (fs, 2800.0, 0.7);
        hornLP.lowpass (fs, 9500.0, 0.7);
        for (Biquad* b : { &hp, &boom, &scoop, &wooferLP, &wooferLP2, &hornHP, &hornLP }) b->reset();
    }
    void setHorn (float h) { hornLevel = 1.2f * taper (h) + 0.05f; }

    float process (float x)
    {
        float w = hp.process (x);
        w = boom.process (w);
        w = scoop.process (w);
        w = wooferLP2.process (wooferLP.process (w));
        const float h = hornLP.process (hornHP.process (x));
        return w + hornLevel * h;
    }
private:
    double sr = 44100.0;
    float hornLevel = 0.3f;
    Biquad hp, boom, scoop, wooferLP, wooferLP2, hornHP, hornLP;
};

} // namespace a260
