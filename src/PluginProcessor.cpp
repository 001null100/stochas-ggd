/***************************************************************
 ** Copyright (C) 2016 by Andrew Shakinovsky
 **
 ** You may also use this code under the terms of the
 ** GPL v3 (see www.gnu.org/licenses).
 ** STOCHAS IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
 ** WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING
 ** MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE DISCLAIMED.
 ***************************************************************/

#include "PluginProcessor.h"
#include "GgdDrumEditor.h"
#include "Persist.h"
#include "Scale.h"

namespace
{
std::uint64_t mixSeed(std::uint64_t value)
{
   value += 0x9e3779b97f4a7c15ULL;
   value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
   value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
   return value ^ (value >> 31);
}
}

void SeqAudioProcessor::addAutParam(SeqAudioProcessorParameter * p)
{
   addParameter(p);
   mAutomationParameters.add(p);
}

SeqAudioProcessor::SeqAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
   : AudioProcessor(BusesProperties().withOutput("Output", AudioChannelSet::stereo(), true))
#endif
{
   for (int i = 0; i < SEQ_MAX_LAYERS; i++)
      mEngines[i].setLayer(i);

   // The parameter list is part of the host contract: hosts address
   // automation by index, so the inherited order and ranges are kept as is.
   for (int i = 0; i < SEQ_MAX_LAYERS; i++) {
      addAutParam(new SeqAudioProcessorParameter(this, SEQ_AUT_CLOCKDIV, 0, SEQ_NUM_CLOCK_DIVS-1, i,"speed"));
      addAutParam(new SeqAudioProcessorParameter(this, SEQ_AUT_NUMSTEPS, SEQ_MIN_STEPS, SEQ_MAX_STEPS, i,"steps"));
      addAutParam(new SeqAudioProcessorParameter(this, SEQ_AUT_STEPS_PER_MEASURE, SEQ_MIN_STEPS, SEQ_MAX_STEPS_PER_MEASURE, i,"steps/measure"));
      addAutParam(new SeqAudioProcessorParameter(this, SEQ_AUT_NOTE_LENGTH, SEQ_DUTY_MIN, SEQ_DUTY_MAX, i,"note length"));
      addAutParam(new SeqAudioProcessorParameter(this, SEQ_AUT_POS_VARIANCE, 0, SEQ_MAX_HUMAN_POSITION, i,"pos variance"));
      addAutParam(new SeqAudioProcessorParameter(this, SEQ_AUT_VELO_VARIANCE, 0, SEQ_MAX_HUMAN_VELOCITY, i,"velo variance"));
      addAutParam(new SeqAudioProcessorParameter(this, SEQ_AUT_LENGTH_VARIANCE, 0, SEQ_MAX_HUMAN_LENGTH, i,"len variance"));
      addAutParam(new SeqAudioProcessorParameter(this, SEQ_AUT_MUTED, 0, 1, i,"muted"));
      addAutParam(new SeqAudioProcessorParameter(this, SEQ_AUT_OUTPUT_CHANNEL, 1, 16, i,"midi channel"));
      addAutParam(new SeqAudioProcessorParameter(this, SEQ_AUT_MAX_POLY, 1, SEQ_MAX_ROWS, i,"max poly"));
      addAutParam(new SeqAudioProcessorParameter(this, SEQ_AUT_POLY_BIAS, SEQ_POLY_BIAS_MIN, SEQ_POLY_BIAS_MAX, i,"poly bias"));
      addAutParam(new SeqAudioProcessorParameter(this, SEQ_AUT_TRANSPOSE, SEQMIDI_VALUE_TRANSPOSE_MIN, SEQMIDI_VALUE_TRANSPOSE_MAX, i, "transpose"));
      addAutParam(new SeqAudioProcessorParameter(this, SEQ_AUT_CURRENT_PATTERN, 0, SEQ_MAX_PATTERNS-1, i,"pattern"));
   }
   addAutParam(new SeqAudioProcessorParameter(this, SEQ_AUT_GLOBAL_SWING, SEQ_MIN_SWING, SEQ_MAX_SWING, -1,"swing"));
   addAutParam(new SeqAudioProcessorParameter(this, SEQ_AUT_RECORD, 0, 1, -1,"record"));
   addParameter(new SeqPlaybackParameter());

   // A fresh instance starts as a configured GGD drum layer, even if the
   // editor is never opened.
   mProject.normalise(*mData.getUISeqData());
   mData.swap();

   mStandaloneStartMs = Time::getMillisecondCounterHiRes();
}

