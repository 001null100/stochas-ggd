#include "GgdKitMap.h"
#include "GgdBuiltInMaps.h"

const GgdMidiBinding* GgdArticulation::primaryNoteBinding() const
{
    for (const auto& binding : bindings)
    {
        if (binding.kind == "note" && binding.role == "primary" && binding.midi >= 0)
            return &binding;
    }

    for (const auto& binding : bindings)
    {
        if (binding.kind == "note" && binding.midi >= 0)
            return &binding;
    }

    return nullptr;
}

const GgdArticulation* GgdKitMap::findArticulation(const juce::String& semanticId) const
{
    for (const auto& group : groups)
        for (const auto& articulation : group.articulations)
            if (articulation.semanticId == semanticId)
                return &articulation;

    return nullptr;
}

bool GgdKitMapLibrary::parseMap(const juce::String& json,
                                GgdKitMap& outMap,
                                juce::String& error)
{
    auto root = juce::JSON::parse(json);
    auto* rootObject = root.getDynamicObject();
    if (rootObject == nullptr)
    {
        error = "Map root is not a JSON object";
        return false;
    }

    outMap.id = rootObject->getProperty("id").toString();
    outMap.vendor = rootObject->getProperty("vendor").toString();
    outMap.library = rootObject->getProperty("library").toString();
    outMap.variant = rootObject->getProperty("variant").toString();

    auto groupsValue = rootObject->getProperty("groups");
    auto* groupsArray = groupsValue.getArray();
    if (groupsArray == nullptr)
    {
        error = "Map has no groups array";
        return false;
    }

    for (const auto& groupValue : *groupsArray)
    {
        auto* groupObject = groupValue.getDynamicObject();
        if (groupObject == nullptr)
            continue;

        GgdGroup group;
        group.id = groupObject->getProperty("id").toString();
        group.label = groupObject->getProperty("label").toString();

        auto articulationsValue = groupObject->getProperty("articulations");
        auto* articulationsArray = articulationsValue.getArray();
        if (articulationsArray == nullptr)
            continue;

        for (const auto& articulationValue : *articulationsArray)
        {
            auto* articulationObject = articulationValue.getDynamicObject();
            if (articulationObject == nullptr)
                continue;

            GgdArticulation articulation;
            articulation.semanticId = articulationObject->getProperty("semanticId").toString();
            articulation.label = articulationObject->getProperty("label").toString();
            articulation.kind = articulationObject->getProperty("kind").toString();

            auto bindingsValue = articulationObject->getProperty("bindings");
            if (auto* bindingsArray = bindingsValue.getArray())
            {
                for (const auto& bindingValue : *bindingsArray)
                {
                    auto* bindingObject = bindingValue.getDynamicObject();
                    if (bindingObject == nullptr)
                        continue;

                    GgdMidiBinding binding;
                    binding.kind = bindingObject->getProperty("kind").toString();
                    const auto midiValue = bindingObject->getProperty("midi");
                    binding.midi = midiValue.isVoid() ? -1 : static_cast<int>(midiValue);
                    binding.noteName = bindingObject->getProperty("noteName").toString();
                    binding.role = bindingObject->getProperty("role").toString();
                    articulation.bindings.add(binding);
                }
            }

            if (articulation.semanticId.isNotEmpty())
                group.articulations.add(articulation);
        }

        if (group.id.isNotEmpty())
            outMap.groups.add(group);
    }

    if (outMap.id.isEmpty() || outMap.groups.isEmpty())
    {
        error = "Map is missing an id or articulations";
        return false;
    }

    return true;
}

juce::Array<GgdKitMap> GgdKitMapLibrary::loadBuiltInMaps()
{
    juce::Array<GgdKitMap> maps;

    // Order is intentionally stable. New built-in libraries should be appended so
    // canonical semantic row indices remain compatible with saved Stochas patterns.
    const char* jsonMaps[] =
    {
        GgdBuiltInMaps::pV,
        GgdBuiltInMaps::pIV,
        GgdBuiltInMaps::modernAndMassive
    };

    for (const auto* json : jsonMaps)
    {
        GgdKitMap map;
        juce::String error;
        if (parseMap(juce::String::fromUTF8(json), map, error))
            maps.add(map);
        else
            DBG("Failed to parse built-in GGD map: " + error);
    }

    return maps;
}

juce::Array<GgdCanonicalRow>
GgdKitMapLibrary::buildCanonicalRows(const juce::Array<GgdKitMap>& maps)
{
    juce::Array<GgdCanonicalRow> rows;
    juce::StringArray seen;

    for (const auto& map : maps)
    {
        for (const auto& group : map.groups)
        {
            for (const auto& articulation : group.articulations)
            {
                if (seen.contains(articulation.semanticId))
                    continue;

                seen.add(articulation.semanticId);
                GgdCanonicalRow row;
                row.semanticId = articulation.semanticId;
                row.groupId = group.id;
                row.groupLabel = group.label;
                row.defaultLabel = articulation.label;
                rows.add(row);
            }
        }
    }

    return rows;
}

int GgdKitMapLibrary::findCanonicalRow(const juce::Array<GgdCanonicalRow>& rows,
                                       const juce::String& semanticId)
{
    for (int i = 0; i < rows.size(); ++i)
        if (rows.getReference(i).semanticId == semanticId)
            return i;

    return -1;
}
