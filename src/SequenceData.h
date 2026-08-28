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
This maintains a model of the current state. It includes everything needed to save/load
a patch or preset. It does not include user preferences. It does not include current state
of midi-switched values (eg midi pattern change). That state is stored in stocha engine
and overrides the state here (so you will not see it here)

Do not add pointers to any of these!
*/

#include "Constants.h"
#include "Scale.h"
#include "GgdEventModel.h"
#include <cstdint>

class SequenceData;

#define FIFO_SIZE 16
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
      abstractFifo.finishedWrite(size1 + size2);
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
      abstractFifo.finishedRead(size1 + size2);
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

   // Legacy cell data is retained only for backwards project migration and
   // inherited Stochas features. Beta drum playback/editing uses mEvents below.
   struct Cell {
      int8_t prob;
      int8_t velo;
      int8_t length;
      int8_t offset;
      Cell() :prob(-1), velo(0), length(0), offset(0) {}
   };

   struct Row {
      Cell mSteps[SEQ_MAX_STEPS];
   };

   struct SourceCell {
      uint16_t col;
      unsigned char row;
      unsigned char targetrow;
      int8_t flags;
      SourceCell() : col(0), row(0), targetrow(0), flags(0) {}
   };
   struct ChainSource {
      SourceCell cells[SEQ_MAX_CHAIN_SOURCES];
   };

   struct Pattern {
      Row mRows[SEQ_MAX_ROWS];
      ChainSource mChains[SEQ_MAX_STEPS];
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

   void setMuted(bool muted);
   bool getMuted();

   bool addChainSource(int row, int step, int sourceRow, int sourceStep, bool negtgt, bool negsrc, int pat=-1);
   int getNumChainSources(int row, int step, int pat=-1);
   bool getChainSource(int row, int step, int *iterate, int *sourceRow, int *sourceCol, bool *negtgt, bool *negsrc, int pat=-1);
   bool getChainTarget(int row, int step, int *iterate, int *targRow, int *targCol, bool *negtgt, bool *negsrc, int pat = -1);

#if 0
   bool getChainSourceRaw(int pat, int step, int idx, int *col, int *row, int *targRow, int *flags);
#endif

   void clearChainSources(int row, int step, int pat=-1);

   void setKeyScaleOct(const char *scale, const char *key, int octave);
   void getKeyScaleOct(const char **scale, const char **key, int *oct);
   void copyScaleData(const SequenceLayer &src);

   // Legacy cell API.
   void setVel(int row, int step, int8_t vel, int pat=-1);
   int8_t getVel(int row, int step, int pat = -1);
   void setProb(int row, int step, int8_t prob, int pat=-1);
   int8_t getProb(int row, int step, int pat = -1);
   void setLength(int row, int step, int8_t length, int pat=-1);
   int8_t getLength(int row, int step, int pat = -1);
   void setOffset(int row, int step, int8_t length, int pat = -1);
   int8_t getOffset(int row, int step, int pat = -1);
   void clearCell(int row, int step);
   void copyCell(int targRow, int targStep, int srcRow, int srcStep);

   // Beta event API. Events are the authoritative drum representation once a
   // pattern is active. Event positions are GGD_EVENT_PPQ ticks per quarter.
   GgdEventPattern *getEventPattern(int pat = -1);
   const GgdEventPattern *getEventPattern(int pat = -1) const;
   bool legacyPatternHasData(int pat = -1) const;
   void migrateLegacyPatternToEvents(int pat,
                                     int numerator,
                                     int denominator,
                                     int bars);

   void setNote(int row, int8_t val, bool custom);
   int8_t getNote(int row, bool custom);
   char *getNoteName(int row);
   void setNoteName(int row, const char *name);
   int8_t getCurNote(int row);
   int getRowForNote(int8_t note);
   void setNoteSource(bool custom);
   bool noteSourceIsCustom();

   int getMaxRows();
   void setMaxRows(int val);
   int getMaxPoly();
   void setMaxPoly(int val);
   int getPolyBias();
   void setPolyBias(int val);
   int getNumSteps();
   void setNumSteps(int val);
   bool isMonoMode();
   void setMonoMode(bool val);
   bool isCombineMode();
   void setCombineMode(bool val);
   int getClockDivider();
   void setClockDivider(int c);
   int8_t getMidiChannel();
   void setMidiChannel(int8_t val);
   int getStepsPerMeasure();
   void setStepsPerMeasure(int val);
   int getDutyCycle();
   void setDutyCycle(int val);
   void setHumanVelocity(int val);
   int getHumanVelocity();
   void setHumanPosition(int val);
   int getHumanPosition();
   void setHumanLength(int val);
   int getHumanLength();
   const char *getLayerName();
   void setLayerName(const char *txt);
   void setPatternName(const char *txt, int pat = -1);
   const char *getPatternName(int pat = -1);
   int getCurrentPattern();
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

   void setDefaultMidiMapItems();
public:
   SequenceData();
   SequenceLayer *getLayer(int layer);
   int getGroove(int idx);
   void setGroove(int idx, int val);
   void clearLayer(int layer);
   void clearPattern(int layer, int pattern);
   void clearGroove();
   void clearMapping();
   void setMappingCount(int count);
   int getMappingCount();
   SeqMidiMapItem *getMappingItem(int ind);
   void setSwing(int val);
   int getSwing();
   void setMidiPassthru(int val);
   int getMidiPassthru();
   void setMidiRespond(int val);
   int getMidiRespond();
   void copyLayer(int targLayer, int srcLayer);
   void copyScaleData(int targLayer, int srcLayer);
   void copyPatternData(int targLayer, int targPat, int srcLayer, int srcPat);
   int64 getRandomSeed();
   void setRandomSeed(int64 val);
   int getOffsetTime();
   void setOffsetTime(int ms);
   int getAutoPlayMode();
   void setAutoPlayMode(int autoplay);
   double getStandaloneBPM();
   void setStandaloneBPM(double bpm);
};

