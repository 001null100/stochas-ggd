// Processor and editor tests, plus the offscreen snapshot tool used for
// visual review:
//
//   stochas_tests --snapshot <output folder>
//
// renders the editor in a set of representative states to PNG files.

#include <JuceHeader.h>

#include "GgdDrumEditor.h"
#include "GgdPatternOps.h"
#include "PluginProcessor.h"

#include <cmath>
#include <functional>
#include <memory>
#include <vector>

// Reaches into the editor for tests and the snapshot tool.
struct GgdEditorTestAccess
{
    static void setMeter(GgdDrumEditor& e, int numerator, int denominator) { e.setMeter(numerator, denominator); }
    static void setBars(GgdDrumEditor& e, int bars) { e.setBars(bars); }
    static void undo(GgdDrumEditor& e) { e.undo(); }
    static void redo(GgdDrumEditor& e) { e.redo(); }
    static void commit(GgdDrumEditor& e) { e.commitEdit(); }
    static void setKit(GgdDrumEditor& e, int index) { e.setKit(index); }
    static void toggleOutput(GgdDrumEditor& e) { e.toggleOutput(); }
    static void clearPattern(GgdDrumEditor& e) { e.clearPattern(); }
    static void duplicateToEmptySlot(GgdDrumEditor& e) { e.duplicateToEmptySlot(); }
    static void doubleLength(GgdDrumEditor& e) { e.doublePatternLength(); }

    static void rename(GgdDrumEditor& e, const juce::String& name)
    {
        e.patternName.setText(name, false);
        e.renamePattern();
    }

    static int parkedCount(GgdDrumEditor& e)
    {
        return static_cast<int>(e.history[static_cast<size_t>(e.currentSlot())].parked.size());
    }

    static int undoDepth(GgdDrumEditor& e)
    {
        return static_cast<int>(e.history[static_cast<size_t>(e.currentSlot())].undo.size());
    }

    static void setTheme(GgdDrumEditor& e, int theme)
    {
        auto next = e.prefs;
        next.theme = theme;
        e.applyPreferences(next, false);
    }

    static void showLibrary(GgdDrumEditor& e, bool show)
    {
        auto next = e.prefs;
        next.showLibrary = show;
        e.applyPreferences(next, false);
    }
};

namespace
{
SequenceLayer& drumLayer(SeqAudioProcessor& processor)
{
    return *processor.mData.getUISeqData()->getLayer(GgdProject::drumLayer);
}

bool addHit(SeqAudioProcessor& processor, int slot, const juce::String& semanticId, int tick, int velocity = 100,
            int probability = 100)
{
    const auto& project = processor.getProject();
    const int canonical = project.findCanonicalRow(semanticId);
    if (canonical < 0)
        return false;
    return drumLayer(processor).getEventPattern(slot)->addEvent(project.storageRowForCanonical(canonical), tick,
                                                               velocity, probability);
}

// A few believable grooves so screenshots and tests look like real use.
void fillDemoProject(SeqAudioProcessor& processor)
{
    auto& layer = drumLayer(processor);
    constexpr int q = GGD_EVENT_PPQ;
    constexpr int s = GGD_TICKS_PER_16TH;

    // Slot 1: two bars of 4/4
    auto* verse = layer.getEventPattern(0);
    verse->clear(false);
    verse->activate(4, 4, 2);
    layer.setPatternName("Verse groove", 0);
    for (int bar = 0; bar < 2; ++bar)
    {
        const int b = bar * 4 * q;
        for (int k : { 0, 6, 8 })
            addHit(processor, 0, "kick.hit", b + k * s, 118);
        addHit(processor, 0, "kick.hit", b + 11 * s, 96, bar == 0 ? 50 : 100);
        addHit(processor, 0, "snare.hit", b + 4 * s, 122);
        addHit(processor, 0, "snare.hit", b + 12 * s, 124);
        addHit(processor, 0, "snare.hit", b + 7 * s, 38);
        addHit(processor, 0, "snare.hit", b + 15 * s, 34, 75);
        for (int i = 0; i < 8; ++i)
            addHit(processor, 0, i == 7 && bar == 1 ? "hihat.open_2" : "hihat.tip_closed", b + i * 2 * s,
                   (i % 2) == 0 ? 108 : 72);
        addHit(processor, 0, "hihat.pedal", b + 2 * s, 64);
        addHit(processor, 0, "hihat.pedal", b + 10 * s, 64);
    }
    addHit(processor, 0, "crash.main_left.hit", 0, 120);
    addHit(processor, 0, "ride.bell", 4 * q + 14 * s, 100, 25);
    addHit(processor, 0, "tom.rack_1.hit", 4 * q + 13 * s, 92);
    addHit(processor, 0, "tom.floor_1.hit", 4 * q + 14 * s, 100);

    // Slot 2: 7/8 bridge
    auto* bridge = layer.getEventPattern(1);
    bridge->clear(false);
    bridge->activate(7, 8, 2);
    layer.setPatternName("Bridge in 7", 1);
    const int bar7 = bridge->getBarTicks();
    for (int bar = 0; bar < 2; ++bar)
    {
        const int b = bar * bar7;
        addHit(processor, 1, "kick.hit", b, 120);
        addHit(processor, 1, "kick.hit", b + 3 * s, 100);
        addHit(processor, 1, "snare.hit", b + 4 * s, 120);
        addHit(processor, 1, "kick.hit", b + 8 * s, 110);
        addHit(processor, 1, "snare.hit", b + 10 * s, 120);
        for (int i = 0; i < 7; ++i)
            addHit(processor, 1, "ride.bow", b + i * 2 * s, i == 0 ? 120 : 90);
    }
    addHit(processor, 1, "china.hit", 0, 124);

    // Slot 3: a one-bar fill
    auto* fill = layer.getEventPattern(2);
    fill->clear(false);
    fill->activate(4, 4, 1);
    layer.setPatternName("Fill", 2);
    const char* toms[] = { "tom.rack_1.hit", "tom.floor_1.hit", "tom.floor_2.hit" };
    for (int i = 0; i < 12; ++i)
        addHit(processor, 2, toms[i / 4], 4 * s + i * s, 90 + (i % 4) * 8);
    addHit(processor, 2, "kick.hit", 0, 120);
    addHit(processor, 2, "snare.hit", 0, 110);

    layer.setCurrentPattern(0);
    GgdProject::syncLegacyStepCount(layer);
    processor.publishEdit(false);
}

struct FakePlayHead final : juce::AudioPlayHead
{
    juce::AudioPlayHead::PositionInfo info;

