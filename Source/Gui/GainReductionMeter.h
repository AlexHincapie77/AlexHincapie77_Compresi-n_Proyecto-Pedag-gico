#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// ===========================================================================
//  GainReductionMeter  —  medidor de reducción de ganancia vertical,
//  estilo "aguja" clásico. Muestra en tiempo real cuántos dB está bajando
//  el compresor, con escala -24 dB … 0 dB, barra de color naranja y
//  valor numérico en grande debajo.
// ===========================================================================

class GainReductionMeter : public juce::Component,
                          private juce::Timer
{
public:
    GainReductionMeter()
    {
        startTimerHz(30);
    }

    ~GainReductionMeter() override { stopTimer(); }

    // Establece el valor de GR (siempre <= 0; 0 = sin compresión)
    void setGainReductionDb(float db)
    {
        // Mantén sólo el valor más negativo (mayor reducción) del bloque
        const float newVal = juce::jlimit(-24.0f, 0.0f, db);
        float cur = levelDb.load();
        // Decay suave: el meter cae lentamente cuando para la compresión
        if (newVal < cur)
            levelDb.store(newVal);
        else
            levelDb.store(cur + (newVal - cur) * 0.30f);  // release visual
        peakDb.store(juce::jmin(peakDb.load(), newVal));
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        const float currentDb = levelDb.load();
        const float peak = peakDb.load();

        // Fondo
        g.setColour(juce::Colour(0xff0f0f12));
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(juce::Colour(0xff2a2a2a));
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

        auto plot = bounds.reduced(8.0f, 10.0f);

        // Etiqueta superior — fuente grande y legible
        g.setColour(juce::Colour(0xffffc060));
        juce::Font labelFont(juce::FontOptions(14.0f));
        labelFont.setBold(true);
        g.setFont(labelFont);
        g.drawText("REDUCCIÓN", plot.toType<int>().removeFromTop(16),
                   juce::Justification::centred);

        // Área del medidor (debajo de "GR")
        auto meterArea = plot.withTrimmedTop(16).withTrimmedBottom(40);

        // Escala -24..0 dB
        auto dbToY = [&](float db) {
            float norm = juce::jlimit(0.0f, 1.0f, std::abs(db) / 24.0f);
            return meterArea.getBottom() - norm * meterArea.getHeight();
        };

        // Marcas de escala (-6, -12, -18, -24)
        g.setColour(juce::Colour(0xff404040));
        juce::Font tickFont(juce::FontOptions(9.0f));
        g.setFont(tickFont);
        for (int db = 0; db <= 24; db += 6)
        {
            float y = dbToY(-static_cast<float>(db));
            g.drawHorizontalLine(static_cast<int>(y), meterArea.getX(), meterArea.getRight());
            g.setColour(juce::Colour(0xff707070));
            g.drawText(juce::String(-db), 
                       static_cast<int>(meterArea.getRight() + 2),
                       static_cast<int>(y - 5), 24, 10,
                       juce::Justification::left);
            g.setColour(juce::Colour(0xff404040));
        }

        // Barra de GR (de abajo hacia arriba, naranja)
        float topY = dbToY(currentDb);
        auto barRect = meterArea.withY(topY).withHeight(meterArea.getBottom() - topY);
        // Gradiente: amarillo arriba (más compresión) → naranja abajo
        juce::ColourGradient cg;
        cg.point1 = barRect.getTopLeft();
        cg.point2 = barRect.getBottomLeft();
        cg.addColour(0.0, juce::Colour(0xffffe060));
        cg.addColour(1.0, juce::Colour(0xffff8030));
        g.setGradientFill(cg);
        g.fillRoundedRectangle(barRect, 2.0f);

        // Peak hold (línea blanca)
        float peakY = dbToY(peak);
        g.setColour(juce::Colours::white);
        g.fillRect(juce::Rectangle<float>(meterArea.getX(), peakY - 1.0f,
                                           meterArea.getWidth(), 2.0f));

        // Valor numérico grande debajo (más legible)
        juce::Font valFont(juce::FontOptions(22.0f));
        valFont.setBold(true);
        g.setFont(valFont);
        juce::String valText = (currentDb < -0.05f)
                                 ? juce::String(currentDb, 1) + " dB"
                                 : juce::String("0.0 dB");
        // Color: si pasa de -12 dB se vuelve rojo (estás aplastando)
        juce::Colour valColor = (currentDb < -12.0f) ? juce::Colour(0xffff5050)
                                  : (currentDb < -6.0f) ? juce::Colour(0xffffc060)
                                  : juce::Colour(0xff80ff80);
        g.setColour(valColor);
        g.drawText(valText,
                   bounds.toType<int>().withY(static_cast<int>(bounds.getBottom() - 32))
                        .withHeight(24),
                   juce::Justification::centred);

        // Etiqueta "GR" debajo del valor
        juce::Font labFont(juce::FontOptions(11.0f));
        g.setFont(labFont);
        g.setColour(juce::Colour(0xff909090));
        g.drawText("Ganancia", 
                   bounds.toType<int>().withY(static_cast<int>(bounds.getBottom() - 12))
                        .withHeight(10),
                   juce::Justification::centred);
    }

private:
    void timerCallback() override
    {
        // El peak decae lentamente
        float p = peakDb.load();
        if (p < -0.1f)
            peakDb.store(p + 0.4f);  // sube ~0.4 dB cada frame
        else
            peakDb.store(0.0f);
        repaint();
    }

    std::atomic<float> levelDb { 0.0f };
    std::atomic<float> peakDb  { 0.0f };
};
