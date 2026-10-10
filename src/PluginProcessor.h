/***************************************************************
 ** Copyright (C) 2016 by Andrew Shakinovsky
 **
 ** You may also use this code under the terms of the
 ** GPL v3 (see www.gnu.org/licenses).
 ** STOCHAS IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
 ** WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING
 ** MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE DISCLAIMED.
 ***************************************************************/

#ifndef PLUGINPROCESSOR_H_INCLUDED
#define PLUGINPROCESSOR_H_INCLUDED

#include "SequenceData.h"
#include "GgdEngine.h"
#include "GgdProject.h"

#include <atomic>

class SeqAudioProcessor;

/* Lets SeqAudioProcessorParameter notify the processor about host changes. */
class AutParamNotify {
public:
   virtual ~AutParamNotify() = default;
   // paramValue is SEQ_AUT_DEFAULT_VALUE_DESIG when the parameter is reset
   virtual void automationParameterHasChanged(int paramId, int paramValue, int layer) = 0;
   virtual String getTextForAutomationParameterValue(int paramId, int paramValue) = 0;
   virtual int parseTextForAutomationParameterValue(int paramId, String text) = 0;
};

/** Host-automatable override.

A host value of 0.0 means "as assigned" (no override). Values above it map
linearly onto [rangeLo, rangeHi]. Internally mValue == rangeLo - 1 represents
"as assigned", and the processor receives SEQ_AUT_DEFAULT_VALUE_DESIG for it.

The parameter list and order are inherited from Stochas and must not change:
hosts identify automation lanes by parameter index.
*/
class SeqAudioProcessorParameter : public AudioProcessorParameter {
   float getValue() const override;
   void setValue(float newValue) override;
   float getDefaultValue() const override;
   String getName(int maximumStringLength) const override;
   String getLabel() const override;
   String getText(float value, int) const override;
   float getValueForText(const String & text) const override;
   bool isAutomatable() const override { return true; }

   AutParamNotify *mNotify;
   int mLayerNumber; // layer specific parameters only
   int mParamId;
   int mRangeLo;
   int mRangeHi;
   std::atomic<int> mValue; // mRangeLo - 1 means "as assigned"
   String mName;

public:
   SeqAudioProcessorParameter(AutParamNotify *notify, int paramId, int rangeLo, int rangeHi, int layerNum, const String &name);

   bool isAssigned() const { return mValue.load() == mRangeLo - 1; }

   // reset to "as assigned" (e.g. when playback stops)
   void reset();
};

// Inherited manual playback parameter (manual / stop / play). Stochas GGD
// always follows the host transport, so it is kept only for automation
// compatibility.
class SeqPlaybackParameter : public AudioProcessorParameter {
   int mValue; // 0,1 or 2
   int getNumSteps() const override { return 3; }
   bool isDiscrete() const override { return true; }
   float getValue() const override;
   void setValue(float newValue) override;
   String getText(float value, int) const override;
   float getValueForText(const String & text) const override;
   float getDefaultValue() const override { return 0.0f; }
   String getName(int) const override { return "playback";}
   String getLabel() const override {return String(); }
   bool isAutomatable() const override { return true; }
public:
   SeqPlaybackParameter() : AudioProcessorParameter(1), mValue(0) {}
};

class SeqAudioProcessor  : public AudioProcessor, public AutParamNotify
{
public:
   SeqAudioProcessor();
   ~SeqAudioProcessor() override;

   // Project state. The editor edits getUISeqData() and publishes with
   // publishEdit(); the audio thread reads the published copy.
   SeqDataBuffer mData;

   // UI -> audio messages (audition notes).
   SeqFifo mIncomingData;

   // Audio -> UI status.
   SeqProcessorNotifier mNotifier;

   const GgdProject &getProject() const { return mProject; }

   // Publishes UI edits to the audio thread and tells the host the project
   // changed. Message thread only.
   void publishEdit(bool markProjectDirty = true);

   // Feedback for the editor. Safe to call from the message thread.
   bool popPlayedEvent(GgdEngine::PlayedEvent &event) { return mEngines[GgdProject::drumLayer].popPlayedEvent(event); }
   bool readPlayhead(GgdPlayheadSnapshot &snapshot) const { return mEngines[GgdProject::drumLayer].readPlayhead(snapshot); }

   // Inherited via AutParamNotify
   void automationParameterHasChanged(int paramId, int paramValue, int layer) override;
   String getTextForAutomationParameterValue(int paramId, int paramValue) override;
   int parseTextForAutomationParameterValue(int paramId, String text) override;

   // AudioProcessor
   void prepareToPlay(double sampleRate, int samplesPerBlock) override;
   void releaseResources() override;
   bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
   void processBlock(AudioBuffer<float>&, MidiBuffer&) override;
   using AudioProcessor::processBlock;
   AudioProcessorEditor* createEditor() override;
   bool hasEditor() const override;
   const String getName() const override;
   bool acceptsMidi() const override;
   bool producesMidi() const override;
   bool isMidiEffect() const override;
   double getTailLengthSeconds() const override;
   int getNumPrograms() override;
   int getCurrentProgram() override;
   void setCurrentProgram(int index) override;
   const String getProgramName(int index) override;
   void changeProgramName(int index, const String& newName) override;
   void getStateInformation(MemoryBlock& destData) override;
   void setStateInformation(const void* data, int sizeInBytes) override;

private:
   GgdProject mProject;
   GgdEngine mEngines[SEQ_MAX_LAYERS];
   juce::Array<SeqAudioProcessorParameter*> mAutomationParameters;

   bool mPlaying = false;
   double mLastPpq = 0.0;
   std::uint64_t mRandomSeed = 0;
   std::uint64_t mSeedCounter = 0;
   double mStandaloneStartMs = 0.0;

   void addAutParam(SeqAudioProcessorParameter *p);
   bool readTransport(GgdTimeline::HostPosition &host, bool &playing, int numSamples);
   void handleUIMessages(const SequenceData &data, MidiBuffer &output);
   std::uint64_t nextRandomSeed();

   JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SeqAudioProcessor)
};

#endif  // PLUGINPROCESSOR_H_INCLUDED
