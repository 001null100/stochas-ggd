// Engine, timeline, pattern and persistence tests.
//
// The engine tests drive GgdEngine block by block the way a host does and
// check every note-on against an analytically computed expectation: each
// expected hit must appear exactly once, at the right musical position, and
// nothing else may play.

#include <JuceHeader.h>

#include "GgdEngine.h"
#include "GgdMidiImporter.h"
#include "GgdPatternOps.h"
#include "GgdProject.h"
#include "GgdTimeline.h"
#include "Persist.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <random>
#include <set>
#include <vector>

namespace
{
using GgdTimeline::SyncMode;

constexpr int rowKick = 100;
constexpr int rowSnare = 101;
constexpr int rowHat = 102;
constexpr int noteKick = 36;
constexpr int noteSnare = 38;
constexpr int noteHat = 42;

struct MeterSection
{
    int startBar = 0;
    int numerator = 4;
    int denominator = 4;
};

// A host song: bar lines and meters, the way Bitwig reports them.
class Song
{
public:
    explicit Song(std::vector<MeterSection> meterSections)
        : sections(std::move(meterSections))
    {
    }

    MeterSection meterAt(std::int64_t bar) const
    {
        MeterSection result = sections.front();
        for (const auto& section : sections)
            if (bar >= section.startBar)
                result = section;
        return result;
    }

    double barQuarters(std::int64_t bar) const
    {
        const auto meter = meterAt(bar);
        return meter.numerator * 4.0 / meter.denominator;
    }

    double barStart(std::int64_t bar) const
    {
        if (bar < 0)
            return static_cast<double>(bar) * barQuarters(0);
        double ppq = 0.0;
        for (std::int64_t b = 0; b < bar; ++b)
            ppq += barQuarters(b);
        return ppq;
    }

    void locate(double ppq, std::int64_t& bar, double& start) const
    {
        if (ppq < 0.0)
        {
            const double length = barQuarters(0);
            bar = static_cast<std::int64_t>(std::floor(ppq / length));
            start = static_cast<double>(bar) * length;
            return;
        }

        bar = 0;
        start = 0.0;
        while (start + barQuarters(bar) <= ppq + 1.0e-12)
        {
            start += barQuarters(bar);
            ++bar;
        }
    }

private:
    std::vector<MeterSection> sections;
};

struct NoteOn
{
    double ppq = 0.0;
    int note = 0;
    int velocity = 0;
};

struct Simulation
{
    std::vector<NoteOn> noteOns;   // note-ons before the simulation end
    int allNoteOns = 0;            // including hits the last block reached past the end
    int noteOffs = 0;
    std::vector<double> noteOffPpqs;
    std::vector<double> playheadTicks; // tick at each block start
    double finalPpq = 0.0;             // host position after the last block
};

struct SimulationSettings
{
    double startPpq = 0.0;
    double endPpq = 16.0;
    double sampleRate = 48000.0;
    SyncMode mode = SyncMode::hostBars;
    std::uint64_t seed = 1;
    bool hostReportsBars = true;
    std::function<double(double)> bpmAt = [](double) { return 120.0; };
    std::function<int(int)> blockSize = [](int) { return 512; };
    double ppqJitter = 0.0; // host position noise, in quarters
    bool stopAtEnd = true;  // false: keep the transport running for a follow-up run
};

Simulation simulate(GgdEngine& engine,
                    const SequenceData& data,
                    const Song& song,
                    const SimulationSettings& settings)
{
    Simulation result;
    double truePpq = settings.startPpq;
    int block = 0;
    std::mt19937 noise(99);
    std::uniform_real_distribution<double> jitter(-settings.ppqJitter, settings.ppqJitter);

    while (truePpq < settings.endPpq - 1.0e-9)
    {
        const int samples = settings.blockSize(block++);
        const double bpm = settings.bpmAt(truePpq);
        const double samplesPerQuarter = settings.sampleRate * 60.0 / bpm;
        const double ppq = truePpq + (settings.ppqJitter > 0.0 ? jitter(noise) : 0.0);

        GgdEngine::BlockContext context;
        context.host.ppq = ppq;
        context.host.bpm = bpm;
        if (settings.hostReportsBars)
        {
            std::int64_t bar = 0;
            double start = 0.0;
            song.locate(ppq, bar, start);
            const auto meter = song.meterAt(bar);
            context.host.hasBarStart = true;
            context.host.barStartPpq = start;
            context.host.hasBarNumber = true;
            context.host.barNumber = bar;
            context.host.hasTimeSignature = true;
            context.host.numerator = meter.numerator;
            context.host.denominator = meter.denominator;
        }
        context.sampleRate = settings.sampleRate;
        context.numSamples = samples;
        context.syncMode = settings.mode;
        context.randomSeed = settings.seed;

        engine.processBlock(data, context);

        GgdPlayheadSnapshot snapshot;
        if (engine.readPlayhead(snapshot))
            result.playheadTicks.push_back(snapshot.tickAt(snapshot.wallMs));

        GgdEngine::MidiOut out;
        while (engine.popMidiEvent(samples, out))
        {
            if (out.velocity > 0)
            {
                ++result.allNoteOns;
                const double at = ppq + out.samplePosition / samplesPerQuarter;
                if (at < settings.endPpq - 1.0e-3)
                    result.noteOns.push_back({ at, out.note, out.velocity });
            }
            else
            {
                ++result.noteOffs;
                result.noteOffPpqs.push_back(ppq + out.samplePosition / samplesPerQuarter);
            }
        }
        engine.finishBlock(samples);
        truePpq += samples / samplesPerQuarter;
    }
    result.finalPpq = truePpq;

    if (!settings.stopAtEnd)
        return result;

    // Flush pending note-offs like a transport stop.
    engine.playbackStopped();
    GgdEngine::MidiOut out;
    while (engine.popMidiEvent(1 << 30, out))
        if (out.velocity == 0)
            ++result.noteOffs;
    return result;
}

std::unique_ptr<SequenceData> makeData(int numerator, int denominator, int bars)
{
    auto data = std::make_unique<SequenceData>();
    auto* layer = data->getLayer(0);
    layer->setNoteSource(true);
    layer->setNote(rowKick, noteKick, true);
    layer->setNote(rowSnare, noteSnare, true);
    layer->setNote(rowHat, noteHat, true);
    layer->getEventPattern(0)->activate(numerator, denominator, bars);
    return data;
}

struct Expected
{
    double ppq = 0.0;
    int note = 0;
};

// Every expected hit exactly once (within a sample), nothing else.
juce::String compareHits(const std::vector<Expected>& expected,
                         const std::vector<NoteOn>& actual,
                         double toleranceQuarters)
{
    std::vector<bool> used(actual.size(), false);
    juce::StringArray problems;

    for (const auto& want : expected)
    {
        int found = -1;
        for (size_t i = 0; i < actual.size(); ++i)
        {
            if (!used[i] && actual[i].note == want.note
                && std::abs(actual[i].ppq - want.ppq) <= toleranceQuarters)
            {
                found = static_cast<int>(i);
                break;
            }
        }
        if (found < 0)
            problems.add("missing note " + juce::String(want.note) + " at " + juce::String(want.ppq, 4));
        else
            used[static_cast<size_t>(found)] = true;
    }

    for (size_t i = 0; i < actual.size(); ++i)
        if (!used[i])
            problems.add("unexpected note " + juce::String(actual[i].note) + " at "
                         + juce::String(actual[i].ppq, 4));

    if (problems.size() > 12)
    {
        const int extra = problems.size() - 12;
        problems.removeRange(12, extra);
        problems.add("... and " + juce::String(extra) + " more");
    }
    return problems.joinIntoString("; ");
}

double sampleTolerance(double bpm, double sampleRate = 48000.0)
{
    return 2.0 * bpm / 60.0 / sampleRate;
}
}

