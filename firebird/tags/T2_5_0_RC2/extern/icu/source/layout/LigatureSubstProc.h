/*
 *
 * (C) Copyright IBM Corp. 1998-2004 - All Rights Reserved
 *
 */

#ifndef __LIGATURESUBSTITUTIONPROCESSOR_H
#define __LIGATURESUBSTITUTIONPROCESSOR_H

/**
 * \file
 * \internal
 */

#include "LETypes.h"
#include "MorphTables.h"
#include "SubtableProcessor.h"
#include "StateTableProcessor.h"
#include "LigatureSubstitution.h"

U_NAMESPACE_BEGIN

class LEGlyphStorage;

#define nComponents 16

class LigatureSubstitutionProcessor : public StateTableProcessor
{
public:
    virtual void beginStateTable();

    virtual ByteOffset processStateEntry(LEGlyphStorage &glyphStorage, le_int32 &currGlyph, EntryTableIndex index);

    virtual void endStateTable();

    LigatureSubstitutionProcessor(const MorphSubtableHeader *morphSubtableHeader);
    virtual ~LigatureSubstitutionProcessor();

    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     *
     * @stable ICU 2.8
     */
    virtual inline UClassID getDynamicClassID() const { return getStaticClassID(); }

    /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     *
     * @stable ICU 2.8
     */
    static inline UClassID getStaticClassID() { return (UClassID)&fgClassID; }

private:
    LigatureSubstitutionProcessor();

protected:
    ByteOffset ligatureActionTableOffset;
    ByteOffset componentTableOffset;
    ByteOffset ligatureTableOffset;

    const LigatureSubstitutionStateEntry *entryTable;

    le_int32 componentStack[nComponents];
    le_int16 m;

    const LigatureSubstitutionHeader *ligatureSubstitutionHeader;

private:

    /**
     * The address of this static class variable serves as this class's ID
     * for ICU "poor man's RTTI".
     */
    static const char fgClassID;
};

U_NAMESPACE_END
#endif
