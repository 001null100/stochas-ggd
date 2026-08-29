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
    playedEffectIntensity = juce::jlimit(0.0f, 1.5f, intensity);
    playedEffectDecayMs = juce::jlimit(100, 1400, decayMs);

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
    const auto text = ggdThemeColour(GgdThemeRole::text);

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
        const float fastEnvelope = envelope * envelope;
        const float slowEnvelope = std::sqrt(juce::jmax(0.0f, envelope));
        const float velocityGain = playedVelocityReactive
            ? 0.30f + 0.70f * (static_cast<float>(pulse.velocity) / 127.0f)
            : 1.0f;
        const float gain = juce::jlimit(
            0.0f, 1.5f, playedEffectIntensity * velocityGain);
        if (gain <= 0.001f)
            continue;

        const float cx = visualHitCenterX(pulse.tick);
        const float cy = static_cast<float>(y) + rowHeight * 0.5f;
        const float cell = cellWidthPixels();
        const float staticRadius = juce::jlimit(8.0f, 21.0f, cell * 0.30f + 7.0f);

        if (playedLaneFlashEnabled && visibleWidth > 0.0f)
        {
            const float laneAlpha = juce::jlimit(0.0f, 0.24f, 0.17f * gain * slowEnvelope);
            g.setColour(secondary.withAlpha(laneAlpha));
            g.fillRect(juce::Rectangle<float>(
                visibleLeft, static_cast<float>(y + 1),
                visibleWidth, static_cast<float>(rowHeight - 2)));

            if (cx >= visibleLeft - 80.0f && cx <= visibleRight + 80.0f)
            {
                const float streakWidth = juce::jlimit(
                    42.0f, 170.0f,
                    cell * 1.35f + 46.0f + progress * 34.0f);
                const float streakAlpha = juce::jlimit(
                    0.0f, 0.34f, 0.27f * gain * fastEnvelope);
                g.setColour(accent.withAlpha(streakAlpha));
                g.fillRoundedRectangle(
                    cx - streakWidth * 0.5f,
                    static_cast<float>(y + 3),
                    streakWidth,
                    static_cast<float>(juce::jmax(2, rowHeight - 6)),
                    4.0f);
            }
        }

        if (cx < visibleLeft - 56.0f || cx > visibleRight + 56.0f)
            continue;

        const float glowRadius = reduceMotion
            ? staticRadius
            : staticRadius + progress * 13.0f;
        const float outerRadius = glowRadius * 1.55f;
        const float innerRadius = juce::jmax(5.0f, glowRadius * 0.78f);

        g.setColour(secondary.withAlpha(
            juce::jlimit(0.0f, 0.18f, 0.13f * gain * slowEnvelope)));
        g.fillEllipse(cx - outerRadius, cy - outerRadius,
                      outerRadius * 2.0f, outerRadius * 2.0f);

        g.setColour(accent.withAlpha(
            juce::jlimit(0.0f, 0.42f, 0.32f * gain * envelope)));
        g.fillEllipse(cx - glowRadius, cy - glowRadius,
                      glowRadius * 2.0f, glowRadius * 2.0f);

        g.setColour(text.withAlpha(
            juce::jlimit(0.0f, 0.48f, 0.38f * gain * fastEnvelope)));
        g.fillEllipse(cx - innerRadius * 0.46f, cy - innerRadius * 0.46f,
                      innerRadius * 0.92f, innerRadius * 0.92f);

        g.setColour(text.withAlpha(
            juce::jlimit(0.0f, 0.56f, 0.43f * gain * envelope)));
        const float coreRadius = juce::jmax(3.5f, staticRadius * 0.42f);
        g.drawEllipse(cx - coreRadius, cy - coreRadius,
                      coreRadius * 2.0f, coreRadius * 2.0f,
                      1.2f + 0.8f * juce::jmin(1.0f, gain));

        if (!reduceMotion && progress < 0.42f)
        {
            const float impact = 1.0f - progress / 0.42f;
            const float flareAlpha = juce::jlimit(
                0.0f, 0.70f, 0.54f * gain * impact * impact);
            const float flareLength = staticRadius + 8.0f + 13.0f * (1.0f - impact);
            g.setColour(text.withAlpha(flareAlpha));
            g.drawLine(cx - flareLength, cy, cx - coreRadius - 2.0f, cy, 1.4f);
            g.drawLine(cx + coreRadius + 2.0f, cy, cx + flareLength, cy, 1.4f);
            g.drawLine(cx, cy - flareLength * 0.62f, cx, cy - coreRadius - 2.0f, 1.15f);
            g.drawLine(cx, cy + coreRadius + 2.0f, cx, cy + flareLength * 0.62f, 1.15f);
        }

        if (playedRippleEnabled && !reduceMotion)
        {
            const float rippleRadius = staticRadius + 5.0f + progress * 30.0f;
            const float rippleAlpha = juce::jlimit(
                0.0f, 0.62f, 0.50f * gain * envelope);
            g.setColour(secondary.withAlpha(rippleAlpha));
            g.drawEllipse(cx - rippleRadius, cy - rippleRadius,
                          rippleRadius * 2.0f, rippleRadius * 2.0f,
                          1.1f + 1.0f * fastEnvelope);

            if (progress > 0.16f)
            {
                const float delayedProgress = juce::jlimit(
                    0.0f, 1.0f, (progress - 0.16f) / 0.84f);
                const float delayedEnvelope = 1.0f - delayedProgress;
                const float secondRadius = staticRadius + 3.0f + delayedProgress * 43.0f;
                const float secondAlpha = juce::jlimit(
                    0.0f, 0.36f, 0.28f * gain * delayedEnvelope);
                g.setColour(accent.withAlpha(secondAlpha));
                g.drawEllipse(cx - secondRadius, cy - secondRadius,
                              secondRadius * 2.0f, secondRadius * 2.0f,
                              0.9f + 0.7f * delayedEnvelope);
            }
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