//==============================================================================
class GgdTimelineTests final : public juce::UnitTest
{
public:
    GgdTimelineTests() : juce::UnitTest("Timeline mapping", "ggd") {}

    void runTest() override
    {
        using namespace GgdTimeline;

        beginTest("Bar anchoring maps host bars onto pattern bars");
        {
            PatternShape shape { 4 * 960, 2 };
            HostPosition host;
            host.ppq = 13.0;
            host.hasBarStart = true;
            host.barStartPpq = 12.0;
            host.hasBarNumber = true;
            host.barNumber = 3;
            host.hasTimeSignature = true;
            const auto anchor = resolveAnchor(SyncMode::hostBars, host, shape);
            expect(anchor.barAnchored);
            // bar 3 -> pattern bar 1, one quarter in
            expectWithinAbsoluteError(tickAt(anchor, shape, 13.0), 3840.0 + 960.0, 1.0e-6);
            // bar 4 -> pattern bar 0
            expectWithinAbsoluteError(tickAt(anchor, shape, 16.5), 480.0, 1.0e-6);
        }

        beginTest("A short pattern bar rests until the next host bar line");
        {
            PatternShape shape { 3 * 960, 1 }; // 3/4 pattern
            HostPosition host;
            host.hasTimeSignature = true; // 4/4 host
            const auto anchor = resolveAnchor(SyncMode::hostBars, host, shape);
            expectWithinAbsoluteError(tickAt(anchor, shape, 2.5), 2400.0, 1.0e-6);
            expect(tickAt(anchor, shape, 3.5) < 0.0, "fourth beat should rest");
            expectWithinAbsoluteError(tickAt(anchor, shape, 4.25), 240.0, 1.0e-6);
        }

        beginTest("Segments split at host bar lines and skip rests");
        {
            PatternShape shape { 3 * 960, 1 };
            HostPosition host;
            host.hasTimeSignature = true;
            const auto anchor = resolveAnchor(SyncMode::hostBars, host, shape);
            Segment segments[8];
            const int count = segmentsForRange(anchor, shape, 2.5, 4.5, segments, 8);
            expectEquals(count, 2);
            expectWithinAbsoluteError(segments[0].ppqStart, 2.5, 1.0e-9);
            expectWithinAbsoluteError(segments[0].ppqEnd, 3.0, 1.0e-9);
            expectWithinAbsoluteError(segments[0].tickStart, 2400.0, 1.0e-6);
            expectWithinAbsoluteError(segments[1].ppqStart, 4.0, 1.0e-9);
            expectWithinAbsoluteError(segments[1].tickStart, 0.0, 1.0e-6);
            expectEquals(static_cast<int>(segments[1].cycle), 1);
        }

        beginTest("Free anchoring loops at the pattern length");
        {
            PatternShape shape { 3 * 960, 1 };
            HostPosition host;
            const auto anchor = resolveAnchor(SyncMode::free, host, shape);
            expect(!anchor.barAnchored);
            expectWithinAbsoluteError(tickAt(anchor, shape, 3.5), 480.0, 1.0e-6);
            Segment segments[8];
            const int count = segmentsForRange(anchor, shape, 2.5, 3.5, segments, 8);
            expectEquals(count, 2);
            expectWithinAbsoluteError(segments[1].ppqStart, 3.0, 1.0e-9);
            expectWithinAbsoluteError(segments[1].tickStart, 0.0, 1.0e-6);
        }

        beginTest("Positions a hair below a bar line belong to the next bar");
        {
            PatternShape shape { 4 * 960, 2 };
            HostPosition host;
            host.hasTimeSignature = true;
            host.hasBarStart = true;
            host.barStartPpq = 0.0;
            host.hasBarNumber = true;
            const auto anchor = resolveAnchor(SyncMode::hostBars, host, shape);
            expectWithinAbsoluteError(tickAt(anchor, shape, 4.0 - 1.0e-12), 3840.0, 1.0e-3);
        }

        beginTest("Missing host bar information falls back to the meter grid");
        {
            PatternShape shape { 4 * 960, 2 };
            HostPosition host;
            host.ppq = 9.0;
            host.hasTimeSignature = true;
            const auto anchor = resolveAnchor(SyncMode::hostBars, host, shape);
            expectEquals(static_cast<int>(anchor.barNumber), 2);
            expectWithinAbsoluteError(tickAt(anchor, shape, 9.0), 960.0, 1.0e-6);
        }
    }
};

static GgdTimelineTests timelineTests;

//==============================================================================
class GgdEngineTests final : public juce::UnitTest
{
public:
    GgdEngineTests() : juce::UnitTest("Engine scheduling", "ggd") {}

