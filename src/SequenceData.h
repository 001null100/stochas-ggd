/***************************************************************
 ** Copyright (C) 2016 by Andrew Shakinovsky
 **
 ** You may also use this code under the terms of the
 ** GPL v3 (see www.gnu.org/licenses).
 ** STOCHAS IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
 ** WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING
 ** MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE DISCLAIMED.
 ***************************************************************/

#ifndef SEQUENCEDATA_H_
#define SEQUENCEDATA_H_

/**
The complete project state that is saved with the host project. It is a plain
value type (no pointers) because the UI and audio threads each work on their
own copy, see SeqDataBuffer.

Stochas GGD stores drum hits as GGD event patterns. The inherited Stochas cell
matrix and chain tables are no longer held in memory: projects saved by early
alpha builds are converted to event patterns while they load (Persist.cpp).
*/

#include "Constants.h"
#include "GgdEventModel.h"
#include "GgdTimeline.h"

#include <atomic>
#include <cstdint>

// Stochas' inherited 14-character pattern-name ceiling is unnecessarily tight
// for a groove library. Names are persisted as strings, so the wider buffer is
// backwards-safe; older builds simply truncate a long name.
#ifdef SEQ_PATTERN_NAME_MAXLEN
#undef SEQ_PATTERN_NAME_MAXLEN
#endif
#define SEQ_PATTERN_NAME_MAXLEN 33 // 32 visible characters + terminator

#define FIFO_SIZE 32
class SeqFifo
{
public:
   struct DataMember {
      int value1;
      int value2;
      int value3;
   };
   SeqFifo() : abstractFifo(FIFO_SIZE) {}
   bool addToFifo(int value1, int value2, int value3) {
      bool ret = false;
      int start1, size1, start2, size2;
      abstractFifo.prepareToWrite(1, start1, size1, start2, size2);
      if (size1 > 0) {
         ret = true;
         myBuffer[start1].value1 = value1;
         myBuffer[start1].value2 = value2;
         myBuffer[start1].value3 = value3;
      }
      abstractFifo.finishedWrite(size1);
      return ret;
   }
   bool readFromFifo(int *value1, int *value2, int *value3)
   {
      bool ret = false;
      int start1, size1, start2, size2;
      abstractFifo.prepareToRead(1, start1, size1, start2, size2);
      if (size1 > 0) {
         *value1 = myBuffer[start1].value1;
         *value2 = myBuffer[start1].value2;
         *value3 = myBuffer[start1].value3;
         ret = true;
      }
      abstractFifo.finishedRead(size1);
      return ret;
   }

   void clearFifo()
   {
      abstractFifo.reset();
   }
private:
   AbstractFifo abstractFifo;
   DataMember myBuffer[FIFO_SIZE];
};

class SequenceLayer {
   friend class SequenceData;

   struct Pattern {
      GgdEventPattern mEvents;
      char mName[SEQ_PATTERN_NAME_MAXLEN];
      Pattern() {
         strncpy(mName, SEQ_DEFAULT_PAT_NAME, SEQ_PATTERN_NAME_MAXLEN);
         mName[SEQ_PATTERN_NAME_MAXLEN - 1] = 0;
      }
   };

   Pattern mPats[SEQ_MAX_PATTERNS];

   struct Note {
      int8_t note;
      char noteName[SEQ_MAX_NOTELABEL_LEN];
      Note() : note(0) {
         memset(noteName, 0, SEQ_MAX_NOTELABEL_LEN);
      }
   };

   struct NoteSet {
      Note notes[SEQ_MAX_ROWS];
   };

   enum WhichNoteSet {
      customSet = 0,
      standardSet,
      noteSetCount
   };
   NoteSet mNoteSets[noteSetCount];

   int mNumRows;
   int mNumSteps;
   bool mIsMonoMode;
   int mMaxPoly;
   int mPolyBias;
   WhichNoteSet mCurrentNoteSet;
   int mCurrentPattern;
   int mClockDivider;
   int8_t mMidiChannel;
   int mDutyCycle;
   int mStepsPerMeasure;
   char mStdKeyName[SEQ_KEY_NAME_MAXLEN];
   char mStdScaleName[SEQ_SCALE_NAME_MAXLEN];
   char mLayerName[SEQ_LAYER_NAME_MAXLEN];
   int  mStdOctave;
   bool mMuted;
   int mHumanLen;
   int mHumanVelo;
   int mHumanPos;
   bool mCombineMode;

public:
   SequenceLayer() { clear(); }