SeqAudioProcessor::~SeqAudioProcessor() = default;

const String SeqAudioProcessor::getName() const
{
   return JucePlugin_Name;
}

bool SeqAudioProcessor::acceptsMidi() const
{
   return true;
}

bool SeqAudioProcessor::producesMidi() const
{
   return true;
}

bool SeqAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
   return true;
#else
   return false;
#endif
}

double SeqAudioProcessor::getTailLengthSeconds() const
{
   return 0.0;
}

int SeqAudioProcessor::getNumPrograms()
{
   return 1; // some hosts misbehave with 0 programs
}

int SeqAudioProcessor::getCurrentProgram()
{
   return 0;
}

void SeqAudioProcessor::setCurrentProgram (int)
{
}

const String SeqAudioProcessor::getProgramName (int)
{
   return String();
}

void SeqAudioProcessor::changeProgramName (int , const String& )
{
}

void SeqAudioProcessor::prepareToPlay (double, int)
{
   // A (re)activation starts from a clean transport state.
   for (auto &engine : mEngines)
      engine.playbackStopped();
   mPlaying = false;
}

void SeqAudioProcessor::releaseResources()
{
}

bool SeqAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
   ignoreUnused (layouts);
   return true;
#else
   if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono()
    && layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
      return false;
  #if ! JucePlugin_IsSynth
   if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
      return false;
  #endif
   return true;
#endif
}

std::uint64_t SeqAudioProcessor::nextRandomSeed()
{
   return mixSeed((std::uint64_t)Time::getHighResolutionTicks() ^ mixSeed(++mSeedCounter));
}

bool SeqAudioProcessor::readTransport(GgdTimeline::HostPosition &host, bool &playing, int)
{
   playing = false;

   if (wrapperType == wrapperType_Standalone) {
      // Standalone builds have no host transport: run a 4/4 clock.
      const double bpm = mData.getAudSeqData()->getStandaloneBPM();
      host.bpm = bpm;
      host.ppq = (Time::getMillisecondCounterHiRes() - mStandaloneStartMs) * bpm / 60000.0;
      host.hasTimeSignature = true;
      host.numerator = 4;
      host.denominator = 4;
      playing = true;
      return true;
   }

   auto *hostPlayHead = getPlayHead();
   if (hostPlayHead == nullptr)
      return false;

   const auto position = hostPlayHead->getPosition();
   if (!position.hasValue())
      return false;

   const auto ppq = position->getPpqPosition();
   if (!ppq.hasValue())
      return false;

   playing = position->getIsPlaying();
   host.ppq = *ppq;
   host.bpm = position->getBpm().orFallback(120.0);
   if (!(host.bpm > 0.0))
      host.bpm = 120.0;

   if (const auto barStart = position->getPpqPositionOfLastBarStart()) {
      host.hasBarStart = true;
      host.barStartPpq = *barStart;
   }
   if (const auto barCount = position->getBarCount()) {
      host.hasBarNumber = true;
      host.barNumber = *barCount;
   }
   if (const auto signature = position->getTimeSignature()) {
      if (GgdTimeline::isUsableMeter(signature->numerator, signature->denominator)) {
         host.hasTimeSignature = true;
         host.numerator = signature->numerator;
         host.denominator = signature->denominator;
      }
   }
   return true;
}

