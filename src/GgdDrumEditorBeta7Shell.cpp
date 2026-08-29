#include "GgdDrumEditor.h"
#include "GgdDrumGridFinal.h"

// Keep the established editor shell, but construct the final 1.0 grid wrapper.
// The editor owns it through the base-class unique_ptr so the compatibility
// layers underneath remain unchanged.
#define GgdDrumGrid GgdDrumGridFinal
#include "GgdDrumEditorBeta2.cpp"
#undef GgdDrumGrid