    juce::Optional<juce::AudioPlayHead::PositionInfo> getPosition() const override { return info; }

    void setPosition(double ppq, int numerator, int denominator, double bpm, bool playing)
    {
        const double barQuarters = numerator * 4.0 / denominator;
        const auto bar = static_cast<std::int64_t>(std::floor(ppq / barQuarters));
        info.setIsPlaying(playing);
        info.setBpm(bpm);
        info.setPpqPosition(ppq);
        info.setTimeSignature(juce::AudioPlayHead::TimeSignature { numerator, denominator });
        info.setPpqPositionOfLastBarStart(static_cast<double>(bar) * barQuarters);
        info.setBarCount(bar);
    }
};

// Runs the processor in real time from `fromPpq` to `toPpq` so the editor
// sees genuine playhead and played-note feedback.
void playRange(SeqAudioProcessor& processor, FakePlayHead& head, double fromPpq, double toPpq, int numerator,
               int denominator, bool realTime)
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 480;
    constexpr double bpm = 120.0;
    juce::AudioBuffer<float> buffer(2, blockSize);
    juce::MidiBuffer midi;
    const double quartersPerBlock = blockSize / sampleRate * bpm / 60.0;

    for (double ppq = fromPpq; ppq < toPpq; ppq += quartersPerBlock)
    {
        head.setPosition(ppq, numerator, denominator, bpm, true);
        midi.clear();
        processor.processBlock(buffer, midi);
        if (realTime)
            juce::Thread::sleep(10);
    }
}

void writePng(const juce::Image& image, const juce::File& file)
{
    file.deleteFile();
    juce::FileOutputStream stream(file);
    juce::PNGImageFormat png;
    if (!stream.openedOk() || !png.writeImageToStream(image, stream))
        std::printf("could not write %s\n", file.getFullPathName().toRawUTF8());
    else
        std::printf("wrote %s\n", file.getFullPathName().toRawUTF8());
}

void snap(GgdDrumEditor& editor, const juce::File& folder, const juce::String& name, float scale = 1.0f)
{
    editor.renderFrame();
    const auto image = editor.createComponentSnapshot(editor.getLocalBounds(), true, scale);
    writePng(image, folder.getChildFile(name + ".png"));
}

void makeFakeLibrary(const juce::File& root)
{
    const char* grooves[] = {
        "01 Verse/01 Verse 120bpm Straight.mid",
        "01 Verse/02 Verse 120bpm Half Time.mid",
        "01 Verse/03 Verse 120bpm Ghosted.mid",
        "02 Chorus/01 Chorus 120bpm Ride.mid",
        "02 Chorus/02 Chorus 120bpm China.mid",
        "03 Bridge/01 Bridge 7-8.mid",
        "04 Fills/01 Fill Toms 16ths.mid",
        "04 Fills/02 Fill Snare Roll.mid",
    };
    for (const auto* path : grooves)
    {
        const auto file = root.getChildFile("Grooves").getChildFile(path);
        file.getParentDirectory().createDirectory();
        file.replaceWithText("MThd");
    }
    root.getChildFile("Patterns").createDirectory();
}
}

//==============================================================================
namespace
{
struct NoteOn
{
    double ppq = 0.0;
    int note = 0;
    int velocity = 0;
};

// Plays the processor block by block across a song of meter sections and
// returns every generated note-on with its musical position.
std::vector<NoteOn> playSong(SeqAudioProcessor& processor, FakePlayHead& head,
                             const std::vector<std::pair<int, int>>& barMeters, juce::MidiBuffer* input = nullptr,
                             std::vector<juce::MidiMessage>* everything = nullptr)
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 256;
    constexpr double bpm = 120.0;
    const double samplesPerQuarter = sampleRate * 60.0 / bpm;
    juce::AudioBuffer<float> buffer(2, blockSize);
    std::vector<NoteOn> notes;

    double barStart = 0.0;
    std::int64_t barIndex = 0;
    double ppq = 0.0;
    for (const auto& [numerator, denominator] : barMeters)
    {
        const double barLength = numerator * 4.0 / denominator;
        const double barEnd = barStart + barLength;
        while (ppq < barEnd - 1.0e-9)
        {
            auto& info = head.info;
            info.setIsPlaying(true);
            info.setBpm(bpm);
            info.setPpqPosition(ppq);
            info.setTimeSignature(juce::AudioPlayHead::TimeSignature { numerator, denominator });
            info.setPpqPositionOfLastBarStart(barStart);
            info.setBarCount(barIndex);

            juce::MidiBuffer midi;
            if (input != nullptr)
                midi = *input;
            processor.processBlock(buffer, midi);
            for (const auto metadata : midi)
            {
                const auto message = metadata.getMessage();
                if (everything != nullptr)
                    everything->push_back(message);
                if (message.isNoteOn())
                    notes.push_back({ ppq + metadata.samplePosition / samplesPerQuarter, message.getNoteNumber(),
                                      message.getVelocity() });
            }
            ppq += blockSize / samplesPerQuarter;
            // keep blocks inside one host bar, as hosts report a bar start per block
            if (ppq > barEnd && ppq - blockSize / samplesPerQuarter < barEnd)
                ppq = barEnd;
        }
        barStart = barEnd;
        ++barIndex;
    }
    return notes;
}

