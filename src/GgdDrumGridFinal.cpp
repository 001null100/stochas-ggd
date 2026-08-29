#include "GgdDrumGridFinal.h"
#include "GgdUiTheme.h"

#include <algorithm>
#include <cmath>

void GgdDrumGridFinal::setPlaybackEffects(bool enabled,
                                          bool ripple,
                                          bool laneFlash,
                                          bool velocityReactive,
                                          bool shouldReduceMotion,
                                          float intensity,
                                          int decayMs)
{
    playedEffectsEnabled = enabled;
    playedRippleEnabled = ripple;
    playedLaneFlashEnabled = laneFlash;
    playedVelocityReactive = velocityReactive;
    reduceMotion = shouldReduceMotion;
    playedEffectIntensity = juce::jlimit(0.0f, 1.0f, intensity);
    playedEffectDecayMs = juce::jlimit(100, 900, decayMs);

    if (!playedEffectsEnabled)
        playedPulses.clear();

    repaint();
}

void GgdDrumGridFinal::drainPlayedEvents()
{
    int storageRow = -1;
    int tick = -1;
    int velocity = 0;
    int drained = 0;

    while (drained < 256
           && processor.popGgdPlayedEvent(0, &storageRow, &tick, &velocity))
    {
        if (playedEffectsEnabled)
            triggerPlayedEvent(storageRow, tick, velocity);
        ++drained;
    }
}

void GgdDrumGridFinal::triggerPlayedEvent(int storageRow, int tick, int velocity)
{
    if (!playedEffectsEnabled)
        return;

    PlayedPulse pulse;
    pulse.storageRow = storageRow;
    pulse.tick = tick;
    pulse.velocity = juce::jlimit(1, 127, velocity);
    pulse.startedMs = juce::Time::getMillisecondCounterHiRes();

    // Keep the visual queue bounded even under deliberately absurd dense rolls.
    // Audio scheduling never depends on this vector.
    if (playedPulses.size() >= 96)
        playedPulses.erase(playedPulses.begin(),
                           playedPulses.begin() + static_cast<std::ptrdiff_t>(playedPulses.size() - 95));

    playedPulses.push_back(pulse);
}

void GgdDrumGridFinal::paintPlaybackEffects(juce::Graphics& g)
{
    if (!playedEffectsEnabled || playedPulses.empty())
        return;

    const double now = juce::Time::getMillisecondCounterHiRes();
    const double decay = static_cast<double>(juce::jmax(100, playedEffectDecayMs));

    playedPulses.erase(
        std::remove_if(playedPulses.begin(), playedPulses.end(),
                       [now, decay](const PlayedPulse& pulse)
                       {
                           return now - pulse.startedMs >= decay;
                       }),
        playedPulses.end());

    if (playedPulses.empty())
        return;

    auto* viewport = findParentComponentOfClass<juce::Viewport>();
    const int viewX = currentViewX();
    const float visibleLeft = static_cast<float>(viewX + nameWidth);
    const float visibleRight = viewport != nullptr
        ? static_cast<float>(viewX + viewport->getViewWidth())
        : static_cast<float>(getWidth());
    const float visibleWidth = juce::jmax(0.0f, visibleRight - visibleLeft);

    const auto accent = ggdThemeColour(GgdThemeRole::accent);
    const auto secondary = ggdThemeColour(GgdThemeRole::accentSecondary);

    for (const auto& pulse : playedPulses)
    {
        const int canonicalRow = canonicalRowForStorage(pulse.storageRow);
        const int y = layoutYForCanonical(canonicalRow);
        if (canonicalRow < 0 || y < 0 || !eventOccupied(canonicalRow, pulse.tick))
            continue;

        const float progress = juce::jlimit(
            0.0f, 1.0f,
            static_cast<float>((now - pulse.startedMs) / decay));
        const float envelope = 1.0f - progress;
        const float velocityGain = playedVelocityReactive
            ? 0.36f + 0.64f * (static_cast<float>(pulse.velocity) / 127.0f)
            : 1.0f;
        const float gain = juce::jlimit(
            0.0f, 1.0f, playedEffectIntensity * velocityGain * envelope);
        if (gain <= 0.001f)
            continue;

        const float cx = visualHitCenterX(pulse.tick);
        const float cy = static_cast<float>(y) + rowHeight * 0.5f;

        if (playedLaneFlashEnabled && visibleWidth > 0.0f)
        {
            const float laneAlpha = juce::jlimit(0.0f, 0.13f, 0.095f * gain);
            g.setColour(secondary.withAlpha(laneAlpha));
            g.fillRect(juce::Rectangle<float>(
                visibleLeft, static_cast<float>(y + 1),
                visibleWidth, static_cast<float>(rowHeight - 2)));
        }

        if (cx < visibleLeft - 32.0f || cx > visibleRight + 32.0f)
            continue;

        const float staticRadius = juce::jlimit(7.0f, 18.0f, cellWidthPixels() * 0.28f + 6.0f);
        const float glowRadius = reduceMotion
            ? staticRadius
            : staticRadius + progress * 8.0f;

        g.setColour(accent.withAlpha(juce::jlimit(0.0f, 0.30f, 0.22f * gain)));
        g.fillEllipse(cx - glowRadius, cy - glowRadius,
                      glowRadius * 2.0f, glowRadius * 2.0f);

        g.setColour(ggdThemeColour(GgdThemeRole::text)
                        .withAlpha(juce::jlimit(0.0f, 0.34f, 0.27f * gain)));
        const float coreRadius = juce::jmax(3.0f, staticRadius * 0.38f);
        g.drawEllipse(cx - coreRadius, cy - coreRadius,
                      coreRadius * 2.0f, coreRadius * 2.0f, 1.2f);

        if (playedRippleEnabled && !reduceMotion)
        {
            const float rippleRadius = staticRadius + 4.0f + progress * 18.0f;
            const float rippleAlpha = juce::jlimit(0.0f, 0.46f, 0.40f * gain);
            g.setColour(secondary.withAlpha(rippleAlpha));
            g.drawEllipse(cx - rippleRadius, cy - rippleRadius,
                          rippleRadius * 2.0f, rippleRadius * 2.0f,
                          1.0f + 0.7f * envelope);
        }
    }
}

void GgdDrumGridFinal::paint(juce::Graphics& g)
{
    // The audio thread only writes compact notifications. Draining them here
    // keeps every effect operation safely on the message/UI thread.
    drainPlayedEvents();
    GgdDrumGridV1::paint(g);
    paintPlaybackEffects(g);
}
