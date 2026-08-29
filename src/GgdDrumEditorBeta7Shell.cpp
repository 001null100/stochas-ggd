#include "GgdDrumEditor.h"
#include "GgdDrumGridBeta7.h"

// Compile the established Beta 2 shell unchanged, but construct the Beta 7
// GgdDrumGrid subclass in the inherited editor constructor. The editor keeps a
// base-class unique_ptr, so the rest of the UI remains unaware of this wrapper.
#define GgdDrumGrid GgdDrumGridBeta7
#include "GgdDrumEditorBeta2.cpp"
#undef GgdDrumGrid
