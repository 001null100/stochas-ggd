#include "PluginProcessor.h"

#include <cmath>

// Keep the inherited processor implementation intact in this translation unit,
// but compile its processBlock under a private fallback name. The GGD build then
// supplies the transport-aware processBlock below.
#define processBlock processLegacyBlock
#include "PluginProcessor.cpp"
#undef processBlock

namespace
{
void normaliseHostBarOrigin(AudioPlayHead::CurrentPositionInfo& posinfo)
{
   // JUCE's ppqPosition is an absolute quarter-note coordinate, but hosts may
   // place musical bar 1 at a non-zero PPQ origin. Bitwig can expose this most
   // clearly in odd meters / preroll, where a 5/4 bar may begin at -1, then 4,
   // 9, ... . The event scheduler intentionally loops in pattern ticks, so feed
   // it a canonical PPQ coordinate whose bar starts are 0, 5, 10, ... instead.
   if (posinfo.timeSigNumerator <= 0 || posinfo.timeSigDenominator <= 0)
      return;

   const double beatsPerBar = static_cast<double>(posinfo.timeSigNumerator) * 4.0
                            / static_cast<double>(posinfo.timeSigDenominator);
   const double barStart = posinfo.ppqPositionOfLastBarStart;
   const double ppq = posinfo.ppqPosition;

   if (!std::isfinite(beatsPerBar) || beatsPerBar <= 0.0
       || !std::isfinite(barStart) || !std::isfinite(ppq))
      return;

   // Reject obviously stale/default host metadata. A valid last-bar-start must
   // be close to the current PPQ and no farther than roughly one bar behind it.
   const double positionInBar = ppq - barStart;
   const double metadataTolerance = std::max(0.25, beatsPerBar * 0.05);
   if (positionInBar < -metadataTolerance
       || positionInBar > beatsPerBar + metadataTolerance)
      return;

   // std::remainder gives the smallest signed offset from the host bar start to
   // an integer multiple of the current bar length. For normal 4/4 projects the
   // result is zero, making this path a no-op.
   const double barOriginOffset = std::remainder(barStart, beatsPerBar);
   if (!std::isfinite(barOriginOffset)
       || std::abs(barOriginOffset) < 1.0e-9)
      return;

   posinfo.ppqPosition -= barOriginOffset;
   posinfo.ppqPositionOfLastBarStart -= barOriginOffset;
}
}

void SeqAudioProcessor::processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
   MidiBuffer processedMidi; // buffer that holds output midi (initially empty)
   MidiBuffer recordedNotes; // buffer that holds recorded notes (if recording)
   AudioPlayHead *ph = getPlayHead();
   AudioPlayHead::CurrentPositionInfo posinfo;
   bool stoppingPlayback = false;
   bool startingPlayback = false;
   bool areWePlaying = false;
   int samplesperblock = 0; // number of samples per block
   double samplerate = getSampleRate();
   double beatsperbar;
   int i = 0;
   // this is used to determine whether to signal a play position change.
   int oldpos[SEQ_MAX_LAYERS];
   SequenceData *seq = mData.getAudSeqData();

   // clear all buffers. We don't produce audio nor do we filter incoming audio.
   buffer.clear();

   // retrieve some important info (ppqposition might be negative <ahem>cubase)
   if(wrapperType == wrapperType_Standalone) {
      // in standalone mode, fake it out
      positionInfoStandalone(&posinfo);
   } else {
      ph->getCurrentPosition(posinfo);

      // GGD 1.0.2: anchor pattern phase to the host's real musical bar origin
      // before applying the inherited Bitwig negative-PPQ preroll guard. This
      // prevents a shifted odd-meter origin from making beat 2 look like tick 0.
      normaliseHostBarOrigin(posinfo);
   }

   // BITWIG FIX
   // In Bitwig it's possible for the ppqPosition to be negative if pre-roll is
   // turned on. After bar-origin normalisation, genuinely negative preroll is
   // still ignored, while the real first musical beat is no longer discarded.
   if(posinfo.ppqPosition < 0 ) {
      // samples per beat
      double s = (60.0 * getSampleRate()) / (posinfo.bpm);
      // ppq pos is in beats
      if ((posinfo.ppqPosition*s)+ buffer.getNumSamples() >=0)
         posinfo.ppqPosition = 0;
      else
         return;
   }
   // END BITWIG FIX

   // position adjustment (which is a problem with protools and nothing else)
   // getPPQOffset should be an atomic operation.
   mPPQAdjust = (float)mEditorState->getPPQOffset() / 1000.0f;
   if (mPPQAdjust)
      posinfo.ppqPosition += mPPQAdjust;

   beatsperbar = (double)posinfo.timeSigNumerator*4.0 / (double)posinfo.timeSigDenominator;

   // samplesblock is how many samples will come before the next processBlock call
   samplesperblock = buffer.getNumSamples();

   // keep track of current positions so we can determine whether they are changing later
   for (i = 0; i<SEQ_MAX_LAYERS; i++)
      oldpos[i] = mStocha[i].getCurrentStepPosition();

