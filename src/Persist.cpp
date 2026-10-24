/***************************************************************
 ** Copyright (C) 2016 by Andrew Shakinovsky
 **
 ** You may also use this code under the terms of the
 ** GPL v3 (see www.gnu.org/licenses).
 ** STOCHAS IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
 ** WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING
 ** MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE DISCLAIMED.
 ***************************************************************/

#include "Persist.h"
#include "Constants.h"
#include "GgdEventPersist.h"

#include <cmath>
#include <memory>

XmlElement * SeqPersist::addKeyVal(const char *name, const String & value)
{
   XmlElement *ret = new XmlElement(name);
   ret->setAttribute("val", value);
   return ret;
}

XmlElement * SeqPersist::addKeyVal(const char * name, int64 value)
{
   String tmp;
   tmp << value;
   return addKeyVal(name,tmp);
}

XmlElement * SeqPersist::addKeyVal(const char * name, double value)
{
   String tmp;
   tmp << value;
   return addKeyVal(name,tmp);
}

bool SeqPersist::getKeyVal(XmlElement * e, int64 * val)
{
   String s;
   if (getKeyVal(e, &s)) {
      *val = s.getLargeIntValue();
      return true;
   }
   return false;
}

bool SeqPersist::getKeyVal(XmlElement * e, double * val)
{
   String s;
   if (getKeyVal(e, &s)) {
      *val = s.getDoubleValue();
      return true;
   }
   return false;
}

bool SeqPersist::getKeyVal(XmlElement * e, String * val)
{
   *val = e->getStringAttribute("val",String());
   return val->length() > 0;
}

void SeqPersist::storeMidiMap(int idx, SeqMidiMapItem * item, XmlElement * parent)
{
   if (item->mAction != SEQMIDI_ACTION_INVALID) {
      XmlElement *mm = new XmlElement("mm");
      mm->setAttribute("idx", idx);
      mm->setAttribute("act", item->mAction);
      mm->setAttribute("chn", item->mChannel);
      mm->setAttribute("not", item->mNote);
      mm->setAttribute("tgt", item->mTarget);
      mm->setAttribute("typ", item->mType);
      mm->setAttribute("val", item->mValue);
      parent->prependChildElement(mm);
   }
}

void SeqPersist::storeLayer(int idx, const SequenceLayer * item, XmlElement * parent)
{
   const char *scale;
   const char *key;
   int oct;
   XmlElement *lyr = new XmlElement("l");
   XmlElement *k;
   parent->prependChildElement(lyr);

   lyr->setAttribute("idx", idx);
   lyr->setAttribute("name", String::fromUTF8(item->getLayerName()));
   lyr->prependChildElement(addKeyVal("combine", item->isCombineMode() ? 1ll:0ll));
   lyr->prependChildElement(addKeyVal("humlen", (int64)item->getHumanLength()));
   lyr->prependChildElement(addKeyVal("humvel", (int64)item->getHumanVelocity()));
   lyr->prependChildElement(addKeyVal("humpos", (int64)item->getHumanPosition()));
   lyr->prependChildElement(addKeyVal("mute", item->getMuted() ? 1ll : 0ll));
   lyr->prependChildElement(addKeyVal("stppm", (int64)item->getStepsPerMeasure()));
   lyr->prependChildElement(addKeyVal("dcycle", (int64)item->getDutyCycle()));
   lyr->prependChildElement(addKeyVal("mchan", (int64)item->getMidiChannel()));
   lyr->prependChildElement(addKeyVal("clkdiv", (int64)item->getClockDivider()));
   lyr->prependChildElement(addKeyVal("notecust", item->noteSourceIsCustom() ? 1ll : 0ll));
   lyr->prependChildElement(addKeyVal("bias", (int64)item->getPolyBias()));
   lyr->prependChildElement(addKeyVal("maxpoly", (int64)item->getMaxPoly()));
   lyr->prependChildElement(addKeyVal("mono", item->isMonoMode() ? 1ll : 0ll));
   lyr->prependChildElement(addKeyVal("numsteps", (int64)item->getNumSteps()));
   lyr->prependChildElement(addKeyVal("numrows", (int64)item->getMaxRows()));
   lyr->prependChildElement(addKeyVal("curpat",(int64)item->getCurrentPattern()));

   item->getKeyScaleOct(&scale, &key, &oct);
   k = new XmlElement("sko");
   lyr->prependChildElement(k);
   k->setAttribute("stdscale", scale);
   k->setAttribute("stdkey", key);
   k->setAttribute("stdoct", oct);

   XmlElement *elem = new XmlElement("notes");
   lyr->prependChildElement(elem);
   for (int i = SEQ_MAX_ROWS-1; i >=0 ; i--)
      storeNote(i, item, elem);

   elem = new XmlElement("pats");
   lyr->prependChildElement(elem);
   for (int i = SEQ_MAX_PATTERNS-1; i >=0 ; i--)
      storePattern(i, item, elem);
}

