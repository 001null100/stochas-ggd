#pragma once

#include "GgdDrumGridBeta7.h"

class GgdDrumGridV1 final : public GgdDrumGridBeta7
{
public:
    using GgdDrumGridBeta7::GgdDrumGridBeta7;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void modifierKeysChanged(const juce::ModifierKeys& modifiers) override;

    void refreshActiveMapLayout();

private:
    std::vector<EventRef> multiVelocityRefs;
    double multiVelocityUntilMs = 0.0;
    bool multiVelocityPinned = false;

    bool activeMapLayoutNeedsRefresh() const;
    void rebuildActiveMapLayout();
    void showMultiVelocity(const std::vector<EventRef>& refs, bool pinned);
    void clearMultiVelocity();
    void paintMultiVelocity(juce::Graphics& g);
};
