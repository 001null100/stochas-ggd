#include "GgdEventPersist.h"

namespace
{
constexpr const char* rootTag = "ggd-event-state";

// Version 1 is understood by every Stochas GGD release. New information is
// added as optional attributes so older builds can still open newer projects.
constexpr int stateVersion = 1;

const char* syncModeToken(GgdTimeline::SyncMode mode)
{
    return mode == GgdTimeline::SyncMode::free ? "free" : "bars";
}

GgdTimeline::SyncMode syncModeFromToken(const juce::String& token)
{
    return token == "free" ? GgdTimeline::SyncMode::free : GgdTimeline::SyncMode::hostBars;
}
}

void GgdEventPersist::store(const SequenceData& sequence, juce::XmlElement& root)
{
    auto* state = new juce::XmlElement(rootTag);
    state->setAttribute("version", stateVersion);
    state->setAttribute("ppq", GGD_EVENT_PPQ);
    state->setAttribute("sync", syncModeToken(sequence.getSyncMode()));

    for (int layerIndex = 0; layerIndex < SEQ_MAX_LAYERS; ++layerIndex)
    {
        const auto* layer = sequence.getLayer(layerIndex);
        auto layerElement = std::make_unique<juce::XmlElement>("layer");
        layerElement->setAttribute("idx", layerIndex);
        bool havePattern = false;

        for (int patternIndex = 0; patternIndex < SEQ_MAX_PATTERNS; ++patternIndex)
        {
            const auto* pattern = layer->getEventPattern(patternIndex);
            if (pattern == nullptr || !pattern->isActive())
                continue;

            havePattern = true;
            auto* patternElement = new juce::XmlElement("pattern");
            patternElement->setAttribute("idx", patternIndex);
            patternElement->setAttribute("num", pattern->getNumerator());
            patternElement->setAttribute("den", pattern->getDenominator());
            patternElement->setAttribute("bars", pattern->getBars());

            for (int eventIndex = 0; eventIndex < pattern->getEventCount(); ++eventIndex)
            {
                const auto* event = pattern->getEvent(eventIndex);
                auto* eventElement = new juce::XmlElement("event");
                eventElement->setAttribute("tick", event->tick);
                eventElement->setAttribute("row", static_cast<int>(event->row));
                eventElement->setAttribute("velocity", static_cast<int>(event->velocity));
                eventElement->setAttribute("probability", static_cast<int>(event->probability));
                eventElement->setAttribute("duration", static_cast<int>(event->durationTicks));
                patternElement->addChildElement(eventElement);
            }

            layerElement->addChildElement(patternElement);
        }

        if (havePattern)
            state->addChildElement(layerElement.release());
    }

    root.addChildElement(state);
}

bool GgdEventPersist::retrieve(SequenceData& sequence, const juce::XmlElement& root)
{
    const auto* state = root.getChildByName(rootTag);
    if (state == nullptr)
        return false;

    if (state->getIntAttribute("version", -1) != stateVersion)
        return false;

    sequence.setSyncMode(syncModeFromToken(state->getStringAttribute("sync", "bars")));

    for (auto* layerElement : state->getChildWithTagNameIterator("layer"))
    {
        const int layerIndex = layerElement->getIntAttribute("idx", -1);
        if (layerIndex < 0 || layerIndex >= SEQ_MAX_LAYERS)
            continue;

        auto* layer = sequence.getLayer(layerIndex);
        for (auto* patternElement : layerElement->getChildWithTagNameIterator("pattern"))
        {
            const int patternIndex = patternElement->getIntAttribute("idx", -1);
            if (patternIndex < 0 || patternIndex >= SEQ_MAX_PATTERNS)
                continue;

            auto* pattern = layer->getEventPattern(patternIndex);
            pattern->clear(false);
            pattern->activate(
                patternElement->getIntAttribute("num", 4),
                patternElement->getIntAttribute("den", 4),
                patternElement->getIntAttribute("bars", 1));

            const int lengthTicks = pattern->getLengthTicks();
            for (auto* eventElement : patternElement->getChildWithTagNameIterator("event"))
            {
                const int tick = eventElement->getIntAttribute("tick", -1);
                const int row = eventElement->getIntAttribute("row", -1);
                if (tick < 0 || tick >= lengthTicks || row < 0 || row >= SEQ_MAX_ROWS)
                    continue;

                pattern->addEvent(
                    row,
                    tick,
                    eventElement->getIntAttribute("velocity", 100),
                    eventElement->getIntAttribute("probability", SEQ_PROB_ON),
                    eventElement->getIntAttribute("duration", GGD_DEFAULT_EVENT_DURATION_TICKS));
            }
        }
    }

    return true;
}