void SeqPersist::retrieveLayer(XmlElement * e, int layerIndex, SequenceLayer * lay)
{
   int64 ival;
   String sval = e->getStringAttribute("name");
   lay->setLayerName(sval.toRawUTF8());
   for (auto* ch : e->getChildIterator()) {
      if (ch->hasTagName("humlen")) {
         if (getKeyVal(ch, &ival))
            lay->setHumanLength((int)ival);
      }
      else if (ch->hasTagName("humvel")) {
         if (getKeyVal(ch, &ival))
            lay->setHumanVelocity((int)ival);
      }
      else if (ch->hasTagName("humpos")) {
         if (getKeyVal(ch, &ival))
            lay->setHumanPosition((int)ival);
      }
      else if (ch->hasTagName("combine")) {
         if (getKeyVal(ch, &ival))
            lay->setCombineMode(0 != ival);
      }
      else if (ch->hasTagName("mute")) {
         if (getKeyVal(ch, &ival))
            lay->setMuted(0 != ival);
      }
      else if (ch->hasTagName("stppm")) {
         if (getKeyVal(ch, &ival))
            lay->setStepsPerMeasure((int)ival);
      }
      else if (ch->hasTagName("dcycle")) {
         if (getKeyVal(ch, &ival))
            lay->setDutyCycle((int)ival);
      }
      else if (ch->hasTagName("mchan")) {
         if (getKeyVal(ch, &ival))
            lay->setMidiChannel((int8_t)ival);
      }
      else if (ch->hasTagName("clkdiv")) {
         if (getKeyVal(ch, &ival))
            lay->setClockDivider((int)ival);
      }
      else if (ch->hasTagName("notecust")) {
         if (getKeyVal(ch, &ival))
            lay->setNoteSource(ival!=0);
      }
      else if (ch->hasTagName("bias")) {
         if (getKeyVal(ch, &ival))
            lay->setPolyBias((int)ival);
      }
      else if (ch->hasTagName("maxpoly")) {
         if (getKeyVal(ch, &ival))
            lay->setMaxPoly((int)ival);
      }
      else if (ch->hasTagName("mono")) {
         if (getKeyVal(ch, &ival))
            lay->setMonoMode(ival !=0);
      }
      else if (ch->hasTagName("numsteps")) {
         if (getKeyVal(ch, &ival))
            lay->setNumSteps((int)ival);
      }
      else if (ch->hasTagName("numrows")) {
         if (getKeyVal(ch, &ival))
            lay->setMaxRows((int)ival);
      }
      else if (ch->hasTagName("notes")) {
         for (auto* note : ch->getChildWithTagNameIterator("n")) {
            const int idx = note->getIntAttribute("idx");
            if (idx >= 0 && idx < SEQ_MAX_ROWS) {
               lay->setNote(idx, (int8_t)note->getIntAttribute("std"), false);
               lay->setNote(idx, (int8_t)note->getIntAttribute("cust"), true);
               const auto name = note->getStringAttribute("name");
               if (name.isNotEmpty())
                  lay->setNoteName(idx, name.toRawUTF8());
            }
         }
      }
      else if (ch->hasTagName("pats")) {
         for (auto* pat : ch->getChildWithTagNameIterator("p"))
            retrievePattern(pat, layerIndex, lay);
      }
      else if (ch->hasTagName("sko")) {
         String scale = ch->getStringAttribute("stdscale");
         String key = ch->getStringAttribute("stdkey");
         int oct = ch->getIntAttribute("stdoct");
         lay->setKeyScaleOct(scale.toRawUTF8(), key.toRawUTF8(), oct);
      }
      else if (ch->hasTagName("curpat")) {
        if (getKeyVal(ch, &ival))
          lay->setCurrentPattern((int)ival);
      }
   }
}