   void clear();

   void setMuted(bool muted) { mMuted = muted; }
   bool getMuted() const { return mMuted; }

   // GGD event patterns. Positions are GGD_EVENT_PPQ ticks per quarter note.
   GgdEventPattern *getEventPattern(int pat = -1);
   const GgdEventPattern *getEventPattern(int pat = -1) const;

   // Destination note per storage row (custom set) and the inherited
   // scale-derived note set kept for state compatibility.
   void setKeyScaleOct(const char *scale, const char *key, int octave);
   void getKeyScaleOct(const char **scale, const char **key, int *oct) const;
   void setNote(int row, int8_t val, bool custom);
   int8_t getNote(int row, bool custom) const;
   const char *getNoteName(int row) const;
   void setNoteName(int row, const char *name);
   int8_t getCurNote(int row) const;
   void setNoteSource(bool custom) { mCurrentNoteSet = custom ? customSet : standardSet; }
   bool noteSourceIsCustom() const { return mCurrentNoteSet == customSet; }

   int getMaxRows() const { return mNumRows; }
   void setMaxRows(int val);
   int getMaxPoly() const { return mMaxPoly; }
   void setMaxPoly(int val);
   int getPolyBias() const { return mPolyBias; }
   void setPolyBias(int val);
   int getNumSteps() const { return mNumSteps; }
   void setNumSteps(int val);
   bool isMonoMode() const { return mIsMonoMode; }
   void setMonoMode(bool val) { mIsMonoMode = val; }
   bool isCombineMode() const { return mCombineMode; }
   void setCombineMode(bool val) { mCombineMode = val; }
   int getClockDivider() const { return mClockDivider; }
   void setClockDivider(int c);
   int8_t getMidiChannel() const { return mMidiChannel; }
   void setMidiChannel(int8_t val);
   int getStepsPerMeasure() const { return mStepsPerMeasure; }
   void setStepsPerMeasure(int val);
   int getDutyCycle() const { return mDutyCycle; }
   void setDutyCycle(int val);
   void setHumanVelocity(int val);
   int getHumanVelocity() const { return mHumanVelo; }
   void setHumanPosition(int val);
   int getHumanPosition() const { return mHumanPos; }
   void setHumanLength(int val);
   int getHumanLength() const { return mHumanLen; }
   const char *getLayerName() const { return mLayerName; }
   void setLayerName(const char *txt);
   void setPatternName(const char *txt, int pat = -1);
   const char *getPatternName(int pat = -1) const;
   int getCurrentPattern() const { return mCurrentPattern; }
   void setCurrentPattern(int p);
};

struct SeqMidiMapItem {
   int8_t mAction;
   int8_t mTarget;
   int8_t mValue;
   int8_t mType;
   int8_t mNote;
   int8_t mChannel;
   SeqMidiMapItem(int8_t act, int8_t targ, int8_t val, int8_t type, int8_t note, int8_t chan) :
      mAction(act), mTarget(targ), mValue(val), mType(type), mNote(note), mChannel(chan) {}
   SeqMidiMapItem()  { clear(); }
   void clear();
};

extern SeqMidiMapItem gDefaultMidiMapItems[];

class SequenceData {
   SequenceLayer mLayers[SEQ_MAX_LAYERS];
   int mGroove[SEQ_DEFAULT_NUM_STEPS];
   int mSwing;
   int mMidiPassthru;
   int mMidiRespond;
   int mMidiMapCount;
   SeqMidiMapItem mMidiMap[SEQMIDI_MAX_ITEMS];
   int64 mRandomSeed;
   int mOffsetTime;
   int mAutoPlay;
   double mStandaloneBPM;
   GgdTimeline::SyncMode mSyncMode;

   void setDefaultMidiMapItems();
public:
   SequenceData();
   SequenceLayer *getLayer(int layer);
   const SequenceLayer *getLayer(int layer) const;

