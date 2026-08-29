#pragma once

#include "GgdDrumGridBeta.h"

// Thin interaction layer over the Beta 6 grid. The event model, timing
// scheduler and Beta 6 rendering remain owned by GgdDrumGrid; this wrapper only
// restores high-information edit feedback and adds Select-mode velocity drag.
class GgdDrumGridBeta7 final : public GgdDrumGrid
{
public:
    using GgdDrumGrid::GgdDrumGrid;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
};
