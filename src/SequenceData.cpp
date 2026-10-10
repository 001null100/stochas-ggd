/***************************************************************
 ** Copyright (C) 2016 by Andrew Shakinovsky
 **
 ** You may also use this code under the terms of the
 ** GPL v3 (see www.gnu.org/licenses).
 ** STOCHAS IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
 ** WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING
 ** MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE DISCLAIMED.
 ***************************************************************/
#include "SequenceData.h"
#include "Scale.h"

#include <memory>
#include <thread>

SeqMidiMapItem gDefaultMidiMapItems[SEQMIDI_NUM_DEFAULT_ITEMS] =
{
   // action, target, value, type, note, channel

   // change patterns for all
   { SEQMIDI_ACTION_CHGPAT,    SEQMIDI_TARGET_ALL,  1, SEQ_MIDI_NOTEON, 0, 1 },
   { SEQMIDI_ACTION_CHGPAT,    SEQMIDI_TARGET_ALL,  2, SEQ_MIDI_NOTEON, 1, 1 },
   { SEQMIDI_ACTION_CHGPAT,    SEQMIDI_TARGET_ALL,  3, SEQ_MIDI_NOTEON, 2, 1 },
   { SEQMIDI_ACTION_CHGPAT,    SEQMIDI_TARGET_ALL,  4, SEQ_MIDI_NOTEON, 3, 1 },
   { SEQMIDI_ACTION_CHGPAT,    SEQMIDI_TARGET_ALL,  5, SEQ_MIDI_NOTEON, 4, 1 },
   { SEQMIDI_ACTION_CHGPAT,    SEQMIDI_TARGET_ALL,  6, SEQ_MIDI_NOTEON, 5, 1 },
   { SEQMIDI_ACTION_CHGPAT,    SEQMIDI_TARGET_ALL,  7, SEQ_MIDI_NOTEON, 6, 1 },
   { SEQMIDI_ACTION_CHGPAT,    SEQMIDI_TARGET_ALL,  8, SEQ_MIDI_NOTEON, 7, 1 },

   // toggle mute for each layer
   { SEQMIDI_ACTION_MUTE,      1, SEQMIDI_VALUE_MUTE_TOGGLE, SEQ_MIDI_NOTEON, 9,  1 },
   { SEQMIDI_ACTION_MUTE,      2, SEQMIDI_VALUE_MUTE_TOGGLE, SEQ_MIDI_NOTEON, 10, 1 },
   { SEQMIDI_ACTION_MUTE,      3, SEQMIDI_VALUE_MUTE_TOGGLE, SEQ_MIDI_NOTEON, 11, 1 },
   { SEQMIDI_ACTION_MUTE,      4, SEQMIDI_VALUE_MUTE_TOGGLE, SEQ_MIDI_NOTEON, 12, 1 }
};

/*============================================
SeqProcessorNotifier
*============================================*/
SeqProcessorNotifier::SeqProcessorNotifier()
{
   for (int i = 0; i < SEQ_MAX_LAYERS; i++) {
      mPlayPosition[i].store(-1);
      mCurrentPattern[i].store(-1);
      mMuteState[i].store(0);
   }
}

/*============================================
SeqDataBuffer
*============================================*/
SequenceData *SeqDataBuffer::beginAudioRead()
{
   // Announce which buffer this block reads, then confirm that it is still
   // the published one. Together with swap() (which publishes first and then
   // inspects mAudioReading) this guarantees that either the audio thread
   // sees the new index or swap() sees the audio thread reading the old one.
   int index = mCurrent.load();
   mAudioReading.store(index);
   const int confirmed = mCurrent.load();
   if (confirmed != index) {
      index = confirmed;
      mAudioReading.store(index);
   }
   return &mBuffer[index];
}

void SeqDataBuffer::swap()
{
   const int published = mCurrent.load();     // the copy the audio thread reads now
   const int edited = published ? 0 : 1;      // the copy the UI just finished editing
   mCurrent.store(edited);

   // A block that started before the flip may still be reading the old copy.
   // Blocks are short; give up after a generous timeout in case a host stalls
   // its audio thread mid-block, which only reintroduces the old behaviour.
   const auto deadline = Time::getMillisecondCounterHiRes() + 250.0;
   while (mAudioReading.load() == published && Time::getMillisecondCounterHiRes() < deadline)
      std::this_thread::yield();

   // The old copy becomes the next UI working copy.
   mBuffer[published] = mBuffer[edited];
}

