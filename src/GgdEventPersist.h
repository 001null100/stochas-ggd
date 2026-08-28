#pragma once

#include "SequenceData.h"

class GgdEventPersist
{
public:
    static void store(const SequenceData& sequence, juce::XmlElement& root);
    static bool retrieve(SequenceData& sequence, const juce::XmlElement& root);
};