int noteFor(const SeqAudioProcessor& processor, const juce::String& semanticId)
{
    const auto& project = processor.getProject();
    const auto* map = project.getActiveMap(*const_cast<SeqAudioProcessor&>(processor).mData.getUISeqData());
    const auto* articulation = map != nullptr ? map->findArticulation(semanticId) : nullptr;
    const auto* binding = articulation != nullptr ? articulation->primaryNoteBinding() : nullptr;
    return binding != nullptr ? binding->midi : -1;
}

std::unique_ptr<SeqAudioProcessor> makeProcessor()
{
    auto processor = std::make_unique<SeqAudioProcessor>();
    processor->setRateAndBufferSizeDetails(48000.0, 256);
    processor->prepareToPlay(48000.0, 256);
    return processor;
}

std::vector<GgdSequenceEvent> eventsOf(SeqAudioProcessor& processor, int slot)
{
    std::vector<GgdSequenceEvent> events;
    const auto* pattern = drumLayer(processor).getEventPattern(slot);
    for (int i = 0; i < pattern->getEventCount(); ++i)
        events.push_back(*pattern->getEvent(i));
    return events;
}

bool sameEvents(const std::vector<GgdSequenceEvent>& a, const std::vector<GgdSequenceEvent>& b)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].tick != b[i].tick || a[i].row != b[i].row || a[i].velocity != b[i].velocity
            || a[i].probability != b[i].probability || a[i].durationTicks != b[i].durationTicks)
            return false;
    return true;
}

juce::MouseEvent mouseAt(juce::Component& component, juce::Point<float> position, juce::Point<float> downAt,
                        juce::ModifierKeys mods = {}, bool dragged = false)
{
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), position, mods,
                            juce::MouseInputSource::defaultPressure, juce::MouseInputSource::defaultOrientation,
                            juce::MouseInputSource::defaultRotation, juce::MouseInputSource::defaultTiltX,
                            juce::MouseInputSource::defaultTiltY, &component, &component, now, downAt, now, 1,
                            dragged);
}

// A complete press-drag-release gesture through the given points.
void gesture(juce::Component& component, const std::vector<juce::Point<float>>& points, juce::ModifierKeys mods = {})
{
    const auto down = points.front();
    component.mouseDown(mouseAt(component, down, down, mods));
    for (size_t i = 1; i < points.size(); ++i)
        component.mouseDrag(mouseAt(component, points[i], down, mods, true));
    component.mouseUp(mouseAt(component, points.back(), down, mods, points.size() > 1));
}

// Owns a processor and its editor for editor-level tests.
struct EditorFixture
{
    std::unique_ptr<SeqAudioProcessor> processor = makeProcessor();
    std::unique_ptr<juce::AudioProcessorEditor> base;

    EditorFixture()
    {
        fillDemoProject(*processor);
        base.reset(processor->createEditorIfNeeded());
    }

    ~EditorFixture()
    {
        processor->editorBeingDeleted(base.get());
        base.reset();
    }

    GgdDrumEditor& editor() { return dynamic_cast<GgdDrumEditor&>(*base); }
    GgdDrumGrid& grid() { return editor().getGrid(); }
    SequenceLayer& layer() { return drumLayer(*processor); }
};
}

class GgdProcessorTests final : public juce::UnitTest
{
public:
    GgdProcessorTests() : juce::UnitTest("Processor", "ggd-plugin") {}