/*============================================
SequenceData
*============================================*/
void SequenceData::setDefaultMidiMapItems()
{
   setMappingCount(SEQMIDI_NUM_DEFAULT_ITEMS);
   for (int i = 0; i < SEQMIDI_NUM_DEFAULT_ITEMS; i++)
      *getMappingItem(i) = gDefaultMidiMapItems[i];
}

SequenceData::SequenceData() :
   mSwing(0),
   mMidiPassthru(SEQ_MIDI_PASSTHRU_NONE),
   mMidiRespond(SEQCTL_MIDI_RESPOND_YES),
   mMidiMapCount(0),
   mRandomSeed(0),
   mOffsetTime(0),
   mAutoPlay(SEQ_PLAYMODE_AUTO),
   mStandaloneBPM(SEQ_DEFAULT_STANDALONE_BPM),
   mSyncMode(GgdTimeline::SyncMode::hostBars)
{
   clearGroove();
   setDefaultMidiMapItems();
}

void SequenceData::setStandaloneBPM(double bpm)
{
   mStandaloneBPM = jlimit(1.0, 300.0, bpm);
}

SequenceLayer *SequenceData::getLayer(int layer)
{
   jassert(layer >= 0 && layer < SEQ_MAX_LAYERS);
   return &mLayers[layer];
}

const SequenceLayer *SequenceData::getLayer(int layer) const
{
   jassert(layer >= 0 && layer < SEQ_MAX_LAYERS);
   return &mLayers[layer];
}

int SequenceData::getGroove(int idx) const
{
   jassert(idx >= 0 && idx < SEQ_DEFAULT_NUM_STEPS);
   return mGroove[idx];
}

void SequenceData::setGroove(int idx, int val)
{
   jassert(idx >= 0 && idx < SEQ_DEFAULT_NUM_STEPS);
   mGroove[idx] = jlimit(-50, 50, val);
}

void SequenceData::clearGroove()
{
   for (int i = 0; i < SEQ_DEFAULT_NUM_STEPS; i++)
      mGroove[i] = 0;
}

void SequenceData::clearLayer(int layer)
{
   jassert(layer >= 0 && layer < SEQ_MAX_LAYERS);
   mLayers[layer].clear();
}

void SequenceData::clearPattern(int layer, int pattern)
{
   jassert(layer >= 0 && layer < SEQ_MAX_LAYERS);
   jassert(pattern >= 0 && pattern < SEQ_MAX_PATTERNS);
   mLayers[layer].mPats[pattern] = SequenceLayer::Pattern();
}

void SequenceData::copyPatternData(int targLayer, int targPat, int srcLayer, int srcPat)
{
   jassert(targLayer >= 0 && targLayer < SEQ_MAX_LAYERS);
   jassert(srcLayer >= 0 && srcLayer < SEQ_MAX_LAYERS);
   jassert(targPat >= 0 && targPat < SEQ_MAX_PATTERNS);
   jassert(srcPat >= 0 && srcPat < SEQ_MAX_PATTERNS);
   mLayers[targLayer].mPats[targPat] = mLayers[srcLayer].mPats[srcPat];
}

void SequenceData::clearMapping()
{
   for (int i = 0; i < SEQMIDI_MAX_ITEMS; i++)
      mMidiMap[i].clear();
}

void SequenceData::setMappingCount(int count)
{
   mMidiMapCount = jlimit(0, SEQMIDI_MAX_ITEMS - 1, count);
}

SeqMidiMapItem *SequenceData::getMappingItem(int ind)
{
   jassert(ind >= 0 && ind < SEQMIDI_MAX_ITEMS);
   return &mMidiMap[ind];
}

void SequenceData::setSwing(int val)
{
   mSwing = jlimit(SEQ_MIN_SWING, SEQ_MAX_SWING, val);
}

void SequenceData::setMidiPassthru(int val)
{
   switch (val) {
   case SEQ_MIDI_PASSTHRU_NONE:
   case SEQ_MIDI_PASSTHRU_UNHANDLED:
   case SEQ_MIDI_PASSTHRU_ALL:
      mMidiPassthru = val;
      break;
   default:
      break;
   }
}