   // The groove table, MIDI map, offset time and auto-play mode belong to the
   // inherited Stochas feature set. They are kept so project state written by
   // any build round-trips unchanged; the GGD engine does not use them.
   int getGroove(int idx) const;
   void setGroove(int idx, int val);
   void clearGroove();
   void clearMapping();
   void setMappingCount(int count);
   int getMappingCount() const { return mMidiMapCount; }
   SeqMidiMapItem *getMappingItem(int ind);
   int getOffsetTime() const { return mOffsetTime; }
   void setOffsetTime(int ms) { mOffsetTime = ms; }
   int getAutoPlayMode() const { return mAutoPlay; }
   void setAutoPlayMode(int autoplay) { mAutoPlay = autoplay; }

   void clearLayer(int layer);
   void clearPattern(int layer, int pattern);
   void copyPatternData(int targLayer, int targPat, int srcLayer, int srcPat);

   void setSwing(int val);
   int getSwing() const { return mSwing; }
   void setMidiPassthru(int val);
   int getMidiPassthru() const { return mMidiPassthru; }
   void setMidiRespond(int val);
   int getMidiRespond() const { return mMidiRespond; }
   int64 getRandomSeed() const { return mRandomSeed; }
   void setRandomSeed(int64 val) { mRandomSeed = val; }
   double getStandaloneBPM() const { return mStandaloneBPM; }
   void setStandaloneBPM(double bpm);

   // How patterns line up with the host timeline (see GgdTimeline).
   GgdTimeline::SyncMode getSyncMode() const { return mSyncMode; }
   void setSyncMode(GgdTimeline::SyncMode mode) { mSyncMode = mode; }
};

/**
Double buffer shared by the message thread (editing) and the audio thread
(playback).

The UI edits getUISeqData() and calls swap() to publish. The audio thread
brackets every block with an AudioReadScope. swap() flips the buffer the audio
thread should read and then waits until any block that is still reading the
old buffer has finished before refreshing it, so the audio thread never sees
a half-written pattern.
*/
class SeqDataBuffer {
   std::atomic<int> mCurrent { 0 };
   std::atomic<int> mAudioReading { -1 };
   SequenceData mBuffer[2];

   SequenceData *beginAudioRead();
   void endAudioRead() { mAudioReading.store(-1); }

public:
   SeqDataBuffer() = default;

   // Publishes the UI copy to the audio thread.
   void swap();

   // State that is not being edited (host save, engine queries outside a
   // block) reads the published copy.
   SequenceData *getAudSeqData() { return &mBuffer[mCurrent.load()]; }
   SequenceData *getUISeqData() { return &mBuffer[mCurrent.load() ? 0 : 1]; }

   class AudioReadScope {
   public:
      explicit AudioReadScope(SeqDataBuffer &buffer) : owner(buffer), data(buffer.beginAudioRead()) {}
      ~AudioReadScope() { owner.endAudioRead(); }
      SequenceData &get() const { return *data; }
   private:
      SeqDataBuffer &owner;
      SequenceData *data;
      JUCE_DECLARE_NON_COPYABLE(AudioReadScope)
   };
};

// Lightweight audio -> UI status channel.
class SeqProcessorNotifier {
   std::atomic<int> mPlayPosition[SEQ_MAX_LAYERS];
   std::atomic<int> mCurrentPattern[SEQ_MAX_LAYERS];
   std::atomic<int> mMuteState[SEQ_MAX_LAYERS];
   std::atomic<int> mUINeedsUpdate { 0 };
public:
   SeqProcessorNotifier();
   bool doesUINeedUpdate() { return mUINeedsUpdate.exchange(0) != 0; }
   void uiNeedsUpdate() { mUINeedsUpdate.store(1); }
   int getPlayPosition(int layer) const { return mPlayPosition[layer].load(); }
   void setPlayPosition(int layer, int val) { mPlayPosition[layer].store(val); }
   int getCurrentPattern(int layer) const { return mCurrentPattern[layer].load(); }
   void setCurrentPattern(int layer, int val) { mCurrentPattern[layer].store(val); }
   bool getMuteState(int layer) const { return mMuteState[layer].load() == 1; }
   void setMuteState(int layer, bool val) { mMuteState[layer].store(val ? 1 : 0); }
};

#endif