    void runTest() override
    {
        beginTest("Project state survives a save and reload");
        {
            auto source = makeProcessor();
            fillDemoProject(*source);
            auto& data = *source->mData.getUISeqData();
            data.setSyncMode(GgdTimeline::SyncMode::free);
            source->getProject().setActiveMap(data, 2);
            drumLayer(*source).setMuted(true);
            const juce::String name = juce::String::fromUTF8(u8"Grüße \U0001F941 Verse");
            drumLayer(*source).setPatternName(name.toRawUTF8(), 4);
            drumLayer(*source).setCurrentPattern(2);
            source->publishEdit(false);

            juce::MemoryBlock state;
            source->getStateInformation(state);

            auto target = makeProcessor();
            target->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            auto& loaded = *target->mData.getUISeqData();

            for (int slot = 0; slot < SEQ_MAX_PATTERNS; ++slot)
            {
                expect(sameEvents(eventsOf(*source, slot), eventsOf(*target, slot)), "events of slot " + juce::String(slot));
                const auto* a = drumLayer(*source).getEventPattern(slot);
                const auto* b = drumLayer(*target).getEventPattern(slot);
                expectEquals(b->getNumerator(), a->getNumerator());
                expectEquals(b->getDenominator(), a->getDenominator());
                expectEquals(b->getBars(), a->getBars());
            }
            expectEquals(juce::String::fromUTF8(drumLayer(*target).getPatternName(4)), name);
            expectEquals(juce::String::fromUTF8(drumLayer(*target).getPatternName(1)), juce::String("Bridge in 7"));
            expect(loaded.getSyncMode() == GgdTimeline::SyncMode::free);
            expectEquals(target->getProject().getActiveMapIndex(loaded), 2);
            expect(drumLayer(*target).getMuted());
            expectEquals(drumLayer(*target).getCurrentPattern(), 2);
        }

        beginTest("Long UTF-8 names are cut on a character boundary");
        {
            auto processor = makeProcessor();
            juce::String name;
            for (int i = 0; i < 40; ++i)
                name << juce::String::fromUTF8(u8"é");
            drumLayer(*processor).setPatternName(name.toRawUTF8(), 0);
            const auto stored = juce::String::fromUTF8(drumLayer(*processor).getPatternName(0));
            expect(stored.isNotEmpty());
            expect(stored.length() <= 16, "two bytes per character, 32 bytes at most");
            expect(stored.containsOnly(juce::String::fromUTF8(u8"é")));
        }

        beginTest("Host meter changes never shift the groove (host bars sync)");
        {
            auto processor = makeProcessor();
            FakePlayHead head;
            processor->setPlayHead(&head);
            auto& layer = drumLayer(*processor);
            auto* pattern = layer.getEventPattern(0);
            pattern->clear(false);
            pattern->activate(4, 4, 1);
            for (int beat = 0; beat < 4; ++beat)
                addHit(*processor, 0, "kick.hit", beat * GGD_EVENT_PPQ, 100);
            addHit(*processor, 0, "snare.hit", GGD_EVENT_PPQ + GGD_EVENT_PPQ / 2, 90);
            layer.setCurrentPattern(0);
            processor->publishEdit(false);

            const std::vector<std::pair<int, int>> song { { 4, 4 }, { 4, 4 }, { 7, 8 }, { 7, 8 }, { 3, 4 }, { 4, 4 } };
            const auto notes = playSong(*processor, head, song);
            const int kick = noteFor(*processor, "kick.hit");
            const int snare = noteFor(*processor, "snare.hit");

            std::vector<double> expectedKicks;
            std::vector<double> expectedSnares;
            double barStart = 0.0;
            for (const auto& [n, d] : song)
            {
                const double length = n * 4.0 / d;
                for (int beat = 0; beat < 4; ++beat)
                    if (beat < length - 1.0e-9)
                        expectedKicks.push_back(barStart + beat);
                if (1.5 < length)
                    expectedSnares.push_back(barStart + 1.5);
                barStart += length;
            }

            std::vector<double> kicks, snares;
            for (const auto& note : notes)
            {
                if (note.note == kick)
                    kicks.push_back(note.ppq);
                else if (note.note == snare)
                    snares.push_back(note.ppq);
                else
                    expect(false, "unexpected note " + juce::String(note.note));
            }

            auto matches = [this](const std::vector<double>& actual, const std::vector<double>& expected,
                                  const juce::String& what)
            {
                expectEquals(static_cast<int>(actual.size()), static_cast<int>(expected.size()), what + " count");
                for (size_t i = 0; i < juce::jmin(actual.size(), expected.size()); ++i)
                    expectWithinAbsoluteError(actual[i], expected[i], 1.0e-3, what + " " + juce::String(static_cast<int>(i)));
            };
            matches(kicks, expectedKicks, "kick");
            matches(snares, expectedSnares, "snare");
        }

        beginTest("Seq Off silences the pattern but host MIDI still passes through");
        {
            auto processor = makeProcessor();
            FakePlayHead head;
            processor->setPlayHead(&head);
            fillDemoProject(*processor);
            drumLayer(*processor).setMuted(true);
            processor->publishEdit(false);

            juce::MidiBuffer input;
            input.addEvent(juce::MidiMessage::noteOn(10, 77, static_cast<juce::uint8>(99)), 3);
            std::vector<juce::MidiMessage> everything;
            const auto notes = playSong(*processor, head, { { 4, 4 } }, &input, &everything);

            int passed = 0;
            for (const auto& note : notes)
            {
                expectEquals(note.note, 77, "only the host's note may sound");
                ++passed;
            }
            expect(passed > 0, "host MIDI passes through");
        }

        beginTest("Stopping the transport ends every sounding note");
        {
            auto processor = makeProcessor();
            FakePlayHead head;
            processor->setPlayHead(&head);
            auto& layer = drumLayer(*processor);
            auto* pattern = layer.getEventPattern(0);
            pattern->clear(false);
            pattern->activate(4, 4, 1);
            addHit(*processor, 0, "kick.hit", 0, 100);
            pattern->findEventPtr(processor->getProject().storageRowForCanonical(
                processor->getProject().findCanonicalRow("kick.hit")), 0)->durationTicks = GGD_EVENT_PPQ * 2;
            processor->publishEdit(false);

            std::vector<juce::MidiMessage> everything;
            playSong(*processor, head, { { 1, 4 } }, nullptr, &everything);

            head.info.setIsPlaying(false);
            juce::AudioBuffer<float> buffer(2, 256);
            juce::MidiBuffer midi;
            processor->processBlock(buffer, midi);
            for (const auto metadata : midi)
                everything.push_back(metadata.getMessage());

            // The last block crosses into the next bar, so the kick may have
            // started twice; every start must be matched by an end.
            int ons = 0, offs = 0;
            for (const auto& message : everything)
            {
                ons += message.isNoteOn() ? 1 : 0;
                offs += message.isNoteOff() ? 1 : 0;
            }
            expect(ons >= 1);
            expectEquals(offs, ons, "every long note is released at stop");
        }
    }
};