    void runTest() override
    {
        beginTest("Host meter changes keep the pattern on the bar grid");
        {
            // 4 bars of 4/4, one bar of 7/8, then 4/4 again. A 4/4 groove must
            // land its kick on every bar line, including after the 7/8 bar.
            Song song({ { 0, 4, 4 }, { 4, 7, 8 }, { 5, 4, 4 } });
            auto data = makeData(4, 4, 1);
            auto* pattern = data->getLayer(0)->getEventPattern(0);
            pattern->addEvent(rowKick, 0, 110);
            pattern->addEvent(rowSnare, 2 * 960, 100);

            GgdEngine engine;
            SimulationSettings settings;
            settings.endPpq = song.barStart(9);
            const auto result = simulate(engine, *data, song, settings);

            std::vector<Expected> expected;
            for (int bar = 0; bar < 9; ++bar)
            {
                expected.push_back({ song.barStart(bar), noteKick });
                if (song.barQuarters(bar) > 2.0)
                    expected.push_back({ song.barStart(bar) + 2.0, noteSnare });
            }
            const auto problems = compareHits(expected, result.noteOns, sampleTolerance(120.0));
            expect(problems.isEmpty(), problems);
            expectEquals(result.noteOffs, result.allNoteOns);
        }

        beginTest("Free sync keeps the inherited polymeter behaviour");
        {
            Song song({ { 0, 4, 4 } });
            auto data = makeData(3, 4, 1);
            data->getLayer(0)->getEventPattern(0)->addEvent(rowKick, 0, 110);
            GgdEngine engine;
            SimulationSettings settings;
            settings.mode = SyncMode::free;
            settings.endPpq = 12.0;
            const auto result = simulate(engine, *data, song, settings);
            std::vector<Expected> expected;
            for (int i = 0; i < 4; ++i)
                expected.push_back({ i * 3.0, noteKick });
            const auto problems = compareHits(expected, result.noteOns, sampleTolerance(120.0));
            expect(problems.isEmpty(), problems);
        }

        beginTest("Odd meters line up with matching host meters");
        {
            for (const auto meter : { std::pair<int, int> { 7, 8 }, { 5, 4 }, { 15, 16 }, { 3, 4 }, { 6, 8 } })
            {
                Song song({ { 0, meter.first, meter.second } });
                auto data = makeData(meter.first, meter.second, 2);
                auto* pattern = data->getLayer(0)->getEventPattern(0);
                const int barTicks = pattern->getBarTicks();
                pattern->addEvent(rowKick, 0, 110);
                pattern->addEvent(rowSnare, barTicks + 240, 100);

                GgdEngine engine;
                SimulationSettings settings;
                settings.endPpq = song.barStart(6);
                settings.blockSize = [](int block) { return 64 + (block * 977) % 900; };
                const auto result = simulate(engine, *data, song, settings);

                std::vector<Expected> expected;
                for (int bar = 0; bar < 6; ++bar)
                {
                    if (bar % 2 == 0)
                        expected.push_back({ song.barStart(bar), noteKick });
                    else
                        expected.push_back({ song.barStart(bar) + 0.25, noteSnare });
                }
                const auto problems = compareHits(expected, result.noteOns, sampleTolerance(120.0));
                expect(problems.isEmpty(),
                       juce::String(meter.first) + "/" + juce::String(meter.second) + ": " + problems);
            }
        }

        beginTest("Pattern bars shorter than the host bar rest until the bar line");
        {
            Song song({ { 0, 4, 4 } });
            auto data = makeData(3, 4, 1);
            auto* pattern = data->getLayer(0)->getEventPattern(0);
            for (int beat = 0; beat < 3; ++beat)
                pattern->addEvent(rowHat, beat * 960, 90);
            GgdEngine engine;
            SimulationSettings settings;
            settings.endPpq = 12.0;
            const auto result = simulate(engine, *data, song, settings);
            std::vector<Expected> expected;
            for (int bar = 0; bar < 3; ++bar)
                for (int beat = 0; beat < 3; ++beat)
                    expected.push_back({ bar * 4.0 + beat, noteHat });
            const auto problems = compareHits(expected, result.noteOns, sampleTolerance(120.0));
            expect(problems.isEmpty(), problems);
        }

        beginTest("Multi-bar patterns follow the host bar number from any start");
        {
            Song song({ { 0, 4, 4 } });
            auto data = makeData(4, 4, 2);
            auto* pattern = data->getLayer(0)->getEventPattern(0);
            pattern->addEvent(rowKick, 0, 110);        // bar A
            pattern->addEvent(rowSnare, 3840, 100);    // bar B
            GgdEngine engine;
            SimulationSettings settings;
            settings.startPpq = 12.0; // bar 4 (index 3) -> pattern bar B first
            settings.endPpq = 20.0;
            const auto result = simulate(engine, *data, song, settings);
            std::vector<Expected> expected { { 12.0, noteSnare }, { 16.0, noteKick } };
            const auto problems = compareHits(expected, result.noteOns, sampleTolerance(120.0));
            expect(problems.isEmpty(), problems);
        }

        beginTest("Chained block windows never double or drop hits under tempo changes");
        {
            Song song({ { 0, 4, 4 }, { 3, 5, 8 }, { 4, 4, 4 } });
            auto data = makeData(4, 4, 1);
            auto* pattern = data->getLayer(0)->getEventPattern(0);
            for (int i = 0; i < 32; ++i)
                pattern->addEvent(rowHat, i * 120, 70 + i);

            GgdEngine engine;
            SimulationSettings settings;
            settings.endPpq = song.barStart(8);
            settings.bpmAt = [](double ppq) { return 90.0 + 15.0 * ppq; };
            std::mt19937 random(7);
            std::vector<int> sizes;
            for (int i = 0; i < 20000; ++i)
                sizes.push_back(1 + static_cast<int>(random() % 1500));
            settings.blockSize = [sizes](int block) { return sizes[static_cast<size_t>(block) % sizes.size()]; };
            settings.ppqJitter = 3.0e-5;
            const auto result = simulate(engine, *data, song, settings);

            std::vector<Expected> expected;
            for (int bar = 0; bar < 8; ++bar)
                for (int i = 0; i < 32; ++i)
                {
                    const double offset = i * 0.125;
                    if (offset < song.barQuarters(bar))
                        expected.push_back({ song.barStart(bar) + offset, noteHat });
                }
            // Tempo reaches ~550 BPM by the end; allow two samples there
            // plus the host position jitter.
            const auto problems = compareHits(expected, result.noteOns,
                                              sampleTolerance(600.0) + 2.0 * settings.ppqJitter);
            expect(problems.isEmpty(), problems);
            expectEquals(result.noteOffs, result.allNoteOns);
        }

        beginTest("A loop jump restarts cleanly");
        {
            Song song({ { 0, 4, 4 } });
            auto data = makeData(4, 4, 1);
            data->getLayer(0)->getEventPattern(0)->addEvent(rowKick, 0, 110);
            data->getLayer(0)->getEventPattern(0)->addEvent(rowSnare, 1920, 100);

            GgdEngine engine;
            Simulation all;
            for (int pass = 0; pass < 3; ++pass)
            {
                SimulationSettings settings;
                settings.startPpq = 4.0;
                settings.endPpq = 8.0;
                // simulate() stops the transport at the end; a real loop does
                // not, so run the passes through one engine without stopping.
                for (int b = 0; b < 200; ++b) // 200 blocks of 480 samples = 4 quarters
                {
                    const double ppq = settings.startPpq + b * 480.0 / 24000.0;
                    GgdEngine::BlockContext context;
                    context.host.ppq = ppq;
                    context.host.bpm = 120.0;
                    context.host.hasTimeSignature = true;
                    context.sampleRate = 48000.0;
                    context.numSamples = 480;
                    engine.processBlock(*data, context);
                    GgdEngine::MidiOut out;
                    while (engine.popMidiEvent(480, out))
                        if (out.velocity > 0)
                            all.noteOns.push_back({ ppq + out.samplePosition / 24000.0, out.note, out.velocity });
                    engine.finishBlock(480);
                }
            }
            std::vector<Expected> expected;
            for (int pass = 0; pass < 3; ++pass)
            {
                expected.push_back({ 4.0, noteKick });
                expected.push_back({ 6.0, noteSnare });
            }
            const auto problems = compareHits(expected, all.noteOns, sampleTolerance(120.0));
            expect(problems.isEmpty(), problems);
        }

        beginTest("Pre-roll never plays before song start");
        {
            Song song({ { 0, 4, 4 } });
            auto data = makeData(4, 4, 1);
            auto* pattern = data->getLayer(0)->getEventPattern(0);
            pattern->addEvent(rowKick, 0, 110);
            pattern->addEvent(rowSnare, 3 * 960, 100);
            GgdEngine engine;
            SimulationSettings settings;
            settings.startPpq = -2.3;
            settings.endPpq = 4.0;
            const auto result = simulate(engine, *data, song, settings);
            std::vector<Expected> expected { { 0.0, noteKick }, { 3.0, noteSnare } };
            const auto problems = compareHits(expected, result.noteOns, sampleTolerance(120.0));
            expect(problems.isEmpty(), problems);
        }

        beginTest("A start position a hair after a hit still plays it");
        {
            Song song({ { 0, 4, 4 } });
            auto data = makeData(4, 4, 1);
            data->getLayer(0)->getEventPattern(0)->addEvent(rowKick, 0, 110);
            GgdEngine engine;
            SimulationSettings settings;
            settings.startPpq = 8.0 + 1.0e-9;
            settings.endPpq = 9.0;
            const auto result = simulate(engine, *data, song, settings);
            expectEquals(static_cast<int>(result.noteOns.size()), 1);
        }

        beginTest("Probability decisions are deterministic per seed");
        {
            Song song({ { 0, 4, 4 } });
            auto data = makeData(4, 4, 1);
            auto* pattern = data->getLayer(0)->getEventPattern(0);
            for (int i = 0; i < 16; ++i)
                pattern->addEvent(rowHat, i * 240, 80, 50);

            auto run = [&](std::uint64_t seed, int blockSize)
            {
                GgdEngine engine;
                SimulationSettings settings;
                settings.endPpq = 4.0 * 64;
                settings.seed = seed;
                settings.blockSize = [blockSize](int) { return blockSize; };
                return simulate(engine, *data, song, settings).noteOns;
            };

            const auto first = run(1234, 512);
            const auto again = run(1234, 300);
            const auto other = run(98765, 512);
            expectEquals(static_cast<int>(again.size()), static_cast<int>(first.size()),
                         "same seed, different block sizes");
            bool identical = first.size() == again.size();
            for (size_t i = 0; identical && i < first.size(); ++i)
                identical = std::abs(first[i].ppq - again[i].ppq) < 1.0e-3;
            expect(identical, "same seed must give the same hits");

            const double fraction = static_cast<double>(first.size()) / (16.0 * 64.0);
            expect(fraction > 0.42 && fraction < 0.58, "50% probability played " + juce::String(fraction));
            expect(other.size() != first.size()
                   || std::abs(other.front().ppq - first.front().ppq) > 1.0e-6
                   || std::abs(other.back().ppq - first.back().ppq) > 1.0e-6,
                   "different seeds should differ");
        }

        beginTest("Muted layers stay silent while the playhead keeps moving");
        {
            Song song({ { 0, 4, 4 } });
            auto data = makeData(4, 4, 1);
            data->getLayer(0)->getEventPattern(0)->addEvent(rowKick, 0, 110);
            data->getLayer(0)->setMuted(true);
            GgdEngine engine;
            SimulationSettings settings;
            settings.endPpq = 8.0;
            const auto result = simulate(engine, *data, song, settings);
            expect(result.noteOns.empty());
            expect(!result.playheadTicks.empty() && result.playheadTicks.back() > 0.0);
        }

        beginTest("Swing delays every second sixteenth of the bar");
        {
            Song song({ { 0, 5, 16 } });
            auto data = makeData(5, 16, 1);
            data->setSwing(50);
            auto* pattern = data->getLayer(0)->getEventPattern(0);
            for (int i = 0; i < 5; ++i)
                pattern->addEvent(rowHat, i * 240, 90);
            GgdEngine engine;
            SimulationSettings settings;
            settings.endPpq = song.barStart(2);
            const auto result = simulate(engine, *data, song, settings);
            std::vector<Expected> expected;
            for (int bar = 0; bar < 2; ++bar)
                for (int i = 0; i < 5; ++i)
                    expected.push_back({ song.barStart(bar) + i * 0.25 + ((i & 1) ? 0.125 : 0.0), noteHat });
            const auto problems = compareHits(expected, result.noteOns, sampleTolerance(120.0));
            expect(problems.isEmpty(), problems);
        }

        beginTest("Pattern switches take effect at the current bar position");
        {
            Song song({ { 0, 4, 4 } });
            auto data = makeData(4, 4, 1);
            auto* layer = data->getLayer(0);
            layer->getEventPattern(0)->addEvent(rowKick, 0, 110);
            layer->getEventPattern(1)->activate(7, 8, 1);
            layer->getEventPattern(1)->addEvent(rowSnare, 480, 100);

            GgdEngine engine;
            Simulation all;
            for (int b = 0; b < 1125; ++b) // 256-sample blocks up to 12 quarters
            {
                const double ppq = b * 256.0 / 24000.0;
                layer->setCurrentPattern(ppq >= 4.0 - 1.0e-9 ? 1 : 0);
                GgdEngine::BlockContext context;
                context.host.ppq = ppq;
                context.host.bpm = 120.0;
                context.host.hasTimeSignature = true;
                context.host.hasBarStart = true;
                context.host.barStartPpq = std::floor(ppq / 4.0) * 4.0;
                context.host.hasBarNumber = true;
                context.host.barNumber = static_cast<std::int64_t>(std::floor(ppq / 4.0));
                context.sampleRate = 48000.0;
                context.numSamples = 256;
                engine.processBlock(*data, context);
                GgdEngine::MidiOut out;
                while (engine.popMidiEvent(256, out))
                    if (out.velocity > 0)
                        all.noteOns.push_back({ ppq + out.samplePosition / 24000.0, out.note, out.velocity });
                engine.finishBlock(256);
            }
            // 7/8 pattern bar inside a 4/4 host bar: its snare lands half a
            // beat into each host bar.
            std::vector<Expected> expected { { 0.0, noteKick }, { 4.5, noteSnare }, { 8.5, noteSnare } };
            const auto problems = compareHits(expected, all.noteOns, sampleTolerance(120.0));
            expect(problems.isEmpty(), problems);
        }

        beginTest("Changing the pattern meter while playing keeps hits on their bar positions");
        {
            // A 4/4 bar (kicks on every beat, snare on 2&) becomes 3/4 in the
            // middle of the third host bar, while the host stays in 4/4.
            Song song({ { 0, 4, 4 } });
            auto data = makeData(4, 4, 1);
            auto* pattern = data->getLayer(0)->getEventPattern(0);
            for (int beat = 0; beat < 4; ++beat)
                pattern->addEvent(rowKick, beat * 960, 110);
            pattern->addEvent(rowSnare, 1440, 100);

            GgdEngine engine;
            SimulationSettings first;
            first.endPpq = 9.25;
            first.stopAtEnd = false;
            const auto before = simulate(engine, *data, song, first);

            const auto change = GgdPatternOps::changeGeometry(*pattern, 3, 4, 1);
            expect(change.changed);
            expectEquals(change.hidden, 1, "the kick on beat 4 is parked");
            expectEquals(pattern->getParkedCount(), 1);

            SimulationSettings second;
            second.startPpq = before.finalPpq;
            second.endPpq = 20.0;
            const auto after = simulate(engine, *data, song, second);

            std::vector<NoteOn> all = before.noteOns;
            all.insert(all.end(), after.noteOns.begin(), after.noteOns.end());

            std::vector<Expected> expected;
            for (int bar = 0; bar < 5; ++bar)
            {
                const double start = bar * 4.0;
                for (int beat = 0; beat < 4; ++beat)
                {
                    const double at = start + beat;
                    const bool threeFour = at > 9.25;
                    if (!threeFour || beat < 3)
                        expected.push_back({ at, noteKick });
                }
                expected.push_back({ start + 1.5, noteSnare });
            }
            const auto problems = compareHits(expected, all, sampleTolerance(120.0));
            expect(problems.isEmpty(), problems);
        }

        beginTest("Retriggers end the previous note first");
        {
            Song song({ { 0, 4, 4 } });
            auto data = makeData(4, 4, 1);
            auto* pattern = data->getLayer(0)->getEventPattern(0);
            pattern->addEvent(rowKick, 0, 110, 100, 960 * 2);
            pattern->addEvent(rowKick, 240, 110, 100, 960 * 2);
            GgdEngine engine;
            SimulationSettings settings;
            settings.endPpq = 4.0;
            const auto result = simulate(engine, *data, song, settings);
            expectEquals(static_cast<int>(result.noteOns.size()), 2);
            expectEquals(result.noteOffs, result.allNoteOns);
            expect(!result.noteOffPpqs.empty() && result.noteOffPpqs.front() < result.noteOns[1].ppq,
                   "the first kick must end before the retrigger");
        }
    }
};

