/*
 * BandSplitVisualizer.h
 * ─────────────────────────────────────────────────────────────────────────
 * Visualiza las bandas de frecuencia activas y su compresión.
 *
 *  • Barra horizontal logarítmica (20 Hz → 20 kHz)
 *  • Cada banda pintada con su color
 *  • Líneas verticales en los puntos de crossover (drag para cambiar)
 *  • Barra de GR superpuesta por banda (cuánto comprime cada banda en vivo)
 *  • Caja de texto con la frecuencia de crossover
 *
 * Pedagógico: el usuario ve qué rango de frecuencias está comprimiendo cada
 * "color" y cuánto GR hay en cada una.
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <cmath>
#include <algorithm>

class BandSplitVisualizer : public juce::Component
{
public:
    static constexpr float kMinFreq = 20.0f;
    static constexpr float kMaxFreq = 20000.0f;

    BandSplitVisualizer()
    {
        setInterceptsMouseClicks (true, false);
    }

    void setNumBands (int n)
    {
        numBands = juce::jlimit (1, 4, n);
        crossoverDrags.clear();
        crossoverDrags.resize (numBands - 1, false);
        repaint();
    }

    void setCrossover (int index, float freqHz)
    {
        if (index >= 0 && index < 3)
        {
            crossovers[index] = freqHz;
            repaint();
        }
    }

    void setBandGainReduction (int band, float grDb)
    {
        if (band >= 0 && band < 4)
            bandGr[band] = grDb;
        repaint();
    }

    void setBandColor (int band, juce::Colour c)
    {
        if (band >= 0 && band < 4)
            bandColors[band] = c;
        repaint();
    }

    // Para arrastrar crossovers
    std::function<void (int, float)> onCrossoverChanged;

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (4.0f);
        if (bounds.getWidth() <= 10.0f || bounds.getHeight() <= 10.0f) return;

        g.setColour (juce::Colour (0xff1a1a22));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (juce::Colour (0xff6b7080));
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

        g.setColour (juce::Colour (0xffc8c8d8));
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        bounds.removeFromTop (18);
        g.drawText ("Espectro / Bandas (20 Hz - 20 kHz)", bounds.removeFromTop (16), juce::Justification::centred);

        plotBounds = bounds.reduced (8.0f);

        // Escala logarítmica: posiciones de banda 0..N
        auto xFromFreq = [this] (float f) -> float
        {
            const float t = (std::log10 (juce::jlimit (kMinFreq, kMaxFreq, f)) - std::log10 (kMinFreq))
                         / (std::log10 (kMaxFreq) - std::log10 (kMinFreq));
            return plotBounds.getX() + t * plotBounds.getWidth();
        };

        // Marco
        g.setColour (juce::Colour (0xff2c2c3a));
        for (float f = 100.0f; f <= 10000.0f; f *= 10.0f)
        {
            const float x = xFromFreq (f);
            g.drawVerticalLine ((int) x, plotBounds.getY(), plotBounds.getBottom());
            g.setColour (juce::Colour (0xff707088));
            g.setFont (9.0f);
            juce::String label = (f >= 1000.0f) ? juce::String (f / 1000.0f, 0) + "k" : juce::String (f, 0);
            g.drawText (label, juce::Rectangle<float> (x - 14.0f, plotBounds.getBottom() + 1.0f, 28.0f, 10.0f),
                        juce::Justification::centred);
            g.setColour (juce::Colour (0xff2c2c3a));
        }

        // Bandas coloreadas (rectángulos semitransparentes en la parte inferior)
        const float bandBarH = 8.0f;
        const juce::Rectangle<float> bandBar (plotBounds.getX(), plotBounds.getBottom() - bandBarH,
                                               plotBounds.getWidth(), bandBarH);

        auto bandXRange = [this] (int b) -> std::pair<float, float>
        {
            float lo = xFromFreqB (kMinFreq);
            float hi;
            if (numBands == 1)      hi = xFromFreqB (kMaxFreq);
            else if (b == 0)         hi = xFromFreqB (crossovers[0]);
            else if (b == numBands - 1)
            {
                lo = xFromFreqB (crossovers[numBands - 2]);
                hi = xFromFreqB (kMaxFreq);
            }
            else
            {
                lo = xFromFreqB (crossovers[b - 1]);
                hi = xFromFreqB (crossovers[b]);
            }
            return { lo, hi };
        };

        for (int b = 0; b < numBands; ++b)
        {
            auto [lo, hi] = bandXRange (b);
            g.setColour (bandColors[b].withAlpha (0.55f));
            g.fillRoundedRectangle (juce::Rectangle<float> (lo + 1.0f, plotBounds.getY(), hi - lo - 2.0f, plotBounds.getHeight()), 3.0f);

            // Barra de GR (cuánto comprime esa banda, dibujada como barra vertical inversa)
            const float gr = juce::jlimit (0.0f, 20.0f, std::abs (bandGr[b]));
            const float barH = gr / 20.0f * plotBounds.getHeight() * 0.6f;
            g.setColour (bandColors[b]);
            g.fillRoundedRectangle (juce::Rectangle<float> ((lo + hi) * 0.5f - 4.0f,
                                                            plotBounds.getBottom() - barH - 2.0f,
                                                            8.0f, barH), 2.0f);

            // Etiqueta de banda
            g.setColour (juce::Colour (0xffffffff));
            g.setFont (juce::Font (juce::FontOptions (10.0f)));
            g.drawText (juce::String (b + 1), juce::Rectangle<float> (lo + 2.0f, plotBounds.getY() + 2.0f, 14.0f, 12.0f),
                        juce::Justification::centredLeft);
        }

        // Handles de crossover (draggable)
        for (int i = 0; i < numBands - 1; ++i)
        {
            const float x = xFromFreqB (crossovers[i]);
            g.setColour (juce::Colour (0xfff5a623));
            g.drawVerticalLine ((int) x, plotBounds.getY(), plotBounds.getBottom());

            // Triangulito handle
            juce::Path handle;
            handle.addTriangle (x - 5.0f, plotBounds.getY() - 4.0f,
                                x + 5.0f, plotBounds.getY() - 4.0f,
                                x,       plotBounds.getY() + 4.0f);
            g.setColour (crossoverDrags[i] ? juce::Colour (0xffffc060) : juce::Colour (0xfff5a623));
            g.fillPath (handle);

            // Etiqueta de frecuencia
            g.setColour (juce::Colour (0xffe5e5e5));
            g.setFont (9.0f);
            const juce::String f = (crossovers[i] >= 1000.0f)
                                 ? juce::String (crossovers[i] / 1000.0f, 1) + "k"
                                 : juce::String (crossovers[i], 0) + "Hz";
            g.drawText (f, juce::Rectangle<float> (x - 24.0f, plotBounds.getY() - 18.0f, 48.0f, 10.0f),
                        juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (plotBounds.getWidth() < 2.0f || plotBounds.getHeight() < 2.0f) return;
        const float x = (float) e.x;
        for (int i = 0; i < numBands - 1; ++i)
        {
            const float cx = xFromFreqB (crossovers[i]);
            if (std::abs (x - cx) < 8.0f)
            {
                crossoverDrags[i] = true;
                repaint();
                return;
            }
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (plotBounds.getWidth() < 2.0f || plotBounds.getHeight() < 2.0f) return;
        const float x = (float) e.x;
        for (int i = 0; i < numBands - 1; ++i)
        {
            if (crossoverDrags[i])
            {
                // Convertir x → frecuencia (log)
                const float t = (x - plotBounds.getX()) / plotBounds.getWidth();
                float freq = std::pow (10.0f, std::log10 (kMinFreq) + t * (std::log10 (kMaxFreq) - std::log10 (kMinFreq)));
                freq = juce::jlimit (kMinFreq, kMaxFreq, freq);
                if (std::isfinite (freq))
                {
                    crossovers[i] = freq;
                    if (onCrossoverChanged) onCrossoverChanged (i, freq);
                }
                repaint();
                return;
            }
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        std::fill (crossoverDrags.begin(), crossoverDrags.end(), false);
        repaint();
    }

private:
    float xFromFreqB (float f) const
    {
        const float t = (std::log10 (juce::jlimit (kMinFreq, kMaxFreq, f)) - std::log10 (kMinFreq))
                     / (std::log10 (kMaxFreq) - std::log10 (kMinFreq));
        return plotBounds.getX() + t * plotBounds.getWidth();
    }

    int numBands = 1;
    float crossovers[3] { 150.0f, 1200.0f, 6000.0f };
    float bandGr[4] { 0.0f, 0.0f, 0.0f, 0.0f };
    juce::Colour bandColors[4]
    {
        juce::Colour (0xff4a9eff),  // azul
        juce::Colour (0xff3acc5a),  // verde
        juce::Colour (0xfff5a623),  // naranja
        juce::Colour (0xffff5050)   // rojo
    };
    std::vector<bool> crossoverDrags { false, false, false };
    juce::Rectangle<float> plotBounds;
};
