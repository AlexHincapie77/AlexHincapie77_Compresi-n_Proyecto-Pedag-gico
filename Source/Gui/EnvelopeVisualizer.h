#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <deque>

// ===========================================================================
//  EnvelopeVisualizer
//  Muestra cómo es la curva de attack/release que está aplicando el
//  compresor en tiempo real.  Es un osciloscopio que dibuja el historial
//  de la señal de envolvente del detector (en dB) — así se ve claramente
//  si el ataque es lineal, exponencial, suave, agresivo, etc.
//
//  Inspirado en el gráfico de envolvente de Tokyo Dawn Kotelnikov y
//  FabFilter Pro-C.
// ===========================================================================

class EnvelopeVisualizer : public juce::Component,
                          private juce::Timer
{
public:
    EnvelopeVisualizer()
    {
        startTimerHz(60);
        // Pre-llena el historial
        for (int i = 0; i < kHistorySize; ++i)
        {
            inputHistory.push_back(-100.0f);
            envHistory.push_back(-100.0f);
            grHistory.push_back(0.0f);
        }
    }

    ~EnvelopeVisualizer() override { stopTimer(); }

    // Llamado desde el editor con el último valor medido (dB)
    void setInputLevelDb(float db)   { inputDb.store(db); }
    void setEnvelopeDb(float db)    { envDb.store(db); }
    void setGainReductionDb(float db){ grDb.store(db); }

    void setAttackMs(float ms)  { attackMs.store(ms); }
    void setReleaseMs(float ms) { releaseMs.store(ms); }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);

        // Fondo
        g.setColour(juce::Colour(0xff0f0f12));
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(juce::Colour(0xff2a2a2a));
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

        auto plot = bounds.reduced(8.0f, 8.0f);

        // Marco del plot con grid
        g.setColour(juce::Colour(0xff1a1a1f));
        g.fillRect(plot);
        g.setColour(juce::Colour(0xff2a2a2a));
        for (int i = 1; i < 4; ++i)
        {
            float y = plot.getY() + plot.getHeight() * i / 4.0f;
            g.drawHorizontalLine(static_cast<int>(y), plot.getX(), plot.getRight());
            float x = plot.getX() + plot.getWidth() * i / 4.0f;
            g.drawVerticalLine(static_cast<int>(x), plot.getY(), plot.getBottom());
        }

        // Etiquetas
        g.setColour(juce::Colour(0xff909090));
        juce::Font labFont(juce::FontOptions(10.0f));
        g.setFont(labFont);
        g.drawText("Envelope (dB)", 
                   static_cast<int>(plot.getX()),
                   static_cast<int>(plot.getY()) - 12, 100, 12,
                   juce::Justification::left);
        g.drawText("0 dB",
                   static_cast<int>(plot.getRight() - 30),
                   static_cast<int>(plot.getY()) + 2, 28, 10,
                   juce::Justification::right);
        g.drawText("-60 dB",
                   static_cast<int>(plot.getRight() - 36),
                   static_cast<int>(plot.getBottom() - 12), 36, 10,
                   juce::Justification::right);

        // Etiquetas de tiempo
        float aMs = attackMs.load();
        float rMs = releaseMs.load();
        g.setColour(juce::Colour(0xff4a9eff));
        g.drawText("Atk " + juce::String(aMs, 1) + " ms",
                   static_cast<int>(plot.getX() + 4),
                   static_cast<int>(plot.getBottom() + 2), 100, 12,
                   juce::Justification::left);
        g.setColour(juce::Colour(0xff50ff80));
        g.drawText("Rel " + juce::String(rMs, 1) + " ms",
                   static_cast<int>(plot.getX() + 110),
                   static_cast<int>(plot.getBottom() + 2), 100, 12,
                   juce::Justification::left);

        // Map dB (-60..0) → Y (bottom..top)
        auto dbToY = [&](float db) {
            float n = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
            return plot.getBottom() - n * plot.getHeight();
        };
        auto idxToX = [&](size_t i) {
            return plot.getX() + plot.getWidth() * i / float(kHistorySize - 1);
        };

        // Traza de input (azul tenue)
        juce::Path inputPath;
        {
            juce::ScopedLock sl(lock);
            for (size_t i = 0; i < inputHistory.size(); ++i)
            {
                float x = idxToX(i);
                float y = dbToY(inputHistory[i]);
                if (i == 0) inputPath.startNewSubPath(x, y);
                else inputPath.lineTo(x, y);
            }
        }
        g.setColour(juce::Colour(0xff4a9eff).withAlpha(0.5f));
        g.strokePath(inputPath, juce::PathStrokeType(1.0f));

        // Traza de envolvente (verde brillante)
        juce::Path envPath;
        {
            juce::ScopedLock sl(lock);
            for (size_t i = 0; i < envHistory.size(); ++i)
            {
                float x = idxToX(i);
                float y = dbToY(envHistory[i]);
                if (i == 0) envPath.startNewSubPath(x, y);
                else envPath.lineTo(x, y);
            }
        }
        g.setColour(juce::Colour(0xff50ff80));
        g.strokePath(envPath, juce::PathStrokeType(2.0f));

        // Traza de GR (naranja, invertida para visualizar)
        // GR es <= 0; lo mostramos como offset hacia abajo desde 0 dB
        juce::Path grPath;
        {
            juce::ScopedLock sl(lock);
            for (size_t i = 0; i < grHistory.size(); ++i)
            {
                float x = idxToX(i);
                // El GR se muestra como una segunda curva por debajo del 0
                // mapeada -24..0 dB → mitad inferior del plot
                float gr = grHistory[i];  // típicamente 0..-24
                float n = juce::jlimit(0.0f, 1.0f, std::abs(gr) / 24.0f);
                float y = plot.getBottom() - n * (plot.getHeight() * 0.5f);
                if (i == 0) grPath.startNewSubPath(x, y);
                else grPath.lineTo(x, y);
            }
        }
        g.setColour(juce::Colour(0xffffc060));
        g.strokePath(grPath, juce::PathStrokeType(1.5f));

        // Leyenda
        auto leg = bounds.removeFromBottom(12);
        g.setColour(juce::Colour(0xff4a9eff).withAlpha(0.7f));
        g.fillRect(leg.getX() + 4.0f, leg.getY() + 2.0f, 10.0f, 4.0f);
        g.setColour(juce::Colours::lightgrey);
        g.drawText("Input", leg.getX() + 18, leg.getY() - 2, 50, 12, juce::Justification::left);

        g.setColour(juce::Colour(0xff50ff80));
        g.fillRect(leg.getX() + 70.0f, leg.getY() + 2.0f, 10.0f, 4.0f);
        g.setColour(juce::Colours::lightgrey);
        g.drawText("Envelope", leg.getX() + 84, leg.getY() - 2, 60, 12, juce::Justification::left);

        g.setColour(juce::Colour(0xffffc060));
        g.fillRect(leg.getX() + 150.0f, leg.getY() + 2.0f, 10.0f, 4.0f);
        g.setColour(juce::Colours::lightgrey);
        g.drawText("GR", leg.getX() + 164, leg.getY() - 2, 20, 12, juce::Justification::left);
    }

private:
    void timerCallback() override
    {
        // Push al historial
        juce::ScopedLock sl(lock);
        inputHistory.push_back(inputDb.load());
        envHistory.push_back(envDb.load());
        grHistory.push_back(grDb.load());
        if (inputHistory.size() > kHistorySize) inputHistory.pop_front();
        if (envHistory.size() > kHistorySize) envHistory.pop_front();
        if (grHistory.size() > kHistorySize) grHistory.pop_front();
        repaint();
    }

    static constexpr size_t kHistorySize = 256;

    std::atomic<float> inputDb { -100.0f };
    std::atomic<float> envDb  { -100.0f };
    std::atomic<float> grDb   { 0.0f };
    std::atomic<float> attackMs { 10.0f };
    std::atomic<float> releaseMs { 100.0f };

    std::deque<float> inputHistory;
    std::deque<float> envHistory;
    std::deque<float> grHistory;
    juce::CriticalSection lock;
};
