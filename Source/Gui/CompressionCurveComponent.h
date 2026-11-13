#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// ===========================================================================
//  CompressionCurveComponent  —  dibuja la curva In→Out en tiempo real
//  v1.2: Knee ahora se visualiza como zona sombreada (rectángulo del knee)
//        en color semi-transparente, y la curva se redondea claramente.
//        Inspirado en FabFilter Pro-C.
// ===========================================================================

class CompressionCurveComponent : public juce::Component,
                                private juce::Timer
{
public:
    CompressionCurveComponent()
    {
        startTimerHz(30);
    }

    ~CompressionCurveComponent() override { stopTimer(); }

    struct CurveParams {
        float threshold = 0.0f;
        float ratio = 4.0f;
        float knee = 6.0f;
        float makeup = 0.0f;
        int   type = 0;  // 0=Digital, 1=VCA, 2=FET, 3=Optical, 4=VariMu
    };

    void setParams(const CurveParams& p)
    {
        juce::ScopedLock sl(lock);
        params = p;
    }

    void setBandIndex(int idx)
    {
        juce::ScopedLock sl(lock);
        bandIdx = idx;
    }

    void setInputLevelDb(float db)
    {
        juce::ScopedLock sl(lock);
        inputLevelDb = db;
    }

    void setGainReductionDb(float db)
    {
        juce::ScopedLock sl(lock);
        grDb = db;
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);

        // Fondo
        g.setColour(juce::Colour(0xff0f0f12));
        g.fillRoundedRectangle(bounds, 6.0f);

        CurveParams p;
        float inLvl, gr;
        int bIdx;
        { juce::ScopedLock sl(lock); p = params; inLvl = inputLevelDb; gr = grDb; bIdx = bandIdx; }

        // Colores por banda
        static const juce::Colour bandColours[] = {
            juce::Colour(0xff4a9eff),
            juce::Colour(0xff50ff80),
            juce::Colour(0xffffc060),
            juce::Colour(0xffff5050)
        };
        juce::Colour curveColor = bandColours[bIdx % 4];

        // Marco
        g.setColour(juce::Colour(0xff2a2a2a));
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

        // Ejes: 0 dB arriba-derecha, -60 dB abajo-izquierda
        auto plotBounds = bounds.reduced(8.0f);
        auto dbToX = [&](float db) {
            return plotBounds.getX() + juce::jmap(db, -60.0f, 0.0f, 0.0f, plotBounds.getWidth());
        };
        auto dbToY = [&](float db) {
            return plotBounds.getY() + juce::jmap(db, -60.0f, 0.0f, plotBounds.getHeight(), 0.0f);
        };

        // ─── Grid (cada 12 dB) ──────────────────────────────────────────
        g.setColour(juce::Colour(0xff1f1f25));
        for (int db = 0; db >= -60; db -= 12)
        {
            float y = dbToY(static_cast<float>(db));
            g.drawHorizontalLine(static_cast<int>(y), plotBounds.getX(), plotBounds.getRight());
            float x = dbToX(static_cast<float>(db));
            g.drawVerticalLine(static_cast<int>(x), plotBounds.getY(), plotBounds.getBottom());
        }

        // ─── Línea 1:1 de referencia (punteada) ────────────────────────
        g.setColour(juce::Colour(0xff404040));
        juce::Point<float> p1(dbToX(-60.0f), dbToY(-60.0f));
        juce::Point<float> p2(dbToX(0.0f), dbToY(0.0f));
        float dash[2] = { 4.0f, 4.0f };
        g.drawDashedLine(juce::Line<float>(p1, p2), dash, 2, 1.0f);

        // ─── Knee zone (sombreada) ─ FabFilter-style ──────────────────
        // Sólo para tipos con soft-knee significativo (VCA, FET, Optical, VariMu)
        // Digital no tiene knee efectivo aunque el control exista.
        if (p.knee > 0.5f && p.type != 0)
        {
            float kneeHalf = p.knee * 0.5f;
            float kneeLow = p.threshold - kneeHalf;
            float kneeHigh = p.threshold + kneeHalf;
            float kx1 = dbToX(juce::jlimit(-60.0f, 0.0f, kneeLow));
            float kx2 = dbToX(juce::jlimit(-60.0f, 0.0f, kneeHigh));
            auto kneeRect = juce::Rectangle<float>(kx1, plotBounds.getY(),
                                                    kx2 - kx1, plotBounds.getHeight());
            g.setColour(curveColor.withAlpha(0.10f));
            g.fillRect(kneeRect);
            // Líneas verticales delimitando el knee
            g.setColour(curveColor.withAlpha(0.4f));
            float dashK[2] = { 2.0f, 2.0f };
            g.drawDashedLine(juce::Line<float>(juce::Point<float>(kx1, plotBounds.getY()),
                                                juce::Point<float>(kx1, plotBounds.getBottom())),
                              dashK, 2, 0.8f);
            g.drawDashedLine(juce::Line<float>(juce::Point<float>(kx2, plotBounds.getY()),
                                                juce::Point<float>(kx2, plotBounds.getBottom())),
                              dashK, 2, 0.8f);
        }

        // ─── Línea de threshold ────────────────────────────────────────
        float thrX = dbToX(p.threshold);
        float thrY = dbToY(p.threshold);
        g.setColour(juce::Colour(0xff808080));
        g.drawVerticalLine(static_cast<int>(thrX), plotBounds.getY(), plotBounds.getBottom());
        g.drawHorizontalLine(static_cast<int>(thrY), plotBounds.getX(), plotBounds.getRight());

        // ─── Curva de transferencia ────────────────────────────────────
        // Primero un "fill" bajo la curva para realzarla
        juce::Path fillPath;
        bool started = false;
        const int steps = 200;
        float firstX = 0.0f, lastX = 0.0f;
        for (int i = 0; i <= steps; ++i)
        {
            float inDb = -60.0f + 60.0f * (float)i / steps;
            float outDb = computeOutputDb(inDb, p);
            float x = dbToX(inDb);
            float y = dbToY(outDb);
            if (!started) { fillPath.startNewSubPath(x, plotBounds.getBottom()); fillPath.lineTo(x, y); started = true; firstX = x; }
            else fillPath.lineTo(x, y);
            lastX = x;
        }
        fillPath.lineTo(lastX, plotBounds.getBottom());
        fillPath.closeSubPath();
        g.setColour(curveColor.withAlpha(0.08f));
        g.fillPath(fillPath);

        // Curva como tal (stroke grueso)
        juce::Path curve;
        started = false;
        for (int i = 0; i <= steps; ++i)
        {
            float inDb = -60.0f + 60.0f * (float)i / steps;
            float outDb = computeOutputDb(inDb, p);
            float x = dbToX(inDb);
            float y = dbToY(outDb);
            if (!started) { curve.startNewSubPath(x, y); started = true; }
            else curve.lineTo(x, y);
        }
        g.setColour(curveColor);
        g.strokePath(curve, juce::PathStrokeType(2.5f));

        // ─── Punto rojo = nivel actual de entrada ──────────────────────
        if (inLvl > -60.0f)
        {
            float px = dbToX(juce::jlimit(-60.0f, 0.0f, inLvl));
            float outAtThat = computeOutputDb(juce::jlimit(-60.0f, 0.0f, inLvl), p);
            float py = dbToY(outAtThat);

            // Sombra
            g.setColour(juce::Colours::black.withAlpha(0.4f));
            g.fillEllipse(px - 4.0f + 1.0f, py - 4.0f + 1.0f, 8.0f, 8.0f);

            // Punto blanco
            g.setColour(juce::Colours::white);
            g.fillEllipse(px - 4.0f, py - 4.0f, 8.0f, 8.0f);
            g.setColour(curveColor);
            g.drawEllipse(px - 4.0f, py - 4.0f, 8.0f, 8.0f, 2.0f);

            // Línea vertical de seguimiento (input level)
            g.setColour(juce::Colours::white.withAlpha(0.15f));
            g.drawVerticalLine(static_cast<int>(px), plotBounds.getY(), plotBounds.getBottom());
        }

        // ─── Indicador de GR actual (texto grande arriba-izquierda) ──
        if (gr < -0.05f)
        {
            juce::Font grFont(juce::FontOptions(22.0f));
            grFont.setBold(true);
            g.setFont(grFont);
            juce::String grText = juce::String(gr, 1) + " dB";
            juce::Colour grColor = (gr < -12.0f) ? juce::Colour(0xffff5050)
                                  : (gr < -6.0f) ? juce::Colour(0xffffc060)
                                  : juce::Colour(0xff80ff80);
            g.setColour(grColor);
            g.drawText(grText,
                       static_cast<int>(plotBounds.getX() + 6),
                       static_cast<int>(plotBounds.getY() + 4),
                       90, 24, juce::Justification::left);
            juce::Font labFont(juce::FontOptions(9.0f));
            g.setFont(labFont);
            g.setColour(juce::Colour(0xff909090));
            g.drawText("GR",
                       static_cast<int>(plotBounds.getX() + 6),
                       static_cast<int>(plotBounds.getY() + 26),
                       30, 10, juce::Justification::left);
        }

        // Etiquetas
        g.setColour(juce::Colours::lightgrey);
        juce::FontOptions fo(11.0f);
        g.setFont(juce::Font(fo));
        g.drawText("In", plotBounds.getX(), plotBounds.getBottom() - 14, 20, 12, juce::Justification::left);
        g.drawText("Out", plotBounds.getRight() - 24, plotBounds.getY(), 24, 12, juce::Justification::right);
        g.drawText("0 dB", plotBounds.getRight() - 28, plotBounds.getY() + 2, 28, 12, juce::Justification::right);
        g.drawText("-60", plotBounds.getX(), plotBounds.getBottom() - 14, 24, 12, juce::Justification::left);

        // Etiqueta de knee si es significativo
        if (p.knee > 0.5f && p.type != 0)
        {
            float kneeHalf = p.knee * 0.5f;
            float kneeMid = p.threshold;
            float mx = dbToX(kneeMid);
            juce::Font kneeFont(juce::FontOptions(9.0f));
            g.setFont(kneeFont);
            g.setColour(curveColor.withAlpha(0.8f));
            g.drawText("Knee " + juce::String(p.knee, 1) + " dB",
                       static_cast<int>(mx - 30), static_cast<int>(plotBounds.getBottom() - 14),
                       80, 10, juce::Justification::left);
        }
    }

