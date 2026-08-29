/***************************************************************
 ** Copyright (C) 2016 by Andrew Shakinovsky
 **
 ** You may also use this code under the terms of the
 ** GPL v3 (see www.gnu.org/licenses).
 ** STOCHAS IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
 ** WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING
 ** MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE DISCLAIMED.
 ***************************************************************/
#ifndef STOCHAENGINE_H_
#define STOCHAENGINE_H_

#include "Constants.h"
#include "SequenceData.h"
#include "SeqRandom.h"
#include <bitset>

class StochaEngine {
   // PluginProcessorGgd.cpp compiles the original processor block as a dormant
   // fallback. That inherited code references the engine's inherited scheduler,
   // which intentionally remains private to normal callers.
   friend class SeqAudioProcessor;

   typedef std::bitset<SEQ_MAX_ROWS> DepSource;
   DepSource mDependencySource[SEQ_MAX_STEPS];

   struct MidiMappingItem {
      int mChannel;
      int mAction;
      int mValue;
      int mType;
      MidiMappingItem *mNext;
      MidiMappingItem() : mChannel(0), mAction(SEQMIDI_ACTION_INVALID), mValue(0),
         mType(0), mNext(nullptr) {}
   };

   MidiMappingItem *mMapping[128];
   bool mMappingIsValid = false;

   struct MidiOverride {
      bool mOverriden;
      int mValue;
      MidiOverride() : mOverriden(false), mValue(0) {}
      inline void override(int newval) { mValue = newval; mOverriden = true; }
      inline void clear() { mValue = 0; mOverriden = false; }
      inline int get(int defaultVal) { if (mOverriden) return mValue; return defaultVal; }
   };

   MidiOverride mOverridePattern;
   MidiOverride mOverrideMute;
   MidiOverride mOverrideSpeed;
   MidiOverride mOverrideTranspose;
   MidiOverride mOverrideNumSteps;
   MidiOverride mOverridePolyBias;
   MidiOverride mOverridePosVariance;
   MidiOverride mOverrideStepsPerMeasure;
   MidiOverride mOverrideDutyCycle;
   MidiOverride mOverrideVeloVariance;
   MidiOverride mOverrideLengthVariance;
   MidiOverride mOverrideOutputChannel;
   MidiOverride mOverrideMaxPoly;
   MidiOverride mOverrideSwing;

   struct StochaEvent {
      int mNumSamples;
      int8_t mNote;
      int8_t mVelo;
      int8_t mChan;
      StochaEvent *mCorrespondingNoteOff;
      StochaEvent() { clear(); }
      void clear() {
         mNumSamples = -1;
         mNote = -1;
         mVelo = -1;
         mChan = -1;
         mCorrespondingNoteOff = nullptr;
      }
   };

   struct PlayedEventNotification {
      int row = -1;
      int tick = -1;
      int velocity = 0;
   };
   static constexpr int playedEventQueueSize = 128;
   juce::AbstractFifo mPlayedEventFifo { playedEventQueueSize };
   PlayedEventNotification mPlayedEvents[playedEventQueueSize] {};
   void notifyPlayedEvent(int row, int tick, int velocity);

   int mNumActiveNoteOnEvents;
   int mNumActiveNoteOffEvents;
   int mCurrentStepPosition;
   double mRealStepPosition;
   SeqDataBuffer *mSeq;
   StochaEvent mEvents[SEQ_MAX_MIDI_EVENTS];
   int mLayer;
   struct SelectedItem {
      int rowToPlay;
      bool mandatory;
   };
   SelectedItem mMulti[SEQ_MAX_ROWS];

   uint64 mOldSeed;
   uint64 mOldSeq;
   SeqRandom mRand;
   double mOldStepPosInTrack;
   double mOldEventTickPosition = -1.0;
   double mPlayStartPosition;

   bool addMidiEvent(int startSamples, int8_t note,int8_t velo, int8_t chan, int numSamples);
   void quiesceMidi(bool moveNoteOffs=true);
   bool playPositionChange(int samples_per_step, int position, int samples_until);
   int getRandomSingle(int position);
   int getRandomMulti(int position, int maxpoly);
   void performMidiMapAction(int action, int value);
   void rebuildMappingSchema();
   int trimPoly(int maxpoly, int used);
   void compactArray(int used);
   bool isMandatory(int col, int row, bool *off, int pattern);

   // The inherited scheduler is compiled under this name by StochaEngineBeta.cpp.
   bool processLegacyBlock(double beatPosition,
      double sampleRate,
      int numSamplesInBlock,
      double BPM,
      double bpb
#ifdef CUBASE_HACKS
      , double beatPositionActual
#endif
   );

   bool processEventBlock(double beatPosition,
      double sampleRate,
      int numSamplesInBlock,
      double BPM,
      double bpb
#ifdef CUBASE_HACKS
      , double beatPositionActual
#endif
   );

public:
   StochaEngine();
   ~StochaEngine();
   void init(SeqDataBuffer *s,int layer);
   void setRandomSeed(uint64 seed, uint64 seqno);
   int getCurrentStepPosition(int *fraction = 0);
   int getCurrentOverallPosition(int *fraction = 0);
   int getNumSteps();
   int getPlayingPattern();
   bool getMuteState();
   void playbackStopped();
   void setPlaybackStartPosition(double pos);

   bool processBlock(double beatPosition,
      double sampleRate,
      int numSamplesInBlock,
      double BPM,
      double bpb
#ifdef CUBASE_HACKS
      , double beatPositionActual
#endif
   );

   bool getMidiEvent(int numSamplesInBlock, int *pos, int8_t *note, int8_t *velo, int8_t *chan);
   void doneBlock(int numSamplesInBlock);
   bool incomingMidiData(int type, int8_t number, int8_t chan, int8_t val);
   void resetMappingSchema();
   void resetMidiControl();
   bool getStepPlayedState(int position, int notenum);
   void setAutomationParameterValue(int paramId, int value);

   // UI-only feedback channel for visualising events that actually made it
   // through probability/mute decisions and into the generated MIDI queue.
   bool popPlayedEvent(int *row, int *tick, int *velocity);
   void clearPlayedEvents();
};

#endif