typedef void(*ChangeCallback)(void *);
class SeqDataBuffer {
   Atomic<int> mCurrent;
   SequenceData mBuffer[2];
   ChangeCallback mCB;
   void *mCBHandle;
   SequenceData mUndoBuffer;
public:
   SeqDataBuffer() : mCurrent(0), mCB(0) , mCBHandle(0) {}
   void swap();
   void undo();
   void setChangeNotify(ChangeCallback cb, void *cbhandle);

   SequenceData *getAudSeqData() {
      return &mBuffer[mCurrent.get()];
   }

   SequenceData *getUISeqData() {
      return &mBuffer[mCurrent.get() ? 0 : 1];
   }
};

class SeqProcessorNotifierHelper {
public:
   virtual bool getStepPlayedState(int layer, int position, int notenum) = 0;
};

class SeqProcessorNotifier {
   Atomic<int> mPlayPosition[SEQ_MAX_LAYERS];
   Atomic<int> mCurrentPattern[SEQ_MAX_LAYERS];
   Atomic<int> mMuteState[SEQ_MAX_LAYERS];
   Atomic<int> mMidiEvent;
   Atomic<int> mUINeedsUpdate;
   Atomic<int64> mRandomSeed;
   Atomic<int> mRecordingState;
   Atomic<int> mManualPlayingState;
   SeqProcessorNotifierHelper *mNotifierHelper;
   SeqFifo mCompletedNoteFifo;
public:
   SeqProcessorNotifier(SeqProcessorNotifierHelper *hlpr);
   enum PlayRecordState {
      off,
      on,
      standby
   };
   PlayRecordState getRecordingState();
   PlayRecordState getPlaybackState();
   bool getCompletedMidiNote(int *number, int *velocity,
      int *len,
      int *pos);
   void clearCompletedMidiNotes();
   bool doesUINeedUpdate();
   int getPlayPosition(int layer);
   int getCurrentPattern(int layer);
   bool getMidiEventOccurred(int8_t *type, int8_t *channel, int8_t *number, int8_t *value);
   bool getMuteState(int layer);
   int64 getRandomSeed();
   bool getStepPlayedState(int layer, int position, int notenum);
   void setMidiEventOccurred(int8_t type, int8_t channel, int8_t number, int8_t value);
   void setPlayPosition(int layer, int val);
   void setCurrentPattern(int layer, int val);
   void setMuteState(int layer, bool val);
   void uiNeedsUpdate();
   void setRandomSeed(int64 seed);
   void addCompletedMidiNote(int number, int velocity,
      int len, int pos);
   void setRecordingState(PlayRecordState state);
   void setPlaybackState(PlayRecordState playing);
};

#endif