static GgdProcessorTests processorTests;

class GgdEditorTests final : public juce::UnitTest
{
public:
    GgdEditorTests() : juce::UnitTest("Editor", "ggd-plugin") {}

    void runTest() override
    {
        beginTest("Meter changes park hits and undo brings everything back");
        {
            EditorFixture f;
            const auto before = eventsOf(*f.processor, 0);
            GgdEditorTestAccess::setMeter(f.editor(), 3, 4);
            expectEquals(f.layer().getEventPattern(0)->getNumerator(), 3);
            expect(GgdEditorTestAccess::parkedCount(f.editor()) > 0, "beat four hits are parked");
            for (const auto& event : eventsOf(*f.processor, 0))
                expect(event.tick % GgdEventPattern::barTicksFor(3, 4) < GgdEventPattern::barTicksFor(3, 4));

            GgdEditorTestAccess::setMeter(f.editor(), 4, 4);
            expect(sameEvents(eventsOf(*f.processor, 0), before), "growing the bar again restores the hits");
            expectEquals(GgdEditorTestAccess::parkedCount(f.editor()), 0);

            GgdEditorTestAccess::setBars(f.editor(), 1);
            GgdEditorTestAccess::undo(f.editor());
            expect(sameEvents(eventsOf(*f.processor, 0), before), "undo restores the second bar");
            GgdEditorTestAccess::redo(f.editor());
            expectEquals(f.layer().getEventPattern(0)->getBars(), 1);
            GgdEditorTestAccess::undo(f.editor());
            GgdEditorTestAccess::undo(f.editor());
            GgdEditorTestAccess::undo(f.editor());
            expect(sameEvents(eventsOf(*f.processor, 0), before));
        }

        beginTest("Edits are published to the audio copy");
        {
            EditorFixture f;
            auto& grid = f.grid();
            grid.selectAll();
            grid.deleteSelection();
            expectEquals(f.layer().getEventPattern(0)->getEventCount(), 0);
            expectEquals(f.processor->mData.getAudSeqData()->getLayer(0)->getEventPattern(0)->getEventCount(), 0);
        }

        beginTest("Undo restores content and the selection that went with it");
        {
            EditorFixture f;
            auto& grid = f.grid();
            const auto before = eventsOf(*f.processor, 0);
            grid.setToolMode(GgdDrumGrid::ToolMode::select);
            grid.selectAll();
            auto all = grid.getSelectionCoordinates();
            std::vector<std::pair<int, int>> firstBeat;
            for (const auto& c : all)
                if (c.second < GGD_EVENT_PPQ)
                    firstBeat.push_back(c);
            grid.restoreSelectionCoordinates(firstBeat);
            const int selected = grid.getSelectedCount();
            expect(selected > 0);

            grid.deleteSelection();
            expectEquals(grid.getSelectedCount(), 0);
            GgdEditorTestAccess::undo(f.editor());
            expect(sameEvents(eventsOf(*f.processor, 0), before));
            expectEquals(grid.getSelectedCount(), selected, "the deleted hits are selected again");
            GgdEditorTestAccess::redo(f.editor());
            expectEquals(static_cast<int>(eventsOf(*f.processor, 0).size()), static_cast<int>(before.size()) - selected);
        }

        beginTest("Every slot keeps its own history");
        {
            EditorFixture f;
            auto& grid = f.grid();
            const auto slot0 = eventsOf(*f.processor, 0);
            const auto slot1 = eventsOf(*f.processor, 1);

            grid.selectAll();
            grid.deleteSelection();
            f.editor().selectPatternSlot(1);
            GgdEditorTestAccess::setBars(f.editor(), 1);
            f.editor().selectPatternSlot(0);
            GgdEditorTestAccess::undo(f.editor());
            expect(sameEvents(eventsOf(*f.processor, 0), slot0), "slot 1 undo");
            expect(!sameEvents(eventsOf(*f.processor, 1), slot1), "slot 2 untouched by slot 1 undo");
            f.editor().selectPatternSlot(1);
            GgdEditorTestAccess::undo(f.editor());
            expect(sameEvents(eventsOf(*f.processor, 1), slot1), "slot 2 undo");
        }

        beginTest("Renaming is undoable and handles UTF-8");
        {
            EditorFixture f;
            const juce::String name = juce::String::fromUTF8(u8"Café groove");
            GgdEditorTestAccess::rename(f.editor(), name);
            expectEquals(juce::String::fromUTF8(f.layer().getPatternName(0)), name);
            GgdEditorTestAccess::undo(f.editor());
            expectEquals(juce::String::fromUTF8(f.layer().getPatternName(0)), juce::String("Verse groove"));
        }

        beginTest("Duplicate copies into the next empty slot");
        {
            EditorFixture f;
            const auto source = eventsOf(*f.processor, 0);
            GgdEditorTestAccess::duplicateToEmptySlot(f.editor());
            expect(sameEvents(eventsOf(*f.processor, 3), source));
            expectEquals(juce::String::fromUTF8(f.layer().getPatternName(3)), juce::String("Verse groove copy"));
            expectEquals(f.layer().getCurrentPattern(), 0);
        }

        beginTest("Double length repeats the bars");
        {
            EditorFixture f;
            f.editor().selectPatternSlot(2);
            const auto source = eventsOf(*f.processor, 2);
            GgdEditorTestAccess::doubleLength(f.editor());
            const auto doubled = eventsOf(*f.processor, 2);
            expectEquals(f.layer().getEventPattern(2)->getBars(), 2);
            expectEquals(static_cast<int>(doubled.size()), static_cast<int>(source.size()) * 2);
        }

        beginTest("Keyboard shortcuts drive the timeline");
        {
            EditorFixture f;
            auto& grid = f.grid();
            const auto before = eventsOf(*f.processor, 0);
            grid.selectAll();
            expect(grid.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey)));
            expectEquals(static_cast<int>(eventsOf(*f.processor, 0).size()), 0);
            expect(grid.keyPressed(juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0)));
            expect(sameEvents(eventsOf(*f.processor, 0), before));
            expect(!grid.keyPressed(juce::KeyPress('s', juce::ModifierKeys::commandModifier, 0)),
                   "Ctrl+S belongs to the host");
            expect(grid.keyPressed(juce::KeyPress('s', juce::ModifierKeys(), 0)));
            expect(grid.getToolMode() == GgdDrumGrid::ToolMode::select);
            expect(grid.keyPressed(juce::KeyPress('d', juce::ModifierKeys(), 0)));
            expect(grid.getToolMode() == GgdDrumGrid::ToolMode::draw);
        }

        beginTest("Switching kits keeps the hits and changes their notes");
        {
            EditorFixture f;
            const auto before = eventsOf(*f.processor, 0);
            const int row = f.processor->getProject().storageRowForCanonical(
                f.processor->getProject().findCanonicalRow("snare.hit"));
            GgdEditorTestAccess::setKit(f.editor(), 2);
            expect(sameEvents(eventsOf(*f.processor, 0), before));
            expectEquals(f.processor->getProject().getActiveMapIndex(*f.processor->mData.getUISeqData()), 2);
            expect(f.layer().getCurNote(row) >= 0);
        }
    }
};