void SequenceData::setMidiRespond(int val)
{
   switch (val) {
   case SEQ_MIDI_RESPOND_NO:
   case SEQ_MIDI_RESPOND_YES:
      mMidiRespond = val;
      break;
   default:
      break;
   }
}

/*============================================
SequenceLayer
*============================================*/
void SequenceLayer::clear()
{
   mMuted = false;
   mNumRows = SEQ_MAX_VISIBLE_ROWS;
   mNumSteps = SEQ_DEFAULT_NUM_STEPS;
   mIsMonoMode = true;
   mMaxPoly = SEQ_DEFAULT_MAX_POLY;
   mPolyBias = 0;
   mCurrentNoteSet = customSet;
   mCurrentPattern = 0;
   mClockDivider = SEQ_DEFAULT_CLOCK_DIV;
   mMidiChannel = SEQ_DEFAULT_MIDI_CHAN;
   mDutyCycle = SEQ_DUTY_DEFAULT;
   mStepsPerMeasure = SEQ_DEFAULT_NUM_STEPS;
   setKeyScaleOct(SEQ_DEFAULT_SCALE, SEQ_DEFAULT_KEY, SEQ_DEFAULT_OCTAVE);
   // custom notes start out as the standard notes
   mNoteSets[customSet] = mNoteSets[standardSet];
   for (int i = 0; i < SEQ_MAX_PATTERNS; i++)
      mPats[i] = Pattern();

   strncpy(mLayerName, SEQ_DEFAULT_LAYER_NAME, SEQ_LAYER_NAME_MAXLEN);
   mLayerName[SEQ_LAYER_NAME_MAXLEN - 1] = 0;

   mHumanLen = 0;
   mHumanPos = 0;
   mHumanVelo = 0;
   mCombineMode = false;
}

GgdEventPattern *SequenceLayer::getEventPattern(int pat)
{
   if (pat < 0)
      pat = mCurrentPattern;
   jassert(pat >= 0 && pat < SEQ_MAX_PATTERNS);
   return &mPats[pat].mEvents;
}

const GgdEventPattern *SequenceLayer::getEventPattern(int pat) const
{
   if (pat < 0)
      pat = mCurrentPattern;
   jassert(pat >= 0 && pat < SEQ_MAX_PATTERNS);
   return &mPats[pat].mEvents;
}

void SequenceLayer::setKeyScaleOct(const char *scale, const char *key, int octave)
{
   octave = jlimit(0, SEQ_NUM_OCTAVES - 1, octave);

   strncpy(mStdKeyName, key, SEQ_KEY_NAME_MAXLEN);
   mStdKeyName[SEQ_KEY_NAME_MAXLEN - 1] = 0;
   strncpy(mStdScaleName, scale, SEQ_SCALE_NAME_MAXLEN);
   mStdScaleName[SEQ_SCALE_NAME_MAXLEN - 1] = 0;
   mStdOctave = octave;

   SeqScale iterator;
   iterator.startIterateNotesInScale(mStdScaleName, mStdKeyName, octave);

   for (int i = 0; i < SEQ_MAX_ROWS - 1; i++)
      mNoteSets[standardSet].notes[(SEQ_MAX_ROWS - 2) - i].note = iterator.getNextNote();
   mNoteSets[standardSet].notes[SEQ_MAX_ROWS - 1].note = -1; // "off"
}

void SequenceLayer::getKeyScaleOct(const char **scale, const char **key, int *oct) const
{
   *oct = mStdOctave;
   *key = mStdKeyName;
   *scale = mStdScaleName;
}

void SequenceLayer::setNote(int row, int8_t val, bool custom)
{
   jassert(row >= 0 && row < SEQ_MAX_ROWS);
   mNoteSets[custom ? customSet : standardSet].notes[row].note = val;
}

int8_t SequenceLayer::getNote(int row, bool custom) const
{
   jassert(row >= 0 && row < SEQ_MAX_ROWS);
   return mNoteSets[custom ? customSet : standardSet].notes[row].note;
}

const char *SequenceLayer::getNoteName(int row) const
{
   jassert(row >= 0 && row < SEQ_MAX_ROWS);
   return mNoteSets[customSet].notes[row].noteName;
}

