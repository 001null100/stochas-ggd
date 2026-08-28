#include "GgdEventPersist.h"

namespace
{
constexpr const char* rootTag = "ggd-event-state";
constexpr int stateVersion = 1;
}

void GgdEventPersist::store(const SequenceData& sequence, juce::XmlElement& root)
{
    auto* beta = new juce::XmlElement(rootTag);
    beta->setAttribute("version", stateVersion);
    beta->setAttribute("ppq", GGD_EVENT_PPQ);

    for (int layerIndex = 0; layerIndex < SEQ_MAX_LAYERS; ++layerIndex)
    {
        auto* layer = const_cast<SequenceData&>(sequence).getLayer(layerIndex);
        auto* layerElement = new juce::XmlElement("layer");
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
                if (event == nullptr)
                    continue;

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
            beta->addChildElement(layerElement);
        else
            delete layerElement;
    }

    root.addChildElement(beta);
}

bool GgdEventPersist::retrieve(SequenceData& sequence, const juce::XmlElement& root)
{
    const auto* beta = root.getChildByName(rootTag);
    if (beta == nullptr)
        return false;

    if (beta->getIntAttribute("version", -1) != stateVersion)
        return false;

    for (auto* layerElement : beta->getChildWithTagNameIterator("layer"))
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