static GgdEditorTests editorTests;

class GgdGridMouseTests final : public juce::UnitTest
{
public:
    GgdGridMouseTests() : juce::UnitTest("Timeline mouse editing", "ggd-plugin") {}

    void runTest() override
    {
        constexpr int s = GGD_TICKS_PER_16TH;

        beginTest("A paint stroke adds hits and undoes as one step");
        {
            EditorFixture f;
            auto& grid = f.grid();
            const int row = f.processor->getProject().findCanonicalRow("tom.rack_1.hit");
            const int before = f.layer().getEventPattern(0)->getEventCount();
            std::vector<juce::Point<float>> stroke;
            for (int tick = 0; tick <= 6 * s; tick += s / 2)
                stroke.push_back(grid.getCellBounds(row, tick).getCentre());
            gesture(grid, stroke);
            expectEquals(f.layer().getEventPattern(0)->getEventCount(), before + 7, "seven cells painted");
            GgdEditorTestAccess::undo(f.editor());
            expectEquals(f.layer().getEventPattern(0)->getEventCount(), before, "one undo removes the stroke");
        }

        beginTest("A fast stroke never skips cells");
        {
            EditorFixture f;
            auto& grid = f.grid();
            const int row = f.processor->getProject().findCanonicalRow("tom.floor_1.hit");
            const int before = f.layer().getEventPattern(0)->getEventCount();
            gesture(grid, { grid.getCellBounds(row, 0).getCentre(), grid.getCellBounds(row, 9 * s).getCentre() });
            expectEquals(f.layer().getEventPattern(0)->getEventCount(), before + 10);
        }

        beginTest("Clicking a hit removes it, right-drag erases a run");
        {
            EditorFixture f;
            auto& grid = f.grid();
            const auto& project = f.processor->getProject();
            const int hat = project.findCanonicalRow("hihat.tip_closed");
            const int before = f.layer().getEventPattern(0)->getEventCount();
            gesture(grid, { grid.getCellBounds(hat, 0).getCentre() });
            expectEquals(f.layer().getEventPattern(0)->getEventCount(), before - 1);

            gesture(grid, { grid.getCellBounds(hat, 2 * s).getCentre(), grid.getCellBounds(hat, 6 * s).getCentre() },
                    juce::ModifierKeys(juce::ModifierKeys::rightButtonModifier));
            expectEquals(f.layer().getEventPattern(0)->getEventCount(), before - 4, "hats at 2, 4 and 6 erased");
            GgdEditorTestAccess::undo(f.editor());
            GgdEditorTestAccess::undo(f.editor());
            expectEquals(f.layer().getEventPattern(0)->getEventCount(), before);
        }

        beginTest("Shift-drag changes velocity");
        {
            EditorFixture f;
            auto& grid = f.grid();
            const auto& project = f.processor->getProject();
            const int kick = project.findCanonicalRow("kick.hit");
            const auto centre = grid.getCellBounds(kick, 0).getCentre();
            gesture(grid, { centre, centre.translated(0.0f, 20.0f) },
                    juce::ModifierKeys(juce::ModifierKeys::shiftModifier | juce::ModifierKeys::leftButtonModifier));
            const auto* event = f.layer().getEventPattern(0)->findEventPtr(project.storageRowForCanonical(kick), 0);
            expect(event != nullptr);
            if (event != nullptr)
                expectEquals(static_cast<int>(event->velocity), 118 - 16);
        }

        beginTest("Select tool: box select, then drag to move");
        {
            EditorFixture f;
            auto& grid = f.grid();
            const auto& project = f.processor->getProject();
            grid.setToolMode(GgdDrumGrid::ToolMode::select);
            const int snare = project.findCanonicalRow("snare.hit");
            const int storage = project.storageRowForCanonical(snare);

            // box around the snare backbeat on beat 2 (tick 4s)
            const auto cell = grid.getCellBounds(snare, 4 * s);
            gesture(grid, { cell.getTopLeft().translated(-6.0f, 2.0f), cell.getBottomRight().translated(6.0f, -2.0f) });
            expectEquals(grid.getSelectedCount(), 1);

            const auto from = cell.getCentre();
            gesture(grid, { from, from.translated(grid.getCellBounds(snare, 5 * s).getX() - cell.getX(), 0.0f) });
            expect(f.layer().getEventPattern(0)->findEventPtr(storage, 5 * s) != nullptr, "moved one sixteenth later");
            expect(f.layer().getEventPattern(0)->findEventPtr(storage, 4 * s) == nullptr);
            expectEquals(grid.getSelectedCount(), 1);

            // Alt-drag duplicates
            const auto moved = grid.getCellBounds(snare, 5 * s).getCentre();
            gesture(grid, { moved, moved.translated(grid.getCellBounds(snare, 6 * s).getX() - grid.getCellBounds(snare, 5 * s).getX(), 0.0f) },
                    juce::ModifierKeys(juce::ModifierKeys::altModifier | juce::ModifierKeys::leftButtonModifier));
            expect(f.layer().getEventPattern(0)->findEventPtr(storage, 5 * s) != nullptr, "original stays");
            expect(f.layer().getEventPattern(0)->findEventPtr(storage, 6 * s) != nullptr, "copy added");
        }

        beginTest("Clicking a row name auditions its note");
        {
            EditorFixture f;
            auto& grid = f.grid();
            const auto& project = f.processor->getProject();
            const int kick = project.findCanonicalRow("kick.hit");
            const auto cell = grid.getCellBounds(kick, 0);
            f.processor->mIncomingData.clearFifo();
            gesture(grid, { juce::Point<float>(40.0f, cell.getCentreY()) });
            int message = 0, layer = 0, row = 0;
            expect(f.processor->mIncomingData.readFromFifo(&message, &layer, &row));
            expectEquals(message, SEQ_MIDI_NOTEON);
            expectEquals(row, project.storageRowForCanonical(kick));
            expect(f.processor->mIncomingData.readFromFifo(&message, &layer, &row));
            expectEquals(message, SEQ_MIDI_NOTEOFF);
        }
    }
};

