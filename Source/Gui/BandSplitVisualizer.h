#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// ===========================================================================
//  BandSplitVisualizer  —  espectro 20Hz-20kHz con bandas coloreadas y
//  crossovers arrastrables
// ===========================================================================

class BandSplitVisualizer : public juce::Component
{
public:
    BandSplitVisualizer()
    {
        setOpaque(false);
        // Sin timer propio: el editor llama a updateLevels() / repaint() desde el suyo
    }

    void setNumBands(int n)
    {
        n = juce::jlimit(1, 4, n);
        if (n != numBands)
        {
            numBands = n;
            crossoverDrags.assign(n - 1, false);
            repaint();
        }
    }

    void setCrossover(int idx, float freqHz)
    {
        if (idx >= 0 && idx < 3)
        {
            crossoverFreqs[idx] = freqHz;
            repaint();
        }
    }

    void setBandGainReduction(int idx, float grDb)
    {
        if (idx >= 0 && idx < 4)
        {
            bandGR[idx] = grDb;
            repaint();
        }
    }

    // Callback llamado cuando el usuario arrastra un crossover
    std::function<void(int idx, float newFreqHz)> onCrossoverChanged;

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);

        // Fondo
        g.setColour(juce::Colour(0xff0f0f12));
        g.fillRoundedRectangle(bounds, 6.0f);

        // Colores por banda
        static const juce::Colour bandColours[] = {
            juce::Colour(0xff4a9eff),
            juce::Colour(0xff50ff80),
            juce::Colour(0xffffc060),
            juce::Colour(0xffff5050)
        };

        // Plot
        auto plotBounds = bounds.reduced(8.0f);

        // Función log: 20Hz → 20kHz → 0..1
        auto freqToX = [&](float freq) {
            float logMin = std::log10(20.0f);
            float logMax = std::log10(20000.0f);
            float f = (std::log10(juce::jlimit(20.0f, 20000.0f, freq)) - logMin) / (logMax - logMin);
            return plotBounds.getX() + f * plotBounds.getWidth();
        };

        // Dibuja bandas coloreadas con un degradado vertical sutil
        float prevX = plotBounds.getX();
        for (int b = 0; b < numBands; ++b)
        {
            float nextX;
            if (b < numBands - 1)
                nextX = freqToX(crossoverFreqs[b]);
            else
                nextX = plotBounds.getRight();

            juce::Colour c = bandColours[b % 4];
            juce::ColourGradient grad;
            grad.isRadial = false;
            auto bandRect = juce::Rectangle<float>(prevX, plotBounds.getY(),
                                                      nextX - prevX, plotBounds.getHeight());
            grad.point1 = { bandRect.getX(), bandRect.getY() };
            grad.point2 = { bandRect.getX(), bandRect.getBottom() };
            grad.addColour(0.0, c.withAlpha(0.35f));
            grad.addColour(1.0, c.withAlpha(0.10f));
            g.setGradientFill(grad);
            g.fillRect(bandRect);

            // GR overlay (más oscuro = más compresión)
            float gr = bandGR[b];
            if (gr < -0.1f)
            {
                float grNorm = juce::jlimit(0.0f, 1.0f, std::abs(gr) / 18.0f);
                g.setColour(juce::Colours::black.withAlpha(grNorm * 0.4f));
                g.fillRect(bandRect);
            }

            prevX = nextX;
        }

        // Líneas de crossover + etiquetas
        for (int i = 0; i < numBands - 1; ++i)
        {
            float x = freqToX(crossoverFreqs[i]);
            g.setColour(crossoverDrags[i] ? juce::Colour(0xffffc060) : juce::Colour(0xfff5a623));
            g.drawVerticalLine(static_cast<int>(x), plotBounds.getY(), plotBounds.getBottom());

            // Triangle marker arriba
            juce::Path tri;
            tri.addTriangle(x - 5, plotBounds.getY() - 1,
                             x + 5, plotBounds.getY() - 1,
                             x, plotBounds.getY() + 6);
            g.fillPath(tri);

            // Freq label
            juce::String label;
            float f = crossoverFreqs[i];
            if (f < 1000.0f) label = juce::String(static_cast<int>(f)) + " Hz";
            else label = juce::String(f / 1000.0f, 2) + " kHz";

            juce::FontOptions fo(10.0f);
            g.setFont(juce::Font(fo));
            g.setColour(juce::Colours::white);
            g.drawText(label, x - 30, plotBounds.getY() + 6, 60, 12, juce::Justification::centred);
        }

        // Marco
        g.setColour(juce::Colour(0xff2a2a2a));
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

        // Etiquetas de bandas
        juce::FontOptions fo(11.0f);
        g.setFont(juce::Font(fo));
        g.setColour(juce::Colours::lightgrey);
        g.drawText("20 Hz", plotBounds.getX(), plotBounds.getBottom() - 14, 50, 12, juce::Justification::left);
        g.drawText("20 kHz", plotBounds.getRight() - 50, plotBounds.getBottom() - 14, 50, 12, juce::Justification::right);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (numBands <= 1) return;

        auto bounds = getLocalBounds().toFloat().reduced(10.0f);
        auto plotBounds = bounds.reduced(8.0f);

        auto freqToX = [&](float freq) {
            float logMin = std::log10(20.0f);
            float logMax = std::log10(20000.0f);
            float f = (std::log10(juce::jlimit(20.0f, 20000.0f, freq)) - logMin) / (logMax - logMin);
            return plotBounds.getX() + f * plotBounds.getWidth();
        };

        float bestDist = 12.0f;
        int bestIdx = -1;
        for (int i = 0; i < numBands - 1; ++i)
        {
            float x = freqToX(crossoverFreqs[i]);
            float d = std::abs(e.position.getX() - x);
            if (d < bestDist) { bestDist = d; bestIdx = i; }
        }

        if (bestIdx >= 0)
        {
            for (int i = 0; i < (int)crossoverDrags.size(); ++i)
                crossoverDrags[i] = false;
            crossoverDrags[bestIdx] = true;
            repaint();
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        for (size_t i = 0; i < crossoverDrags.size(); ++i)
        {
            if (crossoverDrags[i])
            {
                auto bounds = getLocalBounds().toFloat().reduced(10.0f);
                auto plotBounds = bounds.reduced(8.0f);

                float f = (e.position.getX() - plotBounds.getX()) / plotBounds.getWidth();
                f = juce::jlimit(0.0f, 1.0f, f);
                float logMin = std::log10(20.0f);
                float logMax = std::log10(20000.0f);
                float freq = std::pow(10.0f, logMin + f * (logMax - logMin));

                // Limitar adyacente
                if (i > 0)
                    freq = juce::jmax(freq, crossoverFreqs[i - 1] + 50.0f);
                if (i < crossoverDrags.size() - 1)
                    freq = juce::jmin(freq, crossoverFreqs[i + 1] - 50.0f);

                crossoverFreqs[i] = freq;
                if (onCrossoverChanged)
                    onCrossoverChanged(static_cast<int>(i), freq);
                repaint();
            }
        }
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        for (size_t i = 0; i < crossoverDrags.size(); ++i)
            crossoverDrags[i] = false;
        repaint();
    }

private:
    int numBands = 1;
    float crossoverFreqs[3] = { 120.0f, 1200.0f, 6000.0f };
    std::vector<bool> crossoverDrags;  // OK iterar con auto&
    float bandGR[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};
