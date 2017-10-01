/*
    This file is part of Helio Workstation.

    Helio is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Helio is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Helio. If not, see <http://www.gnu.org/licenses/>.
*/

#include "Common.h"
#include "GenericAudioMonitorComponent.h"
#include "AudioMonitor.h"
#include "AudioCore.h"

#define HQ_METER_MAXDB (+4.0f)
#define HQ_METER_MINDB (-70.0f)
#define HQ_METER_DECAY_RATE1 (1.0f - 3E-5f)
#define HQ_METER_DECAY_RATE2 (1.0f - 3E-7f)
#define HQ_METER_CYCLES_BEFORE_PEAK_FALLOFF 20

#define HQ_METER_NUM_BANDS 12

static const float kSpectrumFrequencies[] =
{
    31.5f,
    50.f, 80.f,
    125.f, 200.f,
    315.f, 500.f,
    800.f, 1250.f,
    2000.f, 3150.f,
    5000.f, 8000.f
};

GenericAudioMonitorComponent::GenericAudioMonitorComponent(WeakReference<AudioMonitor> monitor)
    : Thread("Spectrum Component"),
      audioMonitor(std::move(monitor)),
      skewTime(0)
{
    // (true, false) will enable switching rendering modes on click
    this->setInterceptsMouseClicks(false, false);

    for (int band = 0; band < HQ_METER_NUM_BANDS; ++band)
    {
        this->bands.add(new SpectrumBand());
    }
    
    if (this->audioMonitor != nullptr)
    {
        this->startThread(5);
    }
}

void GenericAudioMonitorComponent::setTargetAnalyzer(WeakReference<AudioMonitor> monitor)
{
    if (monitor != nullptr)
    {
        this->audioMonitor = monitor;
        this->startThread(5);
    }
}

GenericAudioMonitorComponent::~GenericAudioMonitorComponent()
{ 
    this->stopThread(1000);
}

void GenericAudioMonitorComponent::run()
{
    while (! this->threadShouldExit())
    {
        Thread::sleep(jlimit(10, 100, 35 - this->skewTime));
        const double b = Time::getMillisecondCounterHiRes();

        // TODO collect data??

        this->triggerAsyncUpdate();
        const double a = Time::getMillisecondCounterHiRes();
        this->skewTime = int(a - b);
    }
}

void GenericAudioMonitorComponent::handleAsyncUpdate()
{
    this->repaint();
}

void GenericAudioMonitorComponent::resized()
{
    for (int i = 0; i < HQ_METER_NUM_BANDS; ++i)
    {
        this->bands[i]->reset();
        //this->bands[i]->setDashedLineMode(true);
    }
}

void GenericAudioMonitorComponent::paint(Graphics &g)
{
    if (this->audioMonitor == nullptr)
    {
        return;
    }
    
    const int width = this->getWidth();
    const int height = this->getHeight();
    
    Image img(Image::ARGB, width, height, true);

    {
        Graphics g1(img);

        const float size = float(width) / float(HQ_METER_NUM_BANDS);
        
        for (int i = 0; i < HQ_METER_NUM_BANDS; ++i)
        {
            g1.setColour(Colours::white.withAlpha(0.25f));
            const float x = float((i * size) + 1);
            const float v = this->audioMonitor->getInterpolatedSpectrumAtFrequency(kSpectrumFrequencies[i]);
            this->bands[i]->setValue(v);
            this->bands[i]->drawBand(g1, x, 0.f, (size - 2.f), float(height));
        }

        // Show levels?
        //if (this->altMode)
        //{
        //    const int valueForMinus6dB = AudioCore::iecLevel(-6.f) * this->getHeight();
        //    const int valueForMinus12dB = AudioCore::iecLevel(-12.f) * this->getHeight();
        //    const int valueForMinus24dB = AudioCore::iecLevel(-24.f) * this->getHeight();
        //    
        //    g1.setColour(Colours::white.withAlpha(0.075f));
        //    g1.drawHorizontalLine(height - valueForMinus6dB, 0.f, this->getWidth());
        //    g1.drawHorizontalLine(height - valueForMinus12dB, 0.f, this->getWidth());
        //    g1.drawHorizontalLine(height - valueForMinus24dB, 0.f, this->getWidth());
        //}
    }

    // TODO draw peak levels:

    //{
    //    Graphics g1(img);

    //    this->peakBand.setValue(this->volumeAnalyzer->getPeak(this->channel));
    //    const float left = (this->orientation == Left) ? 0.f : w;
    //    const float right = w - left;
    //    this->peakBand.drawBand(g1, left, right, h);
    //}

    //ColourGradient cg(Colours::red, 0.f, 0.f,
    //    Colours::white.withAlpha(0.2f), 0.f, h,
    //    false);

    //cg.addColour(0.05f, Colours::red);
    //cg.addColour(0.06f, Colours::yellow);
    //cg.addColour(0.21f, Colours::white.withAlpha(0.2f));

    //FillType brush3(cg);
    //g.setFillType(brush3);

    // fillAlphaChannelWithCurrentBrush=true is evil, takes up to 9% of CPU on rendering
    g.drawImageAt(img, 0, 0, false);
}