void SeqAudioProcessor::handleUIMessages(const SequenceData &data, MidiBuffer &output)
{
   // Articulation audition from the editor: note on/off for a storage row.
   int message = 0, layerIndex = 0, row = 0;
   while (mIncomingData.readFromFifo(&message, &layerIndex, &row)) {
      if (message != SEQ_MIDI_NOTEON && message != SEQ_MIDI_NOTEOFF)
         continue;
      const auto *layer = data.getLayer(jlimit(0, SEQ_MAX_LAYERS - 1, layerIndex));
      const int note = layer->getCurNote(row);
      if (note < 0)
         continue;
      const int channel = jlimit(1, 16, (int)layer->getMidiChannel());
      output.addEvent(message == SEQ_MIDI_NOTEON
                         ? MidiMessage::noteOn(channel, note, (uint8)127)
                         : MidiMessage::noteOff(channel, note),
                      0);
   }
}

void SeqAudioProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
   buffer.clear();
   const int numSamples = buffer.getNumSamples();
   MidiBuffer output;

   SeqDataBuffer::AudioReadScope read(mData);
   const SequenceData &data = read.get();

   // Incoming MIDI (live playing, Bitwig clips) passes through untouched.
   if (data.getMidiPassthru() != SEQ_MIDI_PASSTHRU_NONE)
      output.addEvents(midiMessages, 0, -1, 0);

   handleUIMessages(data, output);

   GgdTimeline::HostPosition host;
   bool playing = false;
   const bool haveTransport = readTransport(host, playing, numSamples);

   if (!playing || !haveTransport) {
      if (mPlaying) {
         mPlaying = false;
         for (auto &engine : mEngines)
            engine.playbackStopped();
         // Automation overrides only last while the transport runs.
         for (auto *parameter : mAutomationParameters)
            if (!parameter->isAssigned())
               parameter->reset();
      }
   }
   else {
      // Each new pass over the timeline gets fresh probability decisions
      // unless the project pins a seed.
      if (!mPlaying || host.ppq < mLastPpq - 1.0e-6) {
         const auto pinned = (std::uint64_t)data.getRandomSeed();
         mRandomSeed = pinned != 0 ? pinned : nextRandomSeed();
      }
      mPlaying = true;
      mLastPpq = host.ppq;

      GgdEngine::BlockContext context;
      context.host = host;
      context.sampleRate = getSampleRate();
      context.numSamples = numSamples;
      context.syncMode = data.getSyncMode();
      context.randomSeed = mRandomSeed;
      context.wallMs = Time::getMillisecondCounterHiRes();
      for (auto &engine : mEngines)
         engine.processBlock(data, context);
   }

   // Due notes, including note-offs flushed by a stop.
   GgdEngine::MidiOut note;
   for (auto &engine : mEngines) {
      while (engine.popMidiEvent(numSamples, note)) {
         output.addEvent(note.velocity > 0
                            ? MidiMessage::noteOn(note.channel, note.note, (uint8)note.velocity)
                            : MidiMessage::noteOff(note.channel, note.note),
                         note.samplePosition);
      }
      engine.finishBlock(numSamples);
   }

   midiMessages.swapWith(output);

   for (int i = 0; i < SEQ_MAX_LAYERS; i++) {
      mNotifier.setPlayPosition(i, mPlaying ? mEngines[i].getCurrentStepPosition() : -1);
      mNotifier.setCurrentPattern(i, mPlaying ? mEngines[i].getPlayingPattern(data) : -1);
      mNotifier.setMuteState(i, mEngines[i].isMuted(data));
   }
}

void SeqAudioProcessor::publishEdit(bool markProjectDirty)
{
   mData.swap();
   if (markProjectDirty)
      updateHostDisplay(ChangeDetails().withNonParameterStateChanged(true));
}

bool SeqAudioProcessor::hasEditor() const
{
   return true;
}

AudioProcessorEditor* SeqAudioProcessor::createEditor()
{
   return new GgdDrumEditor(*this);
}