private:
    void timerCallback() override { repaint(); }

    float computeOutputDb(float inDb, const CurveParams& p) const
    {
        float gr = 0.0f;
        const float thr = p.threshold;
        const float rat = p.ratio;
        const float knee = p.knee;
        const float kneeHalf = knee * 0.5f;
        const float kneeLow = thr - kneeHalf;
        const float kneeHigh = thr + kneeHalf;

        switch (p.type)
        {
        case 0:  // Digital
            if (inDb > thr) gr = -(inDb - thr) * (1.0f - 1.0f / rat);
            break;
        case 1:  // VCA (soft-knee)
            if (inDb > kneeHigh)
                gr = -(inDb - thr) * (1.0f - 1.0f / rat);
            else if (inDb > kneeLow)
            {
                float t = (inDb - kneeLow) / (2.0f * kneeHalf);
                gr = -(inDb - kneeLow) * t * (1.0f - 1.0f / rat);
            }
            break;
        case 2:  // FET
            if (inDb > thr)
            {
                float over = inDb - thr;
                float effR = rat + std::min(8.0f, over * 0.4f);
                gr = -over * (1.0f - 1.0f / effR);
                gr = juce::jlimit(-24.0f, 0.0f, gr);
            }
            break;
        case 3:  // Optical
            if (inDb > thr)
            {
                float over = inDb - thr;
                float effR = rat + std::min(6.0f, over * 0.3f);
                gr = -over * (1.0f - 1.0f / effR);
                gr = juce::jlimit(-20.0f, 0.0f, gr);
            }
            break;
        case 4:  // Vari-Mu
            if (inDb > thr - 4.0f)
            {
                float over = inDb - thr + 4.0f;
                float effR = rat + std::min(12.0f, over * 0.6f);
                gr = -(inDb - thr) * (1.0f - 1.0f / effR);
                gr = juce::jlimit(-30.0f, 0.0f, gr);
            }
            break;
        }

        return inDb + gr + p.makeup;
    }

    CurveParams params;
    float inputLevelDb = -100.0f;
    float grDb = 0.0f;
    int bandIdx = 0;
    juce::CriticalSection lock;
};
