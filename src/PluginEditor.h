#pragma once

// PluginProcessor.cpp historically constructs SeqAudioProcessorEditor and relied
// on the old editor header to pull in EditorState's full definition. Keep both
// details here while routing the actual editor to the new drum UI.
#include "EditorState.h"
#include "GgdDrumEditor.h"

using SeqAudioProcessorEditor = GgdDrumEditor;
