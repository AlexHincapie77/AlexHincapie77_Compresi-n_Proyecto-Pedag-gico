#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// ===========================================================================
//  LevelMeterComponent  —  medidor vertical simple (Input/Output/GR)
// ===========================================================================

class LevelMeterComponent : public juce::Component,
                             private juce::Timer
{
public:
    enum class Type { Input, Output, GainReduction };

    explicit LevelMeterComponent(Type t) : type(t)
    {
        startTimerHz(30);
    }

    ~LevelMeterComponent() override { stopTimer(); }

    void setLevelDb(float db)
    {
        juce::ScopedLock sl(lock);
        levelDb = db;
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        float currentDb;
        { juce::ScopedLock sl(lock); currentDb = levelDb; }

        // Fondo
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRoundedRectangle(bounds, 4.0f);

        // Determine level (0..1)
        float normalized = 0.0f;
        if (type == Type::GainReduction)
        {
            // GR: 0 dB arriba, -24 dB abajo
            normalized = juce::jlimit(0.0f, 1.0f, std::abs(currentDb) / 24.0f);
        }
        else
        {
            // Input/Output: -60 dB abajo, 0 dB arriba
            normalized = juce::jlimit(0.0f, 1.0f, (currentDb + 60.0f) / 60.0f);
        }

        // Banda de nivel
        auto barBounds = bounds.reduced(3.0f, 3.0f);
        float barHeight = barBounds.getHeight() * normalized;
        auto barRect = barBounds.withY(barBounds.getBottom() - barHeight).withHeight(barHeight);

        juce::Colour colour;
        if (type == Type::GainReduction)
            colour = juce::Colour(0xffffc060);
        else if (currentDb > -3.0f)
            colour = juce::Colour(0xffff5050);
        else if (currentDb > -12.0f)
            colour = juce::Colour(0xffffc060);
        else
            colour = juce::Colour(0xff50ff80);

        g.setColour(colour);
        g.fillRoundedRectangle(barRect, 2.0f);

        // Peak hold
        float peakY = 0.0f;
        if (type == Type::GainReduction)
            peakY = bounds.getY() + (1.0f - normalized) * bounds.getHeight();
        else
            peakY = bounds.getBottom() - normalized * bounds.getHeight();
        g.setColour(juce::Colours::white);
        g.fillRect(bounds.getX(), peakY - 1.0f, bounds.getWidth(), 2.0f);

        // Label
        g.setColour(juce::Colours::lightgrey);
        juce::FontOptions fo(11.0f);
        g.setFont(juce::Font(fo));
        juce::String label;
        if (type == Type::Input)         label = "IN";
        else if (type == Type::Output)   label = "OUT";
        else                              label = "GR";
        g.drawText(label, bounds.reduced(2.0f), juce::Justification::topLeft);
    }

private:
    void timerCallback() override { repaint(); }

    Type type;
    float levelDb = -100.0f;
    juce::CriticalSection lock;
};