void SeqPersist::storeNote(int idx, const SequenceLayer * item, XmlElement * parent)
{
   // Both note sets are stored; only the custom set carries labels.
   XmlElement *n = new XmlElement("n");
   parent->prependChildElement(n);
   n->setAttribute("idx", idx);
   n->setAttribute("std", item->getNote(idx, false));
   n->setAttribute("cust", item->getNote(idx, true));
   const String label(CharPointer_UTF8(item->getNoteName(idx)), (size_t) SEQ_MAX_NOTELABEL_LEN);
   if (label.isNotEmpty())
      n->setAttribute("name", label);
}

void SeqPersist::retrievePattern(XmlElement * e, int layerIndex, SequenceLayer * lay)
{
   const int pat = e->getIntAttribute("idx");
   if (pat < 0 || pat >= SEQ_MAX_PATTERNS)
      return;

   const String name = e->getStringAttribute("name");
   if (name.isNotEmpty())
      lay->setPatternName(name.toRawUTF8(), pat);

   // Inherited grid cells (alpha projects). Chains are ignored: GGD never
   // created them and event patterns have no equivalent.
   auto &cells = mLegacyCells[layerIndex][pat];
   for (auto* rows : e->getChildWithTagNameIterator("rows")) {
      for (auto* row : rows->getChildWithTagNameIterator("r")) {
         const int rowIndex = row->getIntAttribute("idx");
         if (rowIndex < 0 || rowIndex >= SEQ_MAX_ROWS)
            continue;
         for (auto* cellList : row->getChildWithTagNameIterator("cells")) {
            for (auto* cell : cellList->getChildWithTagNameIterator("c")) {
               LegacyCell legacy;
               legacy.row = rowIndex;
               legacy.step = cell->getIntAttribute("idx", -1);
               legacy.prob = cell->getIntAttribute("prob", SEQ_PROB_OFF);
               legacy.velo = cell->getIntAttribute("velo");
               legacy.length = cell->getIntAttribute("len");
               legacy.offset = cell->getIntAttribute("offs");
               if (legacy.step >= 0 && legacy.step < SEQ_MAX_STEPS && legacy.prob != SEQ_PROB_OFF)
                  cells.push_back(legacy);
            }
         }
      }
   }
}

void SeqPersist::storePattern(int idx, const SequenceLayer * item, XmlElement * parent)
{
   // Hits live in the ggd-event-state element; only an edited name is kept
   // here so older builds still show it.
   const char *pname = item->getPatternName(idx);
   if (strcmp(pname, SEQ_DEFAULT_PAT_NAME) == 0)
      return;

   XmlElement *p = new XmlElement("p");
   p->setAttribute("name", String::fromUTF8(pname));
   p->setAttribute("idx", idx);
   p->prependChildElement(new XmlElement("rows"));
   parent->prependChildElement(p);
}

bool SeqPersist::isGgdLayerTag(const String &layerName)
{
   return layerName.startsWith("GGD:") || layerName.startsWith("GGD2:")
       || layerName.startsWith("GGD3:") || layerName.startsWith("GGD4:");
}

bool SeqPersist::parseLegacyMeter(const String &layerName, int &numerator, int &denominator)
{
   if (!layerName.startsWith("GGD3:"))
      return false;
   auto remainder = layerName.fromFirstOccurrenceOf(":", false, false);
   remainder = remainder.fromFirstOccurrenceOf(":", false, false);
   const int n = remainder.upToFirstOccurrenceOf("/", false, false).getIntValue();
   const int d = remainder.fromFirstOccurrenceOf("/", false, false).getIntValue();
   if (n < 1 || n > 32 || (d != 4 && d != 8 && d != 16))
      return false;
   numerator = n;
   denominator = d;
   return true;
}

