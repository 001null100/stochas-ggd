#include "Persist.h"
#include "GgdEventPersist.h"

// Keep the mature Stochas XML implementation intact and compile it here under
// private legacy entry points. Persist.h is included first so the preprocessor
// aliases affect only the inherited definitions, not the class declarations.
#define store storeLegacy
#define retrieve retrieveLegacy
#include "Persist.cpp"
#undef retrieve
#undef store

const XmlElement& SeqPersist::store(SequenceData* sourceData)
{
    auto& root = const_cast<XmlElement&>(storeLegacy(sourceData));
    GgdEventPersist::store(*sourceData, root);
    return root;
}

bool SeqPersist::retrieve(SequenceData* targetData, const XmlElement* sourceData)
{
    if (!retrieveLegacy(targetData, sourceData))
        return false;

    // Absence is expected for alpha projects. Their cell data stays available
    // for the editor's one-time migration into an event pattern.
    GgdEventPersist::retrieve(*targetData, *sourceData);
    return true;
}
