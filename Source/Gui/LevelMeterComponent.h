/*
 * LevelMeterComponent.h — sin timer propio
 * ─────────────────────────────────────────────────────────────────────────
 * El editor principal lo refresca con repaint() desde su propio timer.
 * Esto evita race conditions entre múltiples timers.
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>

class LevelMeterComponent : public juce::Component
{
public:
    enum class Type { Level, GainReduction };

    explicit LevelMeterComponent (Type t = Type::Level) : type (t) {}

    void setLevelDb (float db)
    {
        currentDb.store (db);
        if (db > peakDb.load())
            peakDb.store (db);
    }

    void setPeakHold (bool hold) { peakHold = hold; }
    void resetPeak() { peakDb.store (-100.0f); }

    // Llamado por el editor para que el peak-hold decaiga
    void tick()
    {
        float p = peakDb.load();
        if (peakHold && p > -99.0f)
            peakDb.store (p - 0.4f);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (2.0f);
        if (bounds.getWidth() <= 4.0f || bounds.getHeight() <= 4.0f) return;

        g.setColour (juce::Colour (0xff1a1a22));
        g.fillRoundedRectangle (bounds, 4.0f);

        const float top    = bounds.getY() + 4.0f;
        const float bottom = bounds.getBottom() - 4.0f;
        const float width  = bounds.getWidth() * 0.5f;
        const float cx     = bounds.getCentreX();

        const float minDb = -60.0f;
        const float maxDb = (type == Type::GainReduction) ? 0.0f : 6.0f;
        const float dbRange = maxDb - minDb;
        if (dbRange <= 0.0f) return;

        // Escala (líneas)
        g.setColour (juce::Colour (0xff2c2c3a));
        for (float db = minDb; db <= maxDb; db += 6.0f)
        {
            const float y = bottom - (db - minDb) / dbRange * (bottom - top);
            g.drawHorizontalLine ((int) y, bounds.getX() + 2.0f, bounds.getRight() - 2.0f);
        }

        // Etiquetas
        g.setColour (juce::Colour (0xff707088));
        g.setFont (juce::Font (juce::FontOptions (7.0f)));
        for (float db = minDb; db <= maxDb; db += 12.0f)
        {
            const float y = bottom - (db - minDb) / dbRange * (bottom - top);
            g.drawText (juce::String (db, 0), juce::Rectangle<float> (bounds.getX() + 2.0f, y - 4.0f, 18.0f, 8.0f),
                        juce::Justification::centredLeft);
        }

        // Nivel actual
        float db = currentDb.load();
        if (type == Type::GainReduction)
            db = juce::jlimit (minDb, 0.0f, db);
        else
            db = juce::jlimit (minDb, maxDb, db);

        if (! std::isfinite (db)) return;

        const float y = bottom - (db - minDb) / dbRange * (bottom - top);

        juce::Colour c;
        if (type == Type::GainReduction)
            c = juce::Colour (0xff4a9eff);
        else if (db > 0.0f)      c = juce::Colour (0xffff5050);
        else if (db > -6.0f)     c = juce::Colour (0xfff5a623);
        else                     c = juce::Colour (0xff3acc5a);

        g.setColour (c);
        g.fillRoundedRectangle (juce::Rectangle<float> (cx - width * 0.5f, y, width, bottom - y), 2.0f);

        // Peak hold
        if (peakHold)
        {
            float pdb = peakDb.load();
            if (type == Type::GainReduction) pdb = juce::jlimit (minDb, 0.0f, pdb);
            else                             pdb = juce::jlimit (minDb, maxDb, pdb);
            if (std::isfinite (pdb))
            {
                const float py = bottom - (pdb - minDb) / dbRange * (bottom - top);
                g.setColour (juce::Colour (0xfff5f5f5));
                g.drawHorizontalLine ((int) py, cx - width * 0.5f, cx + width * 0.5f);
            }
        }
    }

private:
    Type type;
    bool peakHold = true;
    std::atomic<float> currentDb { -100.0f };
    std::atomic<float> peakDb    { -100.0f };
};