GenericAudioMonitorComponent::SpectrumBand::SpectrumBand() :
    value(0.0f),
    valueHold(0.0f),
    valueDecay(HQ_METER_DECAY_RATE1),
    peak(0.0f),
    peakHold(0.0f),
    peakDecay(HQ_METER_DECAY_RATE2),
    maxPeak(0.0f),
    averagePeak(0.0f),
    drawsDashedLine(true)
{
}

void GenericAudioMonitorComponent::SpectrumBand::setDashedLineMode(bool shouldDrawDashedLine)
{
    this->drawsDashedLine = shouldDrawDashedLine;
}

void GenericAudioMonitorComponent::SpectrumBand::setValue(float value)
{
    this->value = value;
}

void GenericAudioMonitorComponent::SpectrumBand::reset()
{
    this->value = 0.0f;
    this->valueHold = 0.0f;
    this->valueDecay = HQ_METER_DECAY_RATE1;
    this->peak = 0.0f;
    this->peakHold = 0.0f;
    this->peakDecay = HQ_METER_DECAY_RATE2;
    this->maxPeak = 0.0f;
    this->averagePeak = 0.0f;
    this->drawsDashedLine = true;
}

inline void GenericAudioMonitorComponent::SpectrumBand::drawBand(Graphics &g, float xx, float yy, float w, float h)
{
    const float vauleInDb = jlimit(HQ_METER_MINDB, HQ_METER_MAXDB, 20.0f * AudioCore::fastLog10(this->value));
    float valueInY = float(AudioCore::iecLevel(vauleInDb) * h);
    
    if (this->valueHold < valueInY)
    {
        this->valueHold = valueInY;
        this->valueDecay = HQ_METER_DECAY_RATE1;
    }
    else
    {
        this->valueHold = int(this->valueHold * this->valueDecay);
        
        if (this->valueHold < valueInY)
        {
            this->valueHold = valueInY;
        }
        else
        {
            this->valueDecay = this->valueDecay * this->valueDecay; // * this->valueDecay;
            valueInY = this->valueHold;
        }
    }
    
    if (this->peak < valueInY)
    {
        this->peak = valueInY;
        this->peakHold = 0.0;
        this->peakDecay = HQ_METER_DECAY_RATE2;
    }
    else if (++this->peakHold > HQ_METER_CYCLES_BEFORE_PEAK_FALLOFF)
    {
        this->peak = this->peak * this->peakDecay;
        this->peakDecay = this->peakDecay * this->peakDecay; // * this->peakDecay;
    }
    
    this->maxPeak = jmax(this->maxPeak, this->peak);
    this->averagePeak = 0.0f; // todo!
    
    if (this->drawsDashedLine)
    {
        for (int i = int(h + 1); i > float(h - valueInY); i -= 2)
        {
            g.drawHorizontalLine(i, xx, xx + w);
        }
        
        const float peakH = (h - this->peak - 1.f);
        g.drawHorizontalLine(int(peakH), xx, xx + w);
    }
    else
    {
        g.fillRect(xx, h - valueInY, w, valueInY);

        const float peakH = (h - this->peak - .5f);
        g.drawLine(xx, peakH, xx + w, peakH);
    }
}
