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

class SeqPersist {
   XmlElement mRoot;
   XmlElement *addKeyVal(const char *name, const String &value);
   XmlElement *addKeyVal(const char *name, int64 value);
   XmlElement *addKeyVal(const char *name, double value);
   bool getKeyVal(XmlElement *e, int64 *val);
   bool getKeyVal(XmlElement *e, double *val);
   bool getKeyVal(XmlElement *e, String *val);
   void retrieveLayer(XmlElement *e, SequenceLayer *lay);
   void retrievePattern(XmlElement *e, SequenceLayer *lay);
   void storeMidiMap(int idx, SeqMidiMapItem *item, XmlElement *parent);
   void storeLayer(int idx, SequenceLayer *item, XmlElement *parent);
   void storeNote(int idx, SequenceLayer *item, XmlElement *parent);
   void storePattern(int idx, SequenceLayer *item, XmlElement *parent);
   bool storeRow(int idx, int pat, SequenceLayer *item, XmlElement *parent);
   bool storeCell(int idx, int pat, int row, SequenceLayer *item, XmlElement *parent);

   const XmlElement &storeLegacy(SequenceData *sourceData);
   bool retrieveLegacy(SequenceData *targetData,const XmlElement *sourceData);
public:
   SeqPersist() : mRoot("stochas") {}
   const XmlElement &store(SequenceData *sourceData);
   bool retrieve(SequenceData *targetData,const XmlElement *sourceData);
};
#endif
