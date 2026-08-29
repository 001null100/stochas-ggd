#pragma once

#include "GgdDrumGridV1.h"

class GgdDrumGridFinal final : public GgdDrumGridV1
{
public:
    using GgdDrumGridV1::GgdDrumGridV1;

    void setPlaybackEffects(bool enabled,
                            bool ripple,
                            bool laneFlash,
                            bool velocityReactive,
                            bool reduceMotion,
                            float intensity,
                            int decayMs);

    void paint(juce::Graphics& g) override;

private:
    struct PlayedPulse
    {
        int storageRow = -1;
        int tick = -1;
        int velocity = 100;
        double startedMs = 0.0;
    };

    std::vector<PlayedPulse> playedPulses;
    bool playedEffectsEnabled = true;
    bool playedRippleEnabled = true;
    bool playedLaneFlashEnabled = true;
    bool playedVelocityReactive = true;
    bool reduceMotion = false;
    float playedEffectIntensity = 0.80f;
    int playedEffectDecayMs = 320;

    void drainPlayedEvents();
    void triggerPlayedEvent(int storageRow, int tick, int velocity);
    void paintPlaybackEffects(juce::Graphics& g);
};