static GgdGridMouseTests gridMouseTests;

//==============================================================================
namespace
{
// Runs a list of steps on the message thread, one per timer tick, so that
// asynchronous UI updates (tree views, callAsync) settle between steps.
class StepRunner final : private juce::Timer
{
public:
    void add(std::function<void()> step) { steps.push_back(std::move(step)); }

    void run()
    {
        startTimer(40);
        juce::MessageManager::getInstance()->runDispatchLoop();
    }

private:
    std::vector<std::function<void()>> steps;
    size_t next = 0;

    void timerCallback() override
    {
        if (next < steps.size())
        {
            steps[next++]();
            return;
        }
        stopTimer();
        juce::MessageManager::getInstance()->stopDispatchLoop();
    }
};

void stopTransport(SeqAudioProcessor& processor, FakePlayHead& head, double ppq)
{
    head.setPosition(ppq, 4, 4, 120.0, false);
    juce::AudioBuffer<float> buffer(2, 480);
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);
}
}

int runSnapshotTool(const juce::StringArray& args)
{
    const int index = args.indexOf("--snapshot");
    const auto folder = juce::File::getCurrentWorkingDirectory().getChildFile(
        index >= 0 && index + 1 < args.size() ? args[index + 1] : "snapshots");
    folder.createDirectory();

    // Keep the snapshot run away from the user's real preferences.
    ggdSetSettingsFolderName("Stochas GGD Snapshots");
    const auto settingsFolder = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                    .getChildFile("Stochas GGD Snapshots");
    settingsFolder.deleteRecursively();

    const auto library = folder.getChildFile("library");
    library.deleteRecursively();
    makeFakeLibrary(library);
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "Stochas GGD";
        options.filenameSuffix = "settings";
        options.folderName = ggdSettingsFolderName();
        options.osxLibrarySubFolder = "Application Support";
        options.storageFormat = juce::PropertiesFile::storeAsXML;
        juce::PropertiesFile settings(options);
        settings.setValue("grooveRoot", library.getChildFile("Grooves").getFullPathName());
        settings.saveIfNeeded();
    }

    auto processor = std::make_unique<SeqAudioProcessor>();
    fillDemoProject(*processor);
    FakePlayHead head;
    processor->setPlayHead(&head);
    processor->setRateAndBufferSizeDetails(48000.0, 480);
    processor->prepareToPlay(48000.0, 480);

    std::unique_ptr<juce::AudioProcessorEditor> base(processor->createEditorIfNeeded());
    auto& editor = dynamic_cast<GgdDrumEditor&>(*base);
    auto& grid = editor.getGrid();
    auto& proc = *processor;

    StepRunner runner;
    runner.add([&] { editor.setSize(1480, 860); });
    runner.add([&] { snap(editor, folder, "01-draw"); });
    runner.add([&]
    {
        grid.setToolMode(GgdDrumGrid::ToolMode::select);
        grid.selectAll();
        auto all = grid.getSelectionCoordinates();
        std::vector<std::pair<int, int>> some;
        for (const auto& c : all)
            if (c.second < GGD_EVENT_PPQ * 2)
                some.push_back(c);
        grid.restoreSelectionCoordinates(some);
    });
    runner.add([&] { snap(editor, folder, "02-select"); });
    runner.add([&] { editor.openSettings(); });
    runner.add([&] { snap(editor, folder, "03-settings"); });
    runner.add([&] { editor.closeSettings(); });
    runner.add([&]
    {
        grid.clearSelection();
        grid.setToolMode(GgdDrumGrid::ToolMode::draw);
        playRange(proc, head, 0.0, 5.0, 4, 4, false);
        playRange(proc, head, 5.0, 5.9, 4, 4, true);
        snap(editor, folder, "04-playing");
    });
    for (int theme = 1; theme < static_cast<int>(GgdThemeId::count); ++theme)
    {
        runner.add([&, theme]
        {
            GgdEditorTestAccess::setTheme(editor, theme);
            playRange(proc, head, 5.9, 6.3, 4, 4, true);
        });
        runner.add([&, theme] { snap(editor, folder, "05-theme-" + ggdThemeName(ggdThemeFromIndex(theme)).toLowerCase()); });
    }
    runner.add([&]
    {
        GgdEditorTestAccess::setTheme(editor, 0);
        stopTransport(proc, head, 6.0);
        editor.selectPatternSlot(1);
    });
    runner.add([&]
    {
        playRange(proc, head, 8.0, 9.2, 4, 4, true);
        snap(editor, folder, "06-odd-meter-host-mismatch");
        stopTransport(proc, head, 10.0);
    });
    runner.add([&]
    {
        editor.selectPatternSlot(0);
        GgdEditorTestAccess::setBars(editor, 1);
        GgdEditorTestAccess::setMeter(editor, 3, 4);
    });
    runner.add([&] { snap(editor, folder, "07-parked-hits"); });
    runner.add([&]
    {
        GgdEditorTestAccess::undo(editor);
        GgdEditorTestAccess::undo(editor);
        editor.setSize(1200, 700);
    });
    runner.add([&] { snap(editor, folder, "08-narrow"); });
    runner.add([&] { GgdEditorTestAccess::showLibrary(editor, false); });
    runner.add([&] { snap(editor, folder, "09-narrow-no-library"); });
    runner.add([&]
    {
        GgdEditorTestAccess::showLibrary(editor, true);
        editor.setSize(1480, 860);
        editor.selectPatternSlot(5);
    });
    runner.add([&] { snap(editor, folder, "10-empty-slot"); });
    runner.add([&] { editor.selectPatternSlot(0); });
    runner.add([&] { snap(editor, folder, "11-draw-2x", 2.0f); });

    // Gestures caught mid-way.
    const auto& project = proc.getProject();
    const int kickRow = project.findCanonicalRow("kick.hit");
    const int snareRow = project.findCanonicalRow("snare.hit");
    const int hatRow = project.findCanonicalRow("hihat.tip_closed");
    constexpr int s16 = GGD_TICKS_PER_16TH;
    runner.add([&]
    {
        const auto cell = grid.getCellBounds(hatRow, 5 * s16).getCentre();
        grid.mouseMove(mouseAt(grid, cell, cell));
        snap(editor, folder, "12-hover-empty-cell");
        grid.mouseExit(mouseAt(grid, cell, cell));
    });
    runner.add([&]
    {
        const auto hit = grid.getCellBounds(kickRow, 6 * s16).getCentre();
        const juce::ModifierKeys shift(juce::ModifierKeys::shiftModifier | juce::ModifierKeys::leftButtonModifier);
        grid.mouseDown(mouseAt(grid, hit, hit, shift));
        grid.mouseDrag(mouseAt(grid, hit.translated(0.0f, -14.0f), hit, shift, true));
        snap(editor, folder, "13-velocity-drag");
        grid.mouseUp(mouseAt(grid, hit.translated(0.0f, -14.0f), hit, shift, true));
        GgdEditorTestAccess::undo(editor);
    });
    runner.add([&]
    {
        grid.setToolMode(GgdDrumGrid::ToolMode::select);
        const auto start = grid.getCellBounds(kickRow, 3 * s16).getTopLeft().translated(4.0f, 4.0f);
        const auto end = grid.getCellBounds(hatRow, 9 * s16).getBottomRight();
        grid.mouseDown(mouseAt(grid, start, start));
        grid.mouseDrag(mouseAt(grid, end, start, {}, true));
        snap(editor, folder, "14-marquee");
        grid.mouseUp(mouseAt(grid, end, start, {}, true));
    });
    runner.add([&]
    {
        const auto from = grid.getCellBounds(snareRow, 4 * s16).getCentre();
        const auto to = grid.getCellBounds(snareRow, 6 * s16).getCentre().translated(0.0f, 2.0f);
        grid.mouseDown(mouseAt(grid, from, from));
        grid.mouseDrag(mouseAt(grid, to, from, {}, true));
        snap(editor, folder, "15-move-preview");
        grid.mouseUp(mouseAt(grid, from, from, {}, true));
    });
    runner.add([&]
    {
        const auto hit = grid.getCellBounds(hatRow, 4 * s16).getCentre();
        const juce::ModifierKeys shift(juce::ModifierKeys::shiftModifier | juce::ModifierKeys::leftButtonModifier);
        grid.mouseDown(mouseAt(grid, hit, hit, shift));
        grid.mouseDrag(mouseAt(grid, hit.translated(0.0f, 12.0f), hit, shift, true));
        snap(editor, folder, "16-velocity-chips");
        grid.mouseUp(mouseAt(grid, hit.translated(0.0f, 12.0f), hit, shift, true));
        GgdEditorTestAccess::undo(editor);
        grid.clearSelection();
        grid.setToolMode(GgdDrumGrid::ToolMode::draw);
    });
    runner.run();

    processor->editorBeingDeleted(base.get());
    base.reset();
    processor.reset();
    settingsFolder.deleteRecursively();
    return 0;
}