void SeqPersist::migrateLegacyCells(const std::vector<LegacyCell> &cells,
                                    GgdEventPattern &destination,
                                    int numerator, int denominator, int bars)
{
   destination.clear(false);
   destination.activate(numerator, denominator, bars);
   const int patternLength = destination.getLengthTicks();

   for (const auto &cell : cells) {
      const int baseTick = cell.step * GGD_TICKS_PER_16TH;
      if (baseTick >= patternLength)
         continue;

      const int velocity = jlimit(1, 127, cell.velo);
      const int probability = jlimit(0, 100, cell.prob);

      if (cell.length < 0) {
         // Stochas retrigger: the step is split into evenly spaced hits.
         const int triggers = jmax(2, -cell.length + 1);
         for (int trigger = 0; trigger < triggers; ++trigger) {
            const int tick = baseTick + (int)std::llround(
               (double)GGD_TICKS_PER_16TH * trigger / (double)triggers);
            if (tick >= patternLength)
               break;
            destination.addEvent(cell.row, tick, velocity, probability,
                                 jmax(1, GGD_TICKS_PER_16TH / jmax(1, triggers * 2)));
         }
         continue;
      }

      const int shifted = baseTick + (int)std::llround(
         (double)cell.offset * GGD_TICKS_PER_16TH / 100.0);
      destination.addEvent(cell.row,
                           jlimit(0, patternLength - 1, shifted),
                           velocity, probability,
                           jmax(1, (cell.length + 1) * GGD_TICKS_PER_16TH));
   }
}

void SeqPersist::migrateLegacyLayers(SequenceData *targetData)
{
   for (int layerIndex = 0; layerIndex < SEQ_MAX_LAYERS; ++layerIndex) {
      auto *layer = targetData->getLayer(layerIndex);
      const auto tag = String::fromUTF8(layer->getLayerName());
      if (!isGgdLayerTag(tag))
         continue;

      int numerator = 4;
      int denominator = 4;
      parseLegacyMeter(tag, numerator, denominator);
      const int stepsPerBar = jmax(1, numerator * 16 / denominator);
      const int bars = jlimit(1, 64, (layer->getNumSteps() + stepsPerBar - 1) / stepsPerBar);

      for (int pat = 0; pat < SEQ_MAX_PATTERNS; ++pat) {
         const auto &cells = mLegacyCells[layerIndex][pat];
         if (!cells.empty())
            migrateLegacyCells(cells, *layer->getEventPattern(pat), numerator, denominator, bars);
      }
   }
}

const XmlElement & SeqPersist::store(const SequenceData * sourceData)
{
   int i,len;
   XmlElement *elem;
   auto *data = const_cast<SequenceData *>(sourceData);
   mRoot.deleteAllChildElements();
   mRoot.setAttribute("version", SEQ_SERIALIZE_CURRENT_VERSION);

   mRoot.prependChildElement(addKeyVal("bpm", sourceData->getStandaloneBPM()));
   mRoot.prependChildElement(addKeyVal("autoplay", (int64)sourceData->getAutoPlayMode()));
   mRoot.prependChildElement(addKeyVal("offtime", (int64)sourceData->getOffsetTime()));
   mRoot.prependChildElement(addKeyVal("swing", (int64)sourceData->getSwing()));
   mRoot.prependChildElement(addKeyVal("seed", sourceData->getRandomSeed()));
   mRoot.prependChildElement(addKeyVal("midipass", (int64)sourceData->getMidiPassthru()));
   mRoot.prependChildElement(addKeyVal("midiresp", (int64)sourceData->getMidiRespond()));
   elem = new XmlElement("groove");
   mRoot.prependChildElement(elem);
   for (i = SEQ_DEFAULT_NUM_STEPS-1; i >=0 ; i--) {
      const int grv = sourceData->getGroove(i);
      if (grv != 0) {
         XmlElement *g = new XmlElement("i");
         g->setAttribute("idx", i);
         g->setAttribute("val", grv);
         elem->prependChildElement(g);
      }
   }

   elem = new XmlElement("midimap");
   mRoot.prependChildElement(elem);
   len = sourceData->getMappingCount();
   for (i = len - 1; i >= 0; i--)
      storeMidiMap(i, data->getMappingItem(i), elem);

   elem = new XmlElement("layer");
   mRoot.prependChildElement(elem);
   for (i = SEQ_MAX_LAYERS-1; i >=0 ; i--)
      storeLayer(i, sourceData->getLayer(i), elem);

   GgdEventPersist::store(*sourceData, mRoot);
   return mRoot;
}

