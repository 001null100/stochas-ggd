#include "GgdDrumEditor.h"

// Compile the release-candidate V1 implementation unchanged under compatibility
// names. Final 1.0 only replaces initialization, preference application and the
// Settings surface; drag-out, history and pattern actions remain the proven RC code.
#define initialiseV1Ui initialiseV1UiLegacy
#define applyV1InteractionPreferences applyV1InteractionPreferencesLegacy
#define showSettingsDialogV1 showSettingsDialogV1Legacy
#include "GgdDrumEditorV1.cpp"
#undef showSettingsDialogV1
#undef applyV1InteractionPreferences
#undef initialiseV1Ui