#ifdef CUBASE_HACKS
   if (!mHostType.isCubase()) // just to be safe. Want to limit the scope of this
      mCubaseAtRestPos = 0.0;
#endif

   // check incoming midi to see if manual play start/stop, or a record start/stop is received.
   // needs to be done before determining playback below, so that we can capture
   // a message that might tell us to start playback. Also, for recording, since the engines
   // don't handle record start/stop, we need to check for those messages as well
   checkIncomingMidiForStartStop(midiMessages);

   // determine whether we are currently playing
   if (seq->getAutoPlayMode() == SEQ_PLAYMODE_AUTO) // if autoplay is on
      // determined by daw alone
      areWePlaying = posinfo.isPlaying;
   else {
      // determined by manual playback, quantize, etc.
      // this will also transition state between standby/play/stop
      // and also send notification and setup mMPBStartPosition
      areWePlaying = determinePlaybackState(seq->getAutoPlayMode(), posinfo.isPlaying,
         beatsperbar, samplerate, posinfo.bpm, posinfo.ppqPosition, samplesperblock);
   }

   // determine whether we are transitioning from playing to not playing
   // or vice versa
   if (!areWePlaying) { // if we are stopped playback

#ifdef CUBASE_HACKS
         if (mHostType.isCubase())
            mCubaseAtRestPos = posinfo.ppqPosition;
#endif
         if (mPlaying) {        // but was playing before this, then
            mPlaying = false;   // we are stopping
            stoppingPlayback = true;

         }
      }
   else { // if we are actually playing


      if (!mPlaying) { // and was not playing before this
         mPlaying = true;
         startingPlayback = true;
         // give this an initial value
         mLastPosition = posinfo.ppqPosition;

         // set recording state if applicable
         if (mRecordingMode) {
            // notify the ui that we are now recording (since we were in standby before)
            mNotifier.setRecordingState(SeqProcessorNotifier::on);
         }
      }
#ifdef CUBASE_HACKS
      else
      {
         if (mCubaseAtRestPos && posinfo.ppqPosition >= mCubaseAtRestPos)
            mCubaseAtRestPos = 0.0; // once we've past that play position, turn it off
                                    // so that we don't run into issues if looping back
      }
#endif

   } // if posinfo.isPlaying

   // determine whether we need to generate a new random number.
   // if we are first starting to play or if we have looped back,
   // we will need a new one
   if (startingPlayback || (mPlaying && posinfo.ppqPosition < mLastPosition)) {
      int64 seed = seq->getRandomSeed();
      if (seed == 0) {
         // we need to generate a new random seed
         seed = generateNewRootSeed();
         // we also need to make the ui aware of the seed in case the
         // user clicks "stable" so that it pulls that new seed to save with
         // the patch
         mNotifier.setRandomSeed(seed);
      }

      // we use the seed on each of the engines
      // note that these are not seeds, but are
      // some random prime numbers to use as sequence id's.
      // the same seed is used for each engine, but each has it's
      // own sequence id
      for (i = 0; i < SEQ_MAX_LAYERS; i++) {
         static const uint64 seqid[SEQ_MAX_LAYERS] = {
            999999587
#if(SEQ_MAX_LAYERS > 1)
            ,
            2000037797,
            300045709,
            40044757
#if(SEQ_MAX_LAYERS>4)
#error "Need to add some prime numbers to the array now"
#endif
#endif
         };
         // now set the seed for each engine
         mStocha[i].setRandomSeed((uint64)seed, seqid[i]);
      }
   }

   // set this so that if it's checked again we can know whether we've
   // moved back in time and need to generate again
   if(mPlaying)
      mLastPosition = posinfo.ppqPosition;

   // any incoming midi data that needs handling?
   // handle mapped midi, learn midi, midi light
   // pass the data on to processedMidi if we are passing through data
   // pass data to recordedNotes if recording is active
   handleIncomingMidi(mPlaying, startingPlayback, midiMessages, processedMidi,recordedNotes);
   if (stoppingPlayback) {
      // quiesce all playing notes if we are stopping
      for (i = 0; i < SEQ_MAX_LAYERS; i++)
         mStocha[i].playbackStopped();

      // also reset automation values
      for (i = 0; i < mAutomationParameters.size(); i++)
         mAutomationParameters[i]->reset();

      // also turn off recording mode
      mRecordingMode = false;
      mNotifier.setRecordingState(SeqProcessorNotifier::off);
   }

   // main processing is right here. figure out what notes to play
   // now and in the near future
   if(mPlaying) {
      // this is the main processing to determine what notes to play
      for (i = 0; i < SEQ_MAX_LAYERS; i++) {

         // if we are just starting playback and we are in manual playback mode
         // (ie play starts at 0 position wherever the user hit play btn)
         if (startingPlayback && mMPBstate==MPBstarted) {
            mStocha[i].setPlaybackStartPosition(mMPBStartPosition);
         }
         // todo get retval and blink some overflow light if false
         mStocha[i].processBlock(posinfo.ppqPosition, //which quarter measure we are on
            samplerate,
            samplesperblock,
            posinfo.bpm,
            beatsperbar
#ifdef CUBASE_HACKS
            ,mCubaseAtRestPos
#endif
         );

      }

      // now that we have positional info calculated from StochaEngine, we can
      // deal with recorded midi notes (if any. there will only be some if we are
      // recording and we've received note on/off data)
      if(!recordedNotes.isEmpty())
         dispatchRecordedMidiNotes(recordedNotes);

   }

   // now see whether the engine has any midi notes that need playing
   // right now. only while playing or quiescing
   if (mPlaying || stoppingPlayback) {
      int8_t midi_note = 0, midi_velo = 0, midi_chan = 0;
      int midi_pos = 0;
      for (i = 0; i < SEQ_MAX_LAYERS; i++) {
         while (mStocha[i].getMidiEvent(samplesperblock, &midi_pos, &midi_note, &midi_velo, &midi_chan)) {
            MidiMessage m;
            if (midi_velo) {
               m = MidiMessage::noteOn(midi_chan, (int)midi_note, (uint8)midi_velo);
            }
            else { // note off
               m = MidiMessage::noteOff(midi_chan, (int)midi_note);
            }
            processedMidi.addEvent(m, midi_pos);
         } // get upcoming midi events and add them
      }
   }

   // tell the engine to advance midi in the queue for stuff that does not need to play
   // right away
   if (mPlaying) {
      for (i = 0; i < SEQ_MAX_LAYERS; i++)
         mStocha[i].doneBlock(samplesperblock);
   }

   // see if the UI has any data for us that we need to process
   checkforUIIncomingData(processedMidi);

   // flip from incoming midi data to outgoing data
   midiMessages.swapWith(processedMidi);

   // notify the UI of updates that need to be reflected
   for (i = 0; i < SEQ_MAX_LAYERS; i++) {
      int newpos;
      // currently playing step
      newpos = mStocha[i].getCurrentStepPosition();
      if (oldpos[i] != newpos)
         mNotifier.setPlayPosition(i,newpos);

      // currently playing pattern or -1
      mNotifier.setCurrentPattern(i, mPlaying ? mStocha[i].getPlayingPattern() : -1);

      // whether layer is muted or not
      mNotifier.setMuteState(i, mStocha[i].getMuteState());
   }
}
