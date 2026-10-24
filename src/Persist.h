/***************************************************************
 ** Copyright (C) 2016 by Andrew Shakinovsky
 **
 ** You may also use this code under the terms of the
 ** GPL v3 (see www.gnu.org/licenses).
 ** STOCHAS IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
 ** WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING
 ** MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE DISCLAIMED.
 ***************************************************************/
#ifndef PERSIST_H_
#define PERSIST_H_
#include "SequenceData.h"

#include <vector>

/**
Project state <-> XML.

The document keeps the inherited Stochas layout (root tag "stochas") so state
written by any Stochas GGD build loads in any other, plus a
"ggd-event-state" child holding the GGD event patterns (GgdEventPersist).

Early alpha builds stored hits as Stochas grid cells. Those cells are read
into a temporary list and converted to event patterns during retrieve().
*/
class SeqPersist {
public:
   SeqPersist() : mRoot("stochas") {}
   const XmlElement &store(const SequenceData *sourceData);
   bool retrieve(SequenceData *targetData, const XmlElement *sourceData);

   struct LegacyCell {
      int row = 0;
      int step = 0;
      int prob = SEQ_PROB_OFF;
      int velo = 0;
      int length = 0;
      int offset = 0;
   };

   // Converts inherited 1/16 grid cells into events on an activated pattern.
   static void migrateLegacyCells(const std::vector<LegacyCell> &cells,
                                  GgdEventPattern &destination,
                                  int numerator, int denominator, int bars);

   // Meter encoded in alpha "GGD3:<kit>:<num>/<den>" layer tags.
   static bool parseLegacyMeter(const String &layerName, int &numerator, int &denominator);
   static bool isGgdLayerTag(const String &layerName);

private:
   XmlElement mRoot;
   std::vector<LegacyCell> mLegacyCells[SEQ_MAX_LAYERS][SEQ_MAX_PATTERNS];

   XmlElement *addKeyVal(const char *name, const String &value);
   XmlElement *addKeyVal(const char *name, int64 value);
   XmlElement *addKeyVal(const char *name, double value);
   bool getKeyVal(XmlElement *e, int64 *val);
   bool getKeyVal(XmlElement *e, double *val);
   bool getKeyVal(XmlElement *e, String *val);
   void retrieveLayer(XmlElement *e, int layerIndex, SequenceLayer *lay);
   void retrievePattern(XmlElement *e, int layerIndex, SequenceLayer *lay);
   void storeMidiMap(int idx, SeqMidiMapItem *item, XmlElement *parent);
   void storeLayer(int idx, const SequenceLayer *item, XmlElement *parent);
   void storeNote(int idx, const SequenceLayer *item, XmlElement *parent);
   void storePattern(int idx, const SequenceLayer *item, XmlElement *parent);
   void migrateLegacyLayers(SequenceData *targetData);
};
#endif
