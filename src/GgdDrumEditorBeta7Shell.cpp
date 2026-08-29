#include "GgdDrumEditor.h"
#include "GgdDrumGridV1.h"

// Keep the established editor shell, but construct the final 1.0 grid wrapper.
// The editor owns it through the base-class unique_ptr so the compatibility
// layers underneath remain unchanged.
#define GgdDrumGrid GgdDrumGridV1
#include "GgdDrumEditorBeta2.cpp"
#undef GgdDrumGrid