void SequenceLayer::setNoteName(int row, const char *name)
{
   jassert(row >= 0 && row < SEQ_MAX_ROWS);
   auto &target = mNoteSets[customSet].notes[row].noteName;
   memset(target, 0, SEQ_MAX_NOTELABEL_LEN);
   strncpy(target, name, SEQ_MAX_NOTELABEL_LEN - 1);
}

int8_t SequenceLayer::getCurNote(int row) const
{
   if (row < 0 || row >= SEQ_MAX_ROWS)
      return -1;
   return mNoteSets[mCurrentNoteSet].notes[row].note;
}

void SequenceLayer::setMaxRows(int val)
{
   mNumRows = jlimit(SEQ_MIN_ROWS, SEQ_MAX_ROWS, val);
}

void SequenceLayer::setMaxPoly(int val)
{
   mMaxPoly = jlimit(1, SEQ_MAX_ROWS, val);
}

void SequenceLayer::setPolyBias(int val)
{
   mPolyBias = jlimit(SEQ_POLY_BIAS_MIN, SEQ_POLY_BIAS_MAX, val);
}

void SequenceLayer::setNumSteps(int val)
{
   mNumSteps = jlimit(SEQ_MIN_STEPS, SEQ_MAX_STEPS, val);
}

void SequenceLayer::setClockDivider(int c)
{
   mClockDivider = jlimit(SEQ_MIN_CLOCK_DIV, SEQ_MAX_CLOCK_DIV, c);
}

void SequenceLayer::setMidiChannel(int8_t val)
{
   mMidiChannel = (int8_t)jlimit(1, 16, (int)val);
}

void SequenceLayer::setStepsPerMeasure(int val)
{
   mStepsPerMeasure = jlimit(SEQ_MIN_STEPS, SEQ_MAX_STEPS_PER_MEASURE, val);
}

void SequenceLayer::setDutyCycle(int val)
{
   mDutyCycle = jlimit(SEQ_DUTY_MIN, SEQ_DUTY_MAX, val);
}

void SequenceLayer::setHumanVelocity(int val)
{
   mHumanVelo = jlimit(0, SEQ_MAX_HUMAN_VELOCITY, val);
}

void SequenceLayer::setHumanPosition(int val)
{
   mHumanPos = jlimit(0, SEQ_MAX_HUMAN_POSITION, val);
}

void SequenceLayer::setHumanLength(int val)
{
   mHumanLen = jlimit(0, SEQ_MAX_HUMAN_LENGTH, val);
}

void SequenceLayer::setLayerName(const char *txt)
{
   strncpy(mLayerName, txt, SEQ_LAYER_NAME_MAXLEN);
   mLayerName[SEQ_LAYER_NAME_MAXLEN - 1] = 0;
}

void SequenceLayer::setPatternName(const char *txt, int pat)
{
   if (pat == -1)
      pat = mCurrentPattern;
   jassert(pat >= 0 && pat < SEQ_MAX_PATTERNS);
   char *dest = mPats[pat].mName;
   strncpy(dest, txt, SEQ_PATTERN_NAME_MAXLEN);
   dest[SEQ_PATTERN_NAME_MAXLEN - 1] = 0;

   // Names are UTF-8. Never leave half a multi-byte character behind when
   // a long name is cut to fit.
   const int length = (int)strlen(dest);
   int start = length;
   while (start > 0 && ((unsigned char)dest[start - 1] & 0xC0) == 0x80)
      --start;
   if (start > 0) {
      const unsigned char lead = (unsigned char)dest[start - 1];
      const int expected = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
      if (length - (start - 1) < expected)
         dest[start - 1] = 0;
   }
}

const char *SequenceLayer::getPatternName(int pat) const
{
   if (pat == -1)
      pat = mCurrentPattern;
   jassert(pat >= 0 && pat < SEQ_MAX_PATTERNS);
   return mPats[pat].mName;
}

void SequenceLayer::setCurrentPattern(int p)
{
   mCurrentPattern = jlimit(0, SEQ_MAX_PATTERNS - 1, p);
}

/*============================================
SeqMidiMapItem
*============================================*/
void SeqMidiMapItem::clear()
{
   mAction = SEQMIDI_ACTION_INVALID;
   mTarget = 0;
   mValue = 0;
   mType = 0;
   mNote = 0;
   mChannel = 1;
}