static GgdEngineTests engineTests;

//==============================================================================
class GgdPatternTests final : public juce::UnitTest
{
public:
    GgdPatternTests() : juce::UnitTest("Pattern operations", "ggd") {}

    void runTest() override
    {
        beginTest("Changing the meter keeps hits inside their bars");
        {
            GgdEventPattern pattern;
            pattern.activate(4, 4, 2);
            pattern.addEvent(rowKick, 0);
            pattern.addEvent(rowKick, 3840);          // bar 2 beat 1
            pattern.addEvent(rowSnare, 3840 + 2880);  // bar 2 beat 4

            expect(GgdPatternOps::changeGeometry(pattern, 7, 8, 2).changed);
            expectEquals(pattern.getLengthTicks(), 2 * 3360);
            expect(pattern.findEvent(rowKick, 0) >= 0);
            expect(pattern.findEvent(rowKick, 3360) >= 0, "bar 2 must still start on its bar line");
            expect(pattern.findEvent(rowSnare, 3360 + 2880) >= 0, "beat 4 still fits a 7/8 bar");
            expectEquals(pattern.getParkedCount(), 0);

            const auto toThree = GgdPatternOps::changeGeometry(pattern, 3, 4, 2);
            expectEquals(toThree.hidden, 1);
            expectEquals(pattern.getParkedCount(), 1);
            expectEquals(pattern.getEventCount(), 2);

            const auto toFour = GgdPatternOps::changeGeometry(pattern, 4, 4, 2);
            expectEquals(toFour.restored, 1);
            expectEquals(pattern.getParkedCount(), 0);
            expect(pattern.findEvent(rowSnare, 3840 + 2880) >= 0, "parked hit returns");
        }

        beginTest("Shrinking the bar count parks later bars");
        {
            GgdEventPattern pattern;
            pattern.activate(4, 4, 4);
            pattern.addEvent(rowKick, 3 * 3840);
            GgdPatternOps::changeGeometry(pattern, 4, 4, 2);
            expectEquals(pattern.getEventCount(), 0);
            expectEquals(pattern.getParkedCount(), 1);
            GgdPatternOps::changeGeometry(pattern, 4, 4, 4);
            expect(pattern.findEvent(rowKick, 3 * 3840) >= 0);
            expectEquals(pattern.getParkedCount(), 0);
        }

        beginTest("Live hits win over parked hits");
        {
            // A hidden hit whose spot is taken by a live hit when it returns.
            GgdEventPattern pattern;
            pattern.activate(4, 4, 1);
            pattern.addEvent(rowKick, 960, 120);
            GgdParkedHit hidden;
            hidden.bar = 0;
            hidden.offset = 960;
            hidden.row = static_cast<std::uint8_t>(rowKick);
            hidden.velocity = 50;
            expect(pattern.addParked(hidden));
            GgdPatternOps::changeGeometry(pattern, 4, 4, 2);
            expectEquals(pattern.getParkedCount(), 0);
            const auto* hit = pattern.findEventPtr(rowKick, 960);
            expect(hit != nullptr && hit->velocity == 120, "the live hit keeps its spot");
        }

        beginTest("Doubling replaces hidden hits of the repeated bars");
        {
            GgdEventPattern pattern;
            pattern.activate(4, 4, 2);
            pattern.addEvent(rowKick, 0, 120);
            pattern.addEvent(rowSnare, 3840, 50);     // bar 2
            pattern.addEvent(rowSnare, 3840 + 2880);  // bar 2 beat 4
            GgdPatternOps::changeGeometry(pattern, 4, 4, 1);
            expectEquals(pattern.getParkedCount(), 2);

            int discarded = -1;
            expect(GgdPatternOps::duplicateBars(pattern, &discarded));
            expectEquals(discarded, 2, "bar 2 is now the repeat of bar 1");
            expectEquals(pattern.getParkedCount(), 0);
            const auto* hit = pattern.findEventPtr(rowKick, 3840);
            expect(hit != nullptr && hit->velocity == 120);
            expect(pattern.findEvent(rowSnare, 3840) < 0);
        }

        beginTest("Parked hits keep the slot's meter edits reversible");
        {
            GgdEventPattern pattern;
            pattern.activate(4, 4, 4);
            for (int bar = 0; bar < 4; ++bar)
                for (int beat = 0; beat < 4; ++beat)
                    pattern.addEvent(rowHat, bar * 3840 + beat * 960, 60 + bar * 10 + beat);
            GgdPatternOps::changeGeometry(pattern, 3, 4, 2);
            GgdPatternOps::changeGeometry(pattern, 5, 8, 1);
            GgdPatternOps::changeGeometry(pattern, 4, 4, 4);
            expectEquals(pattern.getEventCount(), 16);
            expectEquals(pattern.getParkedCount(), 0);
            for (int bar = 0; bar < 4; ++bar)
                for (int beat = 0; beat < 4; ++beat)
                {
                    const auto* hit = pattern.findEventPtr(rowHat, bar * 3840 + beat * 960);
                    expect(hit != nullptr && hit->velocity == 60 + bar * 10 + beat);
                }
        }

        beginTest("Duplicating bars repeats the pattern");
        {
            GgdEventPattern pattern;
            pattern.activate(7, 8, 1);
            pattern.addEvent(rowKick, 0);
            pattern.addEvent(rowSnare, 1680);
            expect(GgdPatternOps::duplicateBars(pattern));
            expectEquals(pattern.getBars(), 2);
            expect(pattern.findEvent(rowKick, 3360) >= 0);
            expect(pattern.findEvent(rowSnare, 3360 + 1680) >= 0);
        }

        beginTest("Bar limits follow the engine length limit");
        {
            expectEquals(GgdPatternOps::maxBarsFor(4, 4), 64);
            expectEquals(GgdPatternOps::maxBarsFor(32, 2), GGD_MAX_PATTERN_TICKS / (960 * 64));
        }
    }
};

