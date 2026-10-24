#pragma once

#include "GgdTimeline.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <type_traits>

// Single-writer, multi-reader sequence lock for small trivially copyable
// values. The audio thread publishes with store(); UI readers retry until they
// observe a snapshot that was not being written concurrently. Neither side
// ever blocks.
template <typename T>
class GgdSeqLockValue
{
    static_assert(std::is_trivially_copyable_v<T>, "GgdSeqLockValue needs a trivially copyable type");
    static constexpr std::size_t wordCount = (sizeof(T) + sizeof(std::uint64_t) - 1) / sizeof(std::uint64_t);

public:
    void store(const T& value) noexcept
    {
        std::uint64_t words[wordCount] = {};
        std::memcpy(words, &value, sizeof(T));

        const auto sequence = version.load(std::memory_order_relaxed);
        version.store(sequence + 1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        for (std::size_t i = 0; i < wordCount; ++i)
            storage[i].store(words[i], std::memory_order_relaxed);
        version.store(sequence + 2, std::memory_order_release);
    }

    bool load(T& value) const noexcept
    {
        for (int attempt = 0; attempt < 32; ++attempt)
        {
            const auto before = version.load(std::memory_order_acquire);
            if ((before & 1u) != 0)
                continue;

            std::uint64_t words[wordCount];
            for (std::size_t i = 0; i < wordCount; ++i)
                words[i] = storage[i].load(std::memory_order_relaxed);

            std::atomic_thread_fence(std::memory_order_acquire);
            if (version.load(std::memory_order_relaxed) == before)
            {
                std::memcpy(&value, words, sizeof(T));
                return true;
            }
        }
        return false;
    }

private:
    std::atomic<std::uint32_t> version { 0 };
    std::array<std::atomic<std::uint64_t>, wordCount> storage {};
};

// Everything the editor needs to draw an exact, smoothly moving playhead
// between audio callbacks: the host position at the start of the last block,
// the tempo and the anchoring used by the engine for that block.
struct GgdPlayheadSnapshot
{
    bool playing = false;
    bool muted = false;
    bool hostMeterKnown = false;
    int pattern = -1;
    int hostNumerator = 4;
    int hostDenominator = 4;
    GgdTimeline::Anchor anchor;
    GgdTimeline::PatternShape shape;
    double ppq = 0.0;
    double quartersPerMs = 0.0;
    double wallMs = 0.0;
    double blockMs = 0.0;

    // Host position extrapolated to nowMs. Extrapolation is limited to a
    // little more than one block so a stalled host cannot run the playhead
    // away from the audio.
    double ppqAt(double nowMs) const
    {
        double elapsed = nowMs - wallMs;
        if (elapsed < 0.0)
            elapsed = 0.0;
        const double limit = blockMs + 60.0;
        if (elapsed > limit)
            elapsed = limit;
        return ppq + elapsed * quartersPerMs;
    }

    // Pattern tick under the playhead, or a negative value while stopped,
    // in host pre-roll or resting between a short pattern bar and the next
    // host bar.
    double tickAt(double nowMs) const
    {
        if (!playing)
            return -1.0;
        const double position = ppqAt(nowMs);
        if (position < 0.0)
            return -1.0;
        return GgdTimeline::tickAt(anchor, shape, position);
    }
};