bool SeqPersist::retrieve(SequenceData * targetData, const XmlElement * sourceData)
{
   int idx;
   int64 val;
   double dval;

   if (sourceData == nullptr || sourceData->getTagName().compare("stochas") != 0)
      return false;

   if (sourceData->getIntAttribute("version",-1) != SEQ_SERIALIZE_CURRENT_VERSION)
      return false;

   for (auto &layerCells : mLegacyCells)
      for (auto &cells : layerCells)
         cells.clear();

   // start fresh (SequenceData is too big for the stack)
   *targetData = *std::make_unique<SequenceData>();

   for(auto *e : sourceData->getChildIterator()) {
      if (e->hasTagName("groove")) {
         for(auto *g : e->getChildIterator()) {
            if (g->hasTagName("i")) {
               idx = g->getIntAttribute("idx");
               val = g->getIntAttribute("val");
               if (idx < SEQ_DEFAULT_NUM_STEPS && idx >= 0)
                  targetData->setGroove(idx, (int)val);
            }
         }
      }
      else if (e->hasTagName("midimap")) {
         int mapcount = 0;
         for(auto *m : e->getChildIterator()) {
            if (m->hasTagName("mm")) {
               idx = m->getIntAttribute("idx");
               if (idx >= 0 && idx < SEQMIDI_MAX_ITEMS) {
                  SeqMidiMapItem *mm = targetData->getMappingItem(idx);
                  mm->mAction = (int8_t)m->getIntAttribute("act");
                  mm->mChannel = (int8_t)m->getIntAttribute("chn");
                  mm->mNote = (int8_t)m->getIntAttribute("not");
                  mm->mTarget = (int8_t)m->getIntAttribute("tgt");
                  mm->mType = (int8_t)m->getIntAttribute("typ");
                  mm->mValue = (int8_t)m->getIntAttribute("val");

                  // Older Stochas builds stored relative transpose as special
                  // values of the absolute transpose action.
                  if(mm->mAction ==SEQMIDI_ACTION_TRANS &&
                     (mm->mValue == SEQMIDI_VALUE_TRANS_NEXT_LEGACY || mm->mValue == SEQMIDI_VALUE_TRANS_PREV_LEGACY)) {
                     mm->mAction = SEQMIDI_ACTION_TRANS_RELATIVE;
                     if (mm->mValue == SEQMIDI_VALUE_TRANS_NEXT_LEGACY)
                        mm->mValue = SEQMIDI_VALUE_TRANS_NEXT_NEW;
                     else
                        mm->mValue = SEQMIDI_VALUE_TRANS_PREV_NEW;
                  }

                  mapcount++;
               }
            }
         }
         targetData->setMappingCount(mapcount);
      }
      else if (e->hasTagName("layer")) {
         for (auto* l : e->getChildWithTagNameIterator("l")) {
            idx=l->getIntAttribute("idx");
            if (idx >= 0 && idx < SEQ_MAX_LAYERS)
               retrieveLayer(l, idx, targetData->getLayer(idx));
         }
      }
      else if (e->hasTagName("midiresp")) {
         if (getKeyVal(e, &val))
            targetData->setMidiRespond((int)val);
      }
      else if (e->hasTagName("midipass")) {
         if (getKeyVal(e, &val))
            targetData->setMidiPassthru((int)val);
      }
      else if (e->hasTagName("seed")) {
         if (getKeyVal(e, &val))
            targetData->setRandomSeed(val);
      }
      else if (e->hasTagName("swing")) {
         if (getKeyVal(e, &val))
            targetData->setSwing((int)val);
      }
      else if (e->hasTagName("curpat")) {
         // legacy: pattern and layer used to be linked
         if (getKeyVal(e, &val)) {
            for(int i=0;i<SEQ_MAX_LAYERS;i++)
               targetData->getLayer(i)->setCurrentPattern((int)val);
         }
      }
      else if (e->hasTagName("offtime")) {
         if (getKeyVal(e, &val))
            targetData->setOffsetTime((int)val);
      }
      else if (e->hasTagName("autoplay")) {
         if (getKeyVal(e, &val))
            targetData->setAutoPlayMode((int)val);
      }
      else if (e->hasTagName("bpm")) {
         if (getKeyVal(e, &dval))
            targetData->setStandaloneBPM(dval);
      }
   }

   migrateLegacyLayers(targetData);

   // Event patterns saved by beta and later builds take precedence over any
   // migrated cells for the same slot.
   GgdEventPersist::retrieve(*targetData, *sourceData);
   return true;
}