static GgdPatternTests patternTests;

//==============================================================================
class GgdPersistenceTests final : public juce::UnitTest
{
public:
    GgdPersistenceTests() : juce::UnitTest("Project persistence", "ggd") {}

    void runTest() override
    {
        beginTest("Events, names, mute and sync mode round-trip");
        {
            auto source = makeData(7, 8, 3);
            auto* layer = source->getLayer(0);
            layer->setLayerName("GGD4:pv");
            layer->setPatternName("Verse groove", 0);
            layer->setMuted(true);
            layer->getEventPattern(0)->addEvent(rowKick, 0, 111, 75, 240);
            layer->getEventPattern(0)->addEvent(rowSnare, 3360 * 2 + 160, 33, 100, 120);
            layer->getEventPattern(5)->activate(5, 4, 2);
            source->setSyncMode(SyncMode::free);

            SeqPersist writer;
            const auto xmlText = writer.store(source.get()).toString();
            auto xml = juce::parseXML(xmlText);
            expect(xml != nullptr);

            auto restored = std::make_unique<SequenceData>();
            SeqPersist reader;
            expect(reader.retrieve(restored.get(), xml.get()));

            const auto* restoredLayer = restored->getLayer(0);
            const auto* pattern = restoredLayer->getEventPattern(0);
            expectEquals(pattern->getNumerator(), 7);
            expectEquals(pattern->getDenominator(), 8);
            expectEquals(pattern->getBars(), 3);
            expectEquals(pattern->getEventCount(), 2);
            const auto* kick = pattern->findEventPtr(rowKick, 0);
            expect(kick != nullptr && kick->velocity == 111 && kick->probability == 75 && kick->durationTicks == 240);
            expect(pattern->findEvent(rowSnare, 3360 * 2 + 160) >= 0);
            expectEquals(juce::String(restoredLayer->getPatternName(0)), juce::String("Verse groove"));
            expect(restoredLayer->getMuted());
            expect(restored->getSyncMode() == SyncMode::free);
            expectEquals(restoredLayer->getEventPattern(5)->getNumerator(), 5);
        }

        beginTest("Projects without a sync attribute use host bars");
        {
            auto source = makeData(4, 4, 1);
            SeqPersist writer;
            auto xml = juce::parseXML(writer.store(source.get()).toString());
            auto* state = xml->getChildByName("ggd-event-state");
            expect(state != nullptr);
            state->removeAttribute("sync");
            auto restored = std::make_unique<SequenceData>();
            restored->setSyncMode(SyncMode::free);
            SeqPersist reader;
            expect(reader.retrieve(restored.get(), xml.get()));
            expect(restored->getSyncMode() == SyncMode::hostBars);
        }

        beginTest("Alpha grid cells migrate to events while loading");
        {
            auto xml = juce::parseXML(
                "<stochas version=\"1\"><layer><l idx=\"0\" name=\"GGD3:pv:7/8\">"
                "<numsteps val=\"28\"/>"
                "<pats><p idx=\"2\" name=\"Alpha\"><rows><r idx=\"100\"><cells>"
                "<c idx=\"0\" prob=\"100\" velo=\"90\" len=\"0\" offs=\"0\"/>"
                "<c idx=\"15\" prob=\"60\" velo=\"70\" len=\"0\" offs=\"20\"/>"
                "<c idx=\"20\" prob=\"100\" velo=\"80\" len=\"-1\" offs=\"0\"/>"
                "</cells></r></rows></p></pats></l></layer></stochas>");
            expect(xml != nullptr);
            auto restored = std::make_unique<SequenceData>();
            SeqPersist reader;
            expect(reader.retrieve(restored.get(), xml.get()));
            const auto* pattern = restored->getLayer(0)->getEventPattern(2);
            expect(pattern->isActive());
            expectEquals(pattern->getNumerator(), 7);
            expectEquals(pattern->getDenominator(), 8);
            expectEquals(pattern->getBars(), 2);
            expect(pattern->findEvent(100, 0) >= 0);
            const auto* shifted = pattern->findEventPtr(100, 15 * 240 + 48);
            expect(shifted != nullptr && shifted->probability == 60);
            expect(pattern->findEvent(100, 20 * 240) >= 0 && pattern->findEvent(100, 20 * 240 + 120) >= 0,
                   "retrigger splits into two hits");
            expectEquals(juce::String(restored->getLayer(0)->getPatternName(2)), juce::String("Alpha"));
        }

        beginTest("Cells in non-GGD layers are not migrated");
        {
            auto xml = juce::parseXML(
                "<stochas version=\"1\"><layer><l idx=\"0\" name=\"Lead\">"
                "<pats><p idx=\"0\"><rows><r idx=\"100\"><cells>"
                "<c idx=\"0\" prob=\"100\" velo=\"90\" len=\"0\" offs=\"0\"/>"
                "</cells></r></rows></p></pats></l></layer></stochas>");
            auto restored = std::make_unique<SequenceData>();
            SeqPersist reader;
            expect(reader.retrieve(restored.get(), xml.get()));
            expect(!restored->getLayer(0)->getEventPattern(0)->isActive());
        }

        beginTest("Normalising configures a GGD drum layer");
        {
            GgdProject project;
            expect(project.getMaps().size() >= 3);
            expect(project.getCanonicalRows().size() > 0);

            auto data = std::make_unique<SequenceData>();
            project.normalise(*data);
            const auto* layer = data->getLayer(0);
            for (int p = 0; p < SEQ_MAX_PATTERNS; ++p)
                expect(layer->getEventPattern(p)->isActive());
            expect(juce::String(layer->getLayerName()).startsWith("GGD4:"));
            expectEquals(data->getMidiPassthru(), SEQ_MIDI_PASSTHRU_ALL);

            // Kick hit is bound to the active map's kick note.
            const int kickRow = project.storageRowForCanonical(project.findCanonicalRow("kick.hit"));
            const auto kit = project.getActiveKit(*data);
            expect(kit.groups.size() > 0);
            const auto* kick = kit.findArticulation("kick.hit");
            expect(kick != nullptr && kick->primaryNoteBinding() != nullptr);
            expectEquals(static_cast<int>(layer->getCurNote(kickRow)), kick->primaryNoteBinding()->midi);

            project.setActiveMap(*data, 1);
            expectEquals(project.getActiveMapIndex(*data), 1);
        }

        beginTest("Built-in rows keep their storage rows; pads sit below them");
        {
            GgdProject project;
            const auto& rows = project.getCanonicalRows();
            int builtIn = 0;
            while (builtIn < rows.size() && GgdDrumMachineKit::padForSemanticId(rows.getReference(builtIn).semanticId) < 0)
                ++builtIn;
            expectEquals(rows.size() - builtIn, GGD_DRUM_MACHINE_MAX_PADS);
            std::set<int> used;
            for (int r = 0; r < rows.size(); ++r)
            {
                const int storage = project.storageRowForCanonical(r);
                if (r < builtIn)
                    expectEquals(storage, SEQ_MAX_ROWS - builtIn + r, "built-in rows never move");
                expect(storage >= 0 && storage < SEQ_MAX_ROWS);
                expect(used.insert(storage).second, "storage rows are unique");
                expectEquals(project.canonicalRowForStorage(storage), r);
            }
            expectEquals(project.storageRowForCanonical(builtIn), SEQ_MAX_ROWS - builtIn - GGD_DRUM_MACHINE_MAX_PADS);
        }

        beginTest("The Drum machine kit starts with 16 pads on C1 - D#2");
        {
            GgdProject project;
            auto data = std::make_unique<SequenceData>();
            project.normalise(*data);
            const int drum = project.getDrumMachineIndex();
            expect(project.isDrumMachine(drum));
            project.setActiveMap(*data, drum);
            expectEquals(project.getActiveMapIndex(*data), drum);
            expect(juce::String(data->getLayer(0)->getLayerName()).startsWith("GGD4:"));

            const auto kit = project.getActiveKit(*data);
            int pads = 0;
            for (const auto& group : kit.groups)
                pads += group.articulations.size();
            expectEquals(pads, 16);
            const auto* first = kit.findArticulation("pad.01");
            const auto* last = kit.findArticulation("pad.16");
            expect(first != nullptr && last != nullptr && kit.findArticulation("pad.17") == nullptr);
            expectEquals(first->primaryNoteBinding()->midi, 36);
            expectEquals(first->primaryNoteBinding()->noteName, juce::String("C1"));
            expectEquals(last->primaryNoteBinding()->midi, 51);
            expectEquals(last->primaryNoteBinding()->noteName, juce::String("D#2"));
            expectEquals(first->label, juce::String("Pad 1"));

            const auto* layer = data->getLayer(0);
            for (int pad = 0; pad < GGD_DRUM_MACHINE_MAX_PADS; ++pad)
            {
                const int storage = project.storageRowForCanonical(project.findCanonicalRow(GgdDrumMachineKit::semanticId(pad)));
                expectEquals(static_cast<int>(layer->getCurNote(storage)), pad < 16 ? 36 + pad : -1);
            }
            // Built-in rows are silent while the Drum machine plays.
            const int kick = project.storageRowForCanonical(project.findCanonicalRow("kick.hit"));
            expectEquals(static_cast<int>(layer->getCurNote(kick)), -1);
        }

        beginTest("Pad names and notes drive the bindings");
        {
            GgdProject project;
            auto data = std::make_unique<SequenceData>();
            project.normalise(*data);
            auto& pads = data->getDrumMachine();
            pads.setPadCount(20);
            pads.setNote(0, 24);
            pads.setName(0, juce::String::fromUTF8(u8"Kick é"));
            pads.setNote(19, 127);
            project.setActiveMap(*data, project.getDrumMachineIndex());

            const auto kit = project.getActiveKit(*data);
            expectEquals(kit.findArticulation("pad.01")->label, juce::String::fromUTF8(u8"Kick é"));
            expectEquals(kit.findArticulation("pad.01")->primaryNoteBinding()->midi, 24);
            expectEquals(kit.findArticulation("pad.20")->primaryNoteBinding()->midi, 127);
            const int padRow = project.storageRowForCanonical(project.findCanonicalRow("pad.20"));
            expectEquals(static_cast<int>(data->getLayer(0)->getCurNote(padRow)), 127);

            // Names are cut on a character boundary.
            juce::String longName;
            for (int i = 0; i < 40; ++i)
                longName << juce::String::fromUTF8(u8"é");
            pads.setName(1, longName);
            expect(pads.getName(1).getNumBytesAsUTF8() < static_cast<size_t>(GGD_PAD_NAME_BYTES));
            expect(longName.startsWith(pads.getName(1)));
            pads.setName(1, "   ");
            expectEquals(pads.getDisplayName(1), juce::String("Pad 2"));
        }

        beginTest("Drum machine pads round-trip through the project file");
        {
            GgdProject project;
            auto data = std::make_unique<SequenceData>();
            project.normalise(*data);
            auto& pads = data->getDrumMachine();
            pads.setPadCount(24);
            pads.setNote(3, 60);
            pads.setName(3, juce::String::fromUTF8(u8"Clap \U0001F44F"));
            pads.setName(23, "Last");
            project.setActiveMap(*data, project.getDrumMachineIndex());

            SeqPersist writer;
            auto xml = juce::parseXML(writer.store(data.get()).toString());
            auto restored = std::make_unique<SequenceData>();
            SeqPersist reader;
            expect(reader.retrieve(restored.get(), xml.get()));
            project.normalise(*restored);
            expect(restored->getDrumMachine() == pads);
            expectEquals(project.getActiveMapIndex(*restored), project.getDrumMachineIndex());
            const int row = project.storageRowForCanonical(project.findCanonicalRow("pad.04"));
            expectEquals(static_cast<int>(restored->getLayer(0)->getCurNote(row)), 60);
        }

        beginTest("MIDI files import onto pads by note number");
        {
            GgdProject project;
            auto data = std::make_unique<SequenceData>();
            project.normalise(*data);
            data->getDrumMachine().setNote(1, 40);
            project.setActiveMap(*data, project.getDrumMachineIndex());

            juce::MidiMessageSequence track;
            track.addEvent(juce::MidiMessage::noteOn(10, 36, static_cast<juce::uint8>(100)), 0.0);
            track.addEvent(juce::MidiMessage::noteOff(10, 36), 60.0);
            track.addEvent(juce::MidiMessage::noteOn(10, 40, static_cast<juce::uint8>(90)), 480.0);
            track.addEvent(juce::MidiMessage::noteOff(10, 40), 540.0);
            track.addEvent(juce::MidiMessage::noteOn(10, 90, static_cast<juce::uint8>(90)), 960.0);
            track.addEvent(juce::MidiMessage::noteOff(10, 90), 1020.0);
            juce::MidiFile midi;
            midi.setTicksPerQuarterNote(480);
            midi.addTrack(track);
            const auto file = juce::File::createTempFile(".mid");
            {
                juce::FileOutputStream out(file);
                expect(out.openedOk() && midi.writeTo(out));
            }

            const auto result = GgdMidiImporter::parseFile(file, project.getActiveKit(*data), project.getCanonicalRows(),
                                                           GGD_MAX_PATTERN_TICKS);
            file.deleteFile();
            expect(result.ok, result.error);
            expectEquals(result.formatName, juce::String("MIDI notes"));
            expectEquals(static_cast<int>(result.events.size()), 2);
            expectEquals(result.unresolvedNotes, 1);
            if (result.events.size() == 2)
            {
                expectEquals(result.events[0].canonicalRow, project.findCanonicalRow("pad.01"));
                expectEquals(result.events[1].canonicalRow, project.findCanonicalRow("pad.02"));
                expectEquals(result.events[1].tick, GGD_EVENT_PPQ);
            }
        }
    }
};

static GgdPersistenceTests persistenceTests;