void SeqAudioProcessor::getStateInformation (MemoryBlock& block)
{
   SeqPersist persist;
   copyXmlToBinary(persist.store(mData.getAudSeqData()), block);
}

void SeqAudioProcessor::setStateInformation (const void* in , int size)
{
   if (auto xml = getXmlFromBinary(in, size)) {
      SeqPersist persist;
      auto *target = mData.getUISeqData();
      if (persist.retrieve(target, xml.get())) {
         mProject.normalise(*target);
         mData.swap();
      }
   }

   // the editor (if open) reloads everything from the new state
   mNotifier.uiNeedsUpdate();
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
   return new SeqAudioProcessor();
}

//==================================SeqAudioProcessorParameter===========================

namespace
{
// Some hosts deliver stacked automation slightly outside 0..1.
float clampAutRange(float v)
{
   return jlimit(0.0f, 1.0f, v);
}
}

SeqAudioProcessorParameter::SeqAudioProcessorParameter(AutParamNotify * notify,
   int paramId, int rangeLo, int rangeHi, int layerNum, const String &name) :
   AudioProcessorParameter(1), // version hint; bump only for parameters added later
   mNotify(notify), mLayerNumber(layerNum), mParamId(paramId), mRangeLo(rangeLo),
   mRangeHi(rangeHi), mValue(rangeLo-1), mName(name)
{
}

float SeqAudioProcessorParameter::getValue() const
{
   return jmap<float>((float)mValue.load(), (float)mRangeLo - 1.0f, (float)mRangeHi, 0.0f, 1.0f);
}

void SeqAudioProcessorParameter::setValue(float newValue)
{
   const int value = roundToInt(jmap<float>(clampAutRange(newValue), (float)mRangeLo - 1.0f, (float)mRangeHi));
   mValue.store(value);
   mNotify->automationParameterHasChanged(mParamId, value == mRangeLo - 1 ?
      SEQ_AUT_DEFAULT_VALUE_DESIG : value, mLayerNumber);
}

float SeqAudioProcessorParameter::getDefaultValue() const
{
   return 0.0f;
}

String SeqAudioProcessorParameter::getName(int maximumStringLength) const
{
   String ret = mLayerNumber != -1 ? String::formatted("L%d ", mLayerNumber+1) : String();
   ret.append(mName, maximumStringLength - ret.length());
   return ret;
}

String SeqAudioProcessorParameter::getLabel() const
{
   return String();
}

String SeqAudioProcessorParameter::getText(float value, int) const
{
   const int v = roundToInt(jmap<float>(clampAutRange(value), (float)mRangeLo - 1.0f, (float)mRangeHi));
   if (v == mRangeLo - 1)
      return "as assigned";
   return mNotify->getTextForAutomationParameterValue(mParamId, v);
}

float SeqAudioProcessorParameter::getValueForText(const String & text) const
{
   if (text.compare("as assigned") == 0)
      return 0.0f;

   const int r = mNotify->parseTextForAutomationParameterValue(mParamId, text);
   if (r == SEQ_AUT_DEFAULT_VALUE_DESIG)
      return 0.0f;
   return jmap<float>((float)jlimit(mRangeLo, mRangeHi, r), (float)mRangeLo - 1.0f, (float)mRangeHi, 0.0f, 1.0f);
}

void SeqAudioProcessorParameter::reset()
{
   beginChangeGesture();
   setValueNotifyingHost(getDefaultValue());
   endChangeGesture();
}

//==========implement AutParamNotify ============================
void SeqAudioProcessor::automationParameterHasChanged(int paramId, int paramValue, int layer)
{
   // Recording belonged to the inherited Stochas editor. The parameter stays
   // registered so existing automation lanes keep their slots.
   if(paramId == SEQ_AUT_RECORD)
      return;

   if(layer != -1)
      mEngines[jlimit(0, SEQ_MAX_LAYERS - 1, layer)].setAutomationParameterValue(paramId, paramValue);
   else {
      for(auto &engine : mEngines)
         engine.setAutomationParameterValue(paramId, paramValue);
   }
}

