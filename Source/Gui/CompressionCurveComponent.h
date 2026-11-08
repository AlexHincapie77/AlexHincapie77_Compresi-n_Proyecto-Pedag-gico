/*
 * CompressionCurveComponent.h
 * ─────────────────────────────────────────────────────────────────────────
 * Visualizador pedagógico de la curva In → Out.
 * Muestra en tiempo real:
 *   • Línea 1:1 (sin compresión)
 *   • Curva de transferencia con threshold/ratio/knee/makeup
 *   • Punto rojo = nivel actual de entrada
 *   • Línea de threshold (naranja)
 *   • Valores numéricos en esquina
 *
 * Mejoras: protección completa contra NaN/Inf y bounds 0.
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <limits>

class CompressionCurveComponent : public juce::Component
{
public:
    CompressionCurveComponent()
    {
        setInterceptsMouseClicks (false, false);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (4.0f);
        if (bounds.getWidth() <= 10.0f || bounds.getHeight() <= 10.0f) return;

        // Fondo con gradiente sutil
        juce::ColourGradient grad (juce::Colour (0xff1a1a22), bounds.getTopLeft(),
                                     juce::Colour (0xff0e0e16), bounds.getBottomRight(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bounds, 8.0f);

        g.setColour (juce::Colour (0xff4a9eff).withAlpha (0.3f));
        g.drawRoundedRectangle (bounds, 8.0f, 1.5f);

        // Título
        g.setColour (juce::Colour (0xffc8c8d8));
        g.setFont (juce::Font (juce::FontOptions (14.0f)));
        auto titleArea = bounds.removeFromTop (20);
        g.drawText ("Curva de Transferencia", titleArea, juce::Justification::centred);

        const auto plot = bounds.reduced (10.0f);
        if (plot.getWidth() <= 0 || plot.getHeight() <= 0) return;

        const float minDb = -60.0f;
        const float maxDb = 24.0f;
        const float dbRange = maxDb - minDb;
        if (dbRange <= 0.0f) return;

        auto xToPx = [&] (float db) -> float
        {
            const float t = juce::jlimit (0.0f, 1.0f, (db - minDb) / dbRange);
            return plot.getX() + t * plot.getWidth();
        };
        auto yToPx = [&] (float db) -> float
        {
            const float t = juce::jlimit (0.0f, 1.0f, (db - minDb) / dbRange);
            return plot.getBottom() - t * plot.getHeight();
        };

        // Grid principal
        g.setColour (juce::Colour (0xff2c2c3a));
        for (float db = minDb; db <= maxDb + 0.01f; db += 12.0f)
        {
            const float x = xToPx (db);
            const float y = yToPx (db);
            g.drawVerticalLine   ((int) x, plot.getY(), plot.getBottom());
            g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        }

        // Etiquetas dB
        g.setColour (juce::Colour (0xff707088));
        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        for (float db = -48.0f; db <= 0.0f; db += 12.0f)
        {
            const float x = xToPx (db);
            g.drawText (juce::String (db, 0), juce::Rectangle<float> (x - 14.0f, plot.getBottom() + 1.0f, 28.0f, 10.0f),
                        juce::Justification::centred);
        }
        for (float db = -48.0f; db <= 24.0f; db += 24.0f)
        {
            const float y = yToPx (db);
            g.drawText (juce::String (db, 0), juce::Rectangle<float> (plot.getX() - 28.0f, y - 6.0f, 26.0f, 12.0f),
                        juce::Justification::centredRight);
        }

        // Línea 1:1 (sin compresión)
        g.setColour (juce::Colour (0xff505060));
        const float x1 = xToPx (minDb);
        const float y1 = yToPx (minDb);
        const float x2 = xToPx (maxDb);
        const float y2 = yToPx (maxDb);
        float dashes[] = { 4.0f, 4.0f };
        g.drawDashedLine (juce::Line<float> (x1, y1, x2, y2), dashes, 2, 1.0f);

        // Curva de compresión con gradiente
        const float safeRatio = (ratio > 0.01f) ? ratio : 0.01f;
        juce::Path curve;
        juce::Path fillPath;
        bool started = false;
        for (float inDb = minDb; inDb <= maxDb; inDb += 1.0f)
        {
            const float outDb = transferFunction (inDb, safeRatio);
            if (! std::isfinite (outDb)) continue;
            const float x = xToPx (inDb);
            const float y = yToPx (outDb);
            if (! started) { curve.startNewSubPath (x, y); started = true; }
            else            curve.lineTo (x, y);
        }

        // Glow debajo de la curva
        if (started)
        {
            g.setColour (curveColor.withAlpha (0.3f));
            g.strokePath (curve, juce::PathStrokeType (5.0f));
        }

        // Curva principal
        g.setColour (curveColor);
        g.strokePath (curve, juce::PathStrokeType (2.5f));

        // Threshold line
        if (thresholdDb > minDb && thresholdDb < maxDb)
        {
            const float tx = xToPx (thresholdDb);
            g.setColour (juce::Colour (0xfff5a623));
            float dashes2[] = { 3.0f, 2.0f };
            g.drawDashedLine (juce::Line<float> (tx, plot.getY(), tx, plot.getBottom()), dashes2, 2, 1.5f);

            // Etiqueta TH
            g.setColour (juce::Colour (0xfff5a623));
            g.setFont (juce::Font (juce::FontOptions (9.0f)));
            g.drawText ("TH", juce::Rectangle<float> (tx - 8.0f, plot.getY() + 2.0f, 16.0f, 12.0f),
                        juce::Justification::centred);
        }

        // Punto de operación actual (señal viva)
        if (inputEnvDb > minDb && inputEnvDb < maxDb && std::isfinite (inputEnvDb))
        {
            const float x = xToPx (inputEnvDb);
            const float outAtInput = transferFunction (inputEnvDb, safeRatio);
            if (std::isfinite (outAtInput))
            {
                const float y = yToPx (outAtInput);
                // Glow
                g.setColour (juce::Colour (0xffff5050).withAlpha (0.3f));
                g.fillEllipse (x - 8.0f, y - 8.0f, 16.0f, 16.0f);
                // Punto principal
                g.setColour (juce::Colour (0xffff5050));
                g.fillEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f);
                g.setColour (juce::Colour (0xffffffff));
                g.drawEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f, 1.0f);
            }
        }

        // Valores numéricos en esquina
        g.setColour (juce::Colour (0xffc8c8d8));
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        juce::String txt;
        txt << "THR: "  << juce::String (thresholdDb, 1) << " dB   "
            << "RATIO: " << juce::String (ratio, 2) << ":1   "
            << "KNEE: "  << juce::String (kneeDb, 1) << " dB   "
            << "MK: "    << juce::String (makeupDb, 1) << " dB";
        g.drawText (txt, bounds.removeFromBottom (16), juce::Justification::centredLeft);
    }

    void setThreshold (float v) { thresholdDb = v; }
    void setRatio     (float v) { ratio = v; }
    void setKnee      (float v) { kneeDb = v; }
    void setMakeup    (float v) { makeupDb = v; }
    void setInputEnv  (float v) { inputEnvDb = v; }
    void setCompType  (int t)   { compType = t; }
    void setCurveColor (juce::Colour c) { curveColor = c; }

private:
    float transferFunction (float inDb, float safeRatio) const
    {
        float outDb = inDb;
        const float overDb = inDb - thresholdDb;

        if (compType == 1 && kneeDb > 0.5f)  // VCA soft knee
        {
            const float halfKnee = kneeDb * 0.5f;
            if (overDb > halfKnee)
            {
                outDb = thresholdDb + overDb / safeRatio;
            }
            else if (overDb > -halfKnee)
            {
                const float x = overDb + halfKnee;
                const float denom = 4.0f * halfKnee;
                if (denom > 0.001f)
                {
                    const float k = (1.0f - 1.0f / safeRatio) / denom;
                    outDb = inDb - k * x * x;
                }
            }
        }
        else
        {
            if (overDb > 0.0f)
                outDb = thresholdDb + overDb / safeRatio;
        }

        outDb += makeupDb;
        return outDb;
    }

    float thresholdDb = -20.0f;
    float ratio       = 4.0f;
    float kneeDb      = 6.0f;
    float makeupDb    = 0.0f;
    float inputEnvDb  = -100.0f;
    int   compType    = 0;
    juce::Colour curveColor { 0xff4a9eff };
};
