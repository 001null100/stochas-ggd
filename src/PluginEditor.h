#pragma once

// PluginProcessor.cpp historically constructs SeqAudioProcessorEditor. Keep that
// entry-point name as a tiny compatibility shim while the old Stochas editor is
// retired from the build.
#include "GgdDrumEditor.h"

using SeqAudioProcessorEditor = GgdDrumEditor;