String SeqAudioProcessor::getTextForAutomationParameterValue(int paramId, int paramValue)
{
   switch (paramId) {
   case SEQ_AUT_CLOCKDIV:
      return SeqScale::getClockDividerTextAndId(jlimit(0, SEQ_NUM_CLOCK_DIVS - 1, paramValue));
   case SEQ_AUT_TRANSPOSE:
      return SeqScale::getTransposeText(jlimit(SEQMIDI_VALUE_TRANSPOSE_MIN, SEQMIDI_VALUE_TRANSPOSE_MAX, paramValue));
   case SEQ_AUT_NOTE_LENGTH:
   case SEQ_AUT_POS_VARIANCE:
   case SEQ_AUT_VELO_VARIANCE:
   case SEQ_AUT_LENGTH_VARIANCE:
   case SEQ_AUT_GLOBAL_SWING:
      return String::formatted("%d%%", paramValue);
   case SEQ_AUT_RECORD:
      return paramValue == 0 ? "off" : "on";
   case SEQ_AUT_MUTED:
      return paramValue ? "yes" : "no";
   case SEQ_AUT_CURRENT_PATTERN: // shown 1-based
      return String::formatted("%d", paramValue+1);
   default:
      return String::formatted("%d", paramValue);
   }
}

int SeqAudioProcessor::parseTextForAutomationParameterValue(int paramId, String text)
{
   int res = SEQ_AUT_DEFAULT_VALUE_DESIG;
   switch (paramId) {
   case SEQ_AUT_CLOCKDIV:
      for (int i = 0; i < SEQ_NUM_CLOCK_DIVS; i++) {
         if (text.compare(SeqScale::getClockDividerTextAndId(i)) == 0) {
            res = i;
            break;
         }
      }
      break;
   case SEQ_AUT_TRANSPOSE:
      for (int i = SEQMIDI_VALUE_TRANSPOSE_MIN; i <= SEQMIDI_VALUE_TRANSPOSE_MAX; i++) {
         if (text.compare(SeqScale::getTransposeText(i)) == 0) {
            res = i;
            break;
         }
      }
      break;
   case SEQ_AUT_RECORD:
      if(text.compare("on")==0)
         res = 1;
      else if(text.compare("off")==0)
         res = 0;
      break;
   case SEQ_AUT_NOTE_LENGTH:
   case SEQ_AUT_POS_VARIANCE:
   case SEQ_AUT_VELO_VARIANCE:
   case SEQ_AUT_LENGTH_VARIANCE:
   case SEQ_AUT_GLOBAL_SWING:
      res = text.trimCharactersAtEnd("% ").getIntValue();
      break;
   case SEQ_AUT_MUTED:
      if (text.compare("yes") == 0)
         res = 1;
      else if (text.compare("no") == 0)
         res = 0;
      break;
   case SEQ_AUT_CURRENT_PATTERN: // entered 1-based
      res = text.getIntValue() - 1;
      break;
   default:
      res = text.getIntValue();
      break;
   }
   return res;
}

float SeqPlaybackParameter::getValue() const
{
   return mValue == 0 ? 0.0f : mValue == 1 ? 0.5f : 1.0f;
}

void SeqPlaybackParameter::setValue(float newValue)
{
   mValue = roundToInt(jmap<float>(jlimit(0.0f, 1.0f, newValue), 0.0f, 2.0f));
}

String SeqPlaybackParameter::getText(float value, int) const
{
   const int v = roundToInt(jmap<float>(jlimit(0.0f, 1.0f, value), 0.0f, 2.0f));
   return v == 0 ? "manual" : v == 1 ? "stop" : "play";
}

float SeqPlaybackParameter::getValueForText(const String & text) const
{
   if (text.compare("play") == 0)
      return 1.0f;
   if (text.compare("stop") == 0)
      return 0.5f;
   return 0.0f;
}
