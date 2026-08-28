#include "StochaEngine.h"
#include <algorithm>
#include <cmath>

// Compile the inherited cell scheduler unchanged as a private fallback. The
// header is included before aliasing so only the legacy definition is renamed.
#define processBlock processLegacyBlock
#include "StochaEngine.cpp"
#undef processBlock

bool StochaEngine::processBlock(double beatPosition,
                                double sampleRate,
                                int numSamplesInBlock,
                                double BPM,
                                double bpb
#ifdef CUBASE_HACKS
                                , double beatPositionActual
#endif
                                )
{
    auto* sequence = mSeq->getAudSeqData();
    auto* layer = sequence->getLayer(mLayer);
    const int patternIndex = mOverridePattern.get(layer->getCurrentPattern());
    const auto* pattern = layer->getEventPattern(patternIndex);

    if (pattern != nullptr && pattern->isActive())
    {
        return processEventBlock(beatPosition, sampleRate, numSamplesInBlock, BPM, bpb
#ifdef CUBASE_HACKS
                                 , beatPositionActual
#endif
        );
    }

    return processLegacyBlock(beatPosition, sampleRate, numSamplesInBlock, BPM, bpb
#ifdef CUBASE_HACKS
                              , beatPositionActual
#endif
    );
}

bool StochaEngine::processEventBlock(double beatPosition,
                                     double sampleRate,
                                     int numSamplesInBlock,
                                     double BPM,
                                     double
#ifdef CUBASE_HACKS
                                     , double
#endif
                                     )
{
    if (mSeq == nullptr || sampleRate <= 0.0 || BPM <= 0.0 || numSamplesInBlock <= 0)
        return true;

    auto* sequence = mSeq->getAudSeqData();
    auto* layer = sequence->getLayer(mLayer);
    const int patternIndex = mOverridePattern.get(layer->getCurrentPattern());
    const auto* pattern = layer->getEventPattern(patternIndex);
    if (pattern == nullptr || !pattern->isActive())
        return true;

    const int patternLength = pattern->getLengthTicks();
    if (patternLength <= 0)
        return true;

    const double clockScale = std::max(
        1.0 / 16.0,
        static_cast<double>(mOverrideSpeed.get(layer->getClockDivider()))
            / static_cast<double>(SEQ_CLOCK_DENOM));
    const double ticksPerQuarter = static_cast<double>(GGD_EVENT_PPQ) * clockScale;
    const double ticksPerSample = (BPM / 60.0) * ticksPerQuarter / sampleRate;
    const double samplesPerTick = 1.0 / ticksPerSample;

    double blockStartTick = (beatPosition - mPlayStartPosition) * ticksPerQuarter;
    while (blockStartTick < 0.0)
        blockStartTick += static_cast<double>(patternLength);
    const double blockEndTick = blockStartTick + numSamplesInBlock * ticksPerSample;

    if (mOldEventTickPosition >= 0.0)
    {
        const double discontinuity = std::abs(blockStartTick - mOldEventTickPosition);
        const double tolerance = std::max(4.0, (blockEndTick - blockStartTick) * 2.0);
        if (discontinuity > tolerance)
            quiesceMidi(false);
    }

    mOldEventTickPosition = blockEndTick;

    double localTick = std::fmod(blockStartTick, static_cast<double>(patternLength));
    if (localTick < 0.0)
        localTick += patternLength;
    mRealStepPosition = localTick / static_cast<double>(GGD_TICKS_PER_16TH);
    mCurrentStepPosition = static_cast<int>(std::floor(mRealStepPosition));

    if (getMuteState())
        return true;

    const int swing = mOverrideSwing.get(sequence->getSwing());
    const int humanVelocity = mOverrideVeloVariance.get(layer->getHumanVelocity());
    const int humanLength = mOverrideLengthVariance.get(layer->getHumanLength());
    const double duty = std::max(
        0.01, static_cast<double>(mOverrideDutyCycle.get(layer->getDutyCycle())) / 100.0);
    const int channel = juce::jlimit(
        1, 16, mOverrideOutputChannel.get(layer->getMidiChannel()));

    bool success = true;
    for (int eventIndex = 0; eventIndex < pattern->getEventCount(); ++eventIndex)
    {
        const auto* event = pattern->getEvent(eventIndex);
        if (event == nullptr || event->probability < 0)
            continue;

        double timingShiftTicks = 0.0;
        if ((event->tick % GGD_TICKS_PER_16TH) == 0)
        {
            const int sixteenth = event->tick / GGD_TICKS_PER_16TH;
            int grooveAmount = 0;
            if (swing != 0)
            {
                if ((sixteenth & 1) != 0)
                    grooveAmount = swing;
            }
            else
            {
                grooveAmount = sequence->getGroove(sixteenth % SEQ_DEFAULT_NUM_STEPS);
            }
            timingShiftTicks = static_cast<double>(grooveAmount)
                             * GGD_TICKS_PER_16TH / 100.0;
        }

        const double eventTick = static_cast<double>(event->tick) + timingShiftTicks;
        double cycle = std::floor((blockStartTick - eventTick)
                                  / static_cast<double>(patternLength));
        double occurrence = cycle * patternLength + eventTick;
        while (occurrence < blockStartTick - 1.0e-7)
            occurrence += patternLength;

        while (occurrence < blockEndTick - 1.0e-7)
        {
            const int randomPosition = static_cast<int>(std::fmod(
                std::abs(occurrence) + eventIndex * 131.0, 2147483000.0));
            mRand.prepareSeqPosition(randomPosition);

            const int probability = juce::jlimit(0, 100, static_cast<int>(event->probability));
            const bool shouldPlay = probability >= 100
                || (probability > 0 && (1 + mRand.getNextRandom(100)) <= probability);

            if (shouldPlay)
            {
                int velocity = juce::jlimit(1, 127, static_cast<int>(event->velocity));
                if (humanVelocity > 0)
                {
                    const int variation = mRand.getNextRandom(humanVelocity * 2 + 1)
                                        - humanVelocity;
                    velocity = juce::jlimit(1, 127, velocity + variation * 127 / 100);
                }

                int durationSamples = std::max(
                    1, static_cast<int>(std::llround(
                        static_cast<double>(event->durationTicks) * samplesPerTick * duty)));
                if (humanLength > 0)
                {
                    const int reduction = 1 + mRand.getNextRandom(humanLength);
                    durationSamples = std::max(
                        1, durationSamples - durationSamples * reduction / 100);
                }

                const int startSamples = juce::jlimit(
                    0,
                    juce::jmax(0, numSamplesInBlock - 1),
                    static_cast<int>(std::llround(
                        (occurrence - blockStartTick) * samplesPerTick)));

                const int8_t note = layer->getCurNote(event->row);
                if (note >= 0)
                {
                    if (!addMidiEvent(startSamples,
                                      note,
                                      static_cast<int8_t>(velocity),
                                      static_cast<int8_t>(channel),
                                      durationSamples))
                        success = false;
                }
            }

            occurrence += patternLength;
        }
    }

    return success;
}
