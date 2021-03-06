/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/



#include "oox/dump/xlsbdumper.hxx"

#include <com/sun/star/io/XTextInputStream.hpp>
#include "oox/core/filterbase.hxx"
#include "oox/dump/biffdumper.hxx"
#include "oox/dump/oledumper.hxx"
#include "oox/dump/pptxdumper.hxx"
#include "oox/helper/zipstorage.hxx"
#include "oox/ole/olestorage.hxx"
#include "oox/xls/biffhelper.hxx"
#include "oox/xls/formulabase.hxx"
#include "oox/xls/richstring.hxx"

#if OOX_INCLUDE_DUMPER

namespace oox {
namespace dump {
namespace xlsb {

// ============================================================================

using namespace ::com::sun::star::io;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::util;
using namespace ::oox::xls;

using ::comphelper::MediaDescriptor;
using ::oox::core::FilterBase;
using ::rtl::OUString;
using ::rtl::OUStringBuffer;

// ============================================================================

namespace {

const sal_uInt8 BIFF12_STRINGFLAG_FONTS         = 0x01;
const sal_uInt8 BIFF12_STRINGFLAG_PHONETICS     = 0x02;

const sal_uInt16 BIFF12_OLEOBJECT_LINKED        = 0x0001;

} // namespace

// ============================================================================

RecordObjectBase::RecordObjectBase()
{
}

RecordObjectBase::~RecordObjectBase()
{
}

void RecordObjectBase::construct( const ObjectBase& rParent, const BinaryInputStreamRef& rxStrm, const OUString& rSysFileName )
{
    mxBiffStrm.reset( new SequenceInputStream( getRecordDataSequence() ) );
    SequenceRecordObjectBase::construct( rParent, rxStrm, rSysFileName, mxBiffStrm, "RECORD-NAMES", "SIMPLE-RECORDS" );
    if( SequenceRecordObjectBase::implIsValid() )
        mxErrCodes = cfg().getNameList( "ERRORCODES" );
}

void RecordObjectBase::construct( const RecordObjectBase& rParent )
{
    *this = rParent;
}

bool RecordObjectBase::implReadRecordHeader( BinaryInputStream& rBaseStrm, sal_Int64& ornRecId, sal_Int64& ornRecSize )
{
    sal_Int32 nRecId = 0, nRecSize = 0;
    bool bValid = readCompressedInt( rBaseStrm, nRecId ) && (nRecId >= 0) && readCompressedInt( rBaseStrm, nRecSize ) && (nRecSize >= 0);
    ornRecId = nRecId;
    ornRecSize = nRecSize;
    return bValid;
}

OUString RecordObjectBase::getErrorName( sal_uInt8 nErrCode ) const
{
    return cfg().getName( mxErrCodes, nErrCode );
}

// ------------------------------------------------------------------------

void RecordObjectBase::readAddress( Address& orAddress )
{
    *mxStrm >> orAddress.mnRow >> orAddress.mnCol;
}

void RecordObjectBase::readRange( Range& orRange )
{
    *mxStrm >> orRange.maFirst.mnRow >> orRange.maLast.mnRow >> orRange.maFirst.mnCol >> orRange.maLast.mnCol;
}

void RecordObjectBase::readRangeList( RangeList& orRanges )
{
    sal_Int32 nCount;
    *mxStrm >> nCount;
    if( nCount >= 0 )
    {
        orRanges.resize( getLimitedValue< size_t, sal_Int32 >( nCount, 0, SAL_MAX_UINT16 ) );
        for( RangeList::iterator aIt = orRanges.begin(), aEnd = orRanges.end(); !mxStrm->isEof() && (aIt != aEnd); ++aIt )
            readRange( *aIt );
    }
    else
        orRanges.clear();
}

// ----------------------------------------------------------------------------

void RecordObjectBase::writeBooleanItem( const String& rName, sal_uInt8 nBool )
{
    writeDecItem( rName, nBool, "BOOLEAN" );
}

void RecordObjectBase::writeErrorCodeItem( const String& rName, sal_uInt8 nErrCode )
{
    writeHexItem( rName, nErrCode, mxErrCodes );
}

void RecordObjectBase::writeFontPortions( const FontPortionModelList& rPortions )
{
    if( !rPortions.empty() )
    {
        writeDecItem( "font-count", static_cast< sal_uInt32 >( rPortions.size() ) );
        IndentGuard aIndGuard( mxOut );
        TableGuard aTabGuard( mxOut, 14 );
        for( FontPortionModelList::const_iterator aIt = rPortions.begin(), aEnd = rPortions.end(); aIt != aEnd; ++aIt )
        {
            MultiItemsGuard aMultiGuard( mxOut );
            writeDecItem( "char-pos", aIt->mnPos );
            writeDecItem( "font-id", aIt->mnFontId, "FONTNAMES" );
        }
    }
}

void RecordObjectBase::writePhoneticPortions( const PhoneticPortionModelList& rPortions )
{
    if( !rPortions.empty() )
    {
        writeDecItem( "portion-count", static_cast< sal_uInt32 >( rPortions.size() ) );
        IndentGuard aIndGuard( mxOut );
        TableGuard aTabGuard( mxOut, 14, 21 );
        for( PhoneticPortionModelList::const_iterator aIt = rPortions.begin(), aEnd = rPortions.end(); aIt != aEnd; ++aIt )
        {
            MultiItemsGuard aMultiGuard( mxOut );
            writeDecItem( "char-pos", aIt->mnPos );
            writeDecItem( "base-text-start", aIt->mnBasePos );
            writeDecItem( "base-text-length", aIt->mnBaseLen );
        }
    }
}

// ----------------------------------------------------------------------------

sal_uInt8 RecordObjectBase::dumpBoolean( const String& rName )
{
    sal_uInt8 nBool;
    *mxStrm >> nBool;
    writeBooleanItem( rName( "boolean" ), nBool );
    return nBool;
}

sal_uInt8 RecordObjectBase::dumpErrorCode( const String& rName )
{
    sal_uInt8 nErrCode;
    *mxStrm >> nErrCode;
    writeErrorCodeItem( rName( "error-code" ), nErrCode );
    return nErrCode;
}

OUString RecordObjectBase::dumpString( const String& rName, bool bRich, bool b32BitLen )
{
    sal_uInt8 nFlags = bRich ? dumpHex< sal_uInt8 >( "flags", "STRING-FLAGS" ) : 0;

    OUString aString = BiffHelper::readString( *mxBiffStrm, b32BitLen );
    writeStringItem( rName( "text" ), aString );

    // --- formatting ---
    if( getFlag( nFlags, BIFF12_STRINGFLAG_FONTS ) )
    {
        IndentGuard aIndGuard( mxOut );
        FontPortionModelList aPortions;
        aPortions.importPortions( *mxBiffStrm );
        writeFontPortions( aPortions );
    }

    // --- phonetic text ---
    if( getFlag( nFlags, BIFF12_STRINGFLAG_PHONETICS ) )
    {
        IndentGuard aIndGuard( mxOut );
        dumpString( "phonetic-text" );
        PhoneticPortionModelList aPortions;
        aPortions.importPortions( *mxBiffStrm );
        writePhoneticPortions( aPortions );
        dumpDec< sal_uInt16 >( "font-id", "FONTNAMES" );
        dumpHex< sal_uInt16 >( "flags", "PHONETIC-FLAGS" );
    }

    return aString;
}

void RecordObjectBase::dumpColor( const String& rName )
{
    MultiItemsGuard aMultiGuard( mxOut );
    writeEmptyItem( rName( "color" ) );
    switch( extractValue< sal_uInt8 >( dumpDec< sal_uInt8 >( "flags", "COLOR-FLAGS" ), 1, 7 ) )
    {
        case 0:     dumpUnused( 1 );                                    break;
        case 1:     dumpDec< sal_uInt8 >( "index", "PALETTE-COLORS" );  break;
        case 2:     dumpUnused( 1 );                                    break;
        case 3:     dumpDec< sal_uInt8 >( "theme-id" );                 break;
        default:    dumpUnknown( 1 );
    }
    dumpDec< sal_Int16 >( "tint", "CONV-TINT" );
    dumpColorABGR();
}

DateTime RecordObjectBase::dumpPivotDateTime( const String& rName )
{
    DateTime aDateTime;
    aDateTime.Year = mxStrm->readuInt16();
    aDateTime.Month = mxStrm->readuInt16();
    aDateTime.Day = mxStrm->readuInt8();
    aDateTime.Hours = mxStrm->readuInt8();
    aDateTime.Minutes = mxStrm->readuInt8();
    aDateTime.Seconds = mxStrm->readuInt8();
    writeDateTimeItem( rName, aDateTime );
    return aDateTime;
}

sal_Int32 RecordObjectBase::dumpColIndex( const String& rName )
{
    sal_Int32 nCol;
    *mxStrm >> nCol;
    writeColIndexItem( rName( "col-idx" ), nCol );
    return nCol;
}

sal_Int32 RecordObjectBase::dumpRowIndex( const String& rName )
{
    sal_Int32 nRow;
    *mxStrm >> nRow;
    writeRowIndexItem( rName( "row-idx" ), nRow );
    return nRow;
}

sal_Int32 RecordObjectBase::dumpColRange( const String& rName )
{
    sal_Int32 nCol1, nCol2;
    *mxStrm >> nCol1 >> nCol2;
    writeColRangeItem( rName( "col-range" ), nCol1, nCol2 );
    return nCol2 - nCol1 + 1;
}

sal_Int32 RecordObjectBase::dumpRowRange( const String& rName )
{
    sal_Int32 nRow1, nRow2;
    *mxStrm >> nRow1 >> nRow2;
    writeRowRangeItem( rName( "row-range" ), nRow1, nRow2 );
    return nRow2 - nRow1 + 1;
}

Address RecordObjectBase::dumpAddress( const String& rName )
{
    Address aPos;
    readAddress( aPos );
    writeAddressItem( rName( "addr" ), aPos );
    return aPos;
}

Range RecordObjectBase::dumpRange( const String& rName )
{
    Range aRange;
    readRange( aRange );
    writeRangeItem( rName( "range" ), aRange );
    return aRange;
}

void RecordObjectBase::dumpRangeList( const String& rName )
{
    RangeList aRanges;
    readRangeList( aRanges );
    writeRangeListItem( rName( "range-list" ), aRanges );
}

// private --------------------------------------------------------------------

bool RecordObjectBase::readCompressedInt( BinaryInputStream& rStrm, sal_Int32& ornValue )
{
    ornValue = 0;
    sal_uInt8 nByte;
    rStrm >> nByte;
    ornValue = nByte & 0x7F;
    if( (nByte & 0x80) != 0 )
    {
        rStrm >> nByte;
        ornValue |= sal_Int32( nByte & 0x7F ) << 7;
        if( (nByte & 0x80) != 0 )
        {
            rStrm >> nByte;
            ornValue |= sal_Int32( nByte & 0x7F ) << 14;
            if( (nByte & 0x80) != 0 )
            {
                rStrm >> nByte;
                ornValue |= sal_Int32( nByte & 0x7F ) << 21;
            }
        }
    }
    return !rStrm.isEof();
}

// ============================================================================

FormulaObject::FormulaObject( const RecordObjectBase& rParent ) :
    mnSize( 0 )
{
    RecordObjectBase::construct( rParent );
    constructFmlaObj();
}

FormulaObject::~FormulaObject()
{
}

void FormulaObject::dumpCellFormula( const String& rName )
{
    dumpFormula( rName, false );
}

void FormulaObject::dumpNameFormula( const String& rName )
{
    dumpFormula( rName, true );
}

void FormulaObject::implDump()
{
    {
        MultiItemsGuard aMultiGuard( mxOut );
        writeEmptyItem( maName );
        writeDecItem( "formula-size", mnSize );
    }
    if( mnSize < 0 ) return;

    sal_Int64 nStartPos = mxStrm->tell();
    sal_Int64 nEndPos = ::std::min< sal_Int64 >( nStartPos + mnSize, mxStrm->size() );

    bool bValid = mxTokens.get();
    mxStack.reset( new FormulaStack );
    maAddData.clear();
    IndentGuard aIndGuard( mxOut );
    {
        TableGuard aTabGuard( mxOut, 8, 18 );
        while( bValid && (mxStrm->tell() < nEndPos) )
        {
            MultiItemsGuard aMultiGuard( mxOut );
            writeHexItem( EMPTY_STRING, static_cast< sal_uInt16 >( mxStrm->tell() - nStartPos ) );
            sal_uInt8 nTokenId = dumpHex< sal_uInt8 >( EMPTY_STRING, mxTokens );
            bValid = mxTokens->hasName( nTokenId );
            if( bValid )
            {
                sal_uInt8 nTokClass = nTokenId & BIFF_TOKCLASS_MASK;
                sal_uInt8 nBaseId = nTokenId & BIFF_TOKID_MASK;
                if( nTokClass == BIFF_TOKCLASS_NONE )
                {
                    switch( nBaseId )
                    {
                        case BIFF_TOKID_EXP:        dumpExpToken( "EXP" );          break;
                        case BIFF_TOKID_ADD:        dumpBinaryOpToken( "+" );       break;
                        case BIFF_TOKID_SUB:        dumpBinaryOpToken( "-" );       break;
                        case BIFF_TOKID_MUL:        dumpBinaryOpToken( "*" );       break;
                        case BIFF_TOKID_DIV:        dumpBinaryOpToken( "/" );       break;
                        case BIFF_TOKID_POWER:      dumpBinaryOpToken( "^" );       break;
                        case BIFF_TOKID_CONCAT:     dumpBinaryOpToken( "&" );       break;
                        case BIFF_TOKID_LT:         dumpBinaryOpToken( "<" );       break;
                        case BIFF_TOKID_LE:         dumpBinaryOpToken( "<=" );      break;
                        case BIFF_TOKID_EQ:         dumpBinaryOpToken( "=" );       break;
                        case BIFF_TOKID_GE:         dumpBinaryOpToken( ">=" );      break;
                        case BIFF_TOKID_GT:         dumpBinaryOpToken( "<" );       break;
                        case BIFF_TOKID_NE:         dumpBinaryOpToken( "<>" );      break;
                        case BIFF_TOKID_ISECT:      dumpBinaryOpToken( " " );       break;
                        case BIFF_TOKID_LIST:       dumpBinaryOpToken( "," );       break;
                        case BIFF_TOKID_RANGE:      dumpBinaryOpToken( ":" );       break;
                        case BIFF_TOKID_UPLUS:      dumpUnaryOpToken( "+", "" );    break;
                        case BIFF_TOKID_UMINUS:     dumpUnaryOpToken( "-", "" );    break;
                        case BIFF_TOKID_PERCENT:    dumpUnaryOpToken( "", "%" );    break;
                        case BIFF_TOKID_PAREN:      dumpUnaryOpToken( "(", ")" );   break;
                        case BIFF_TOKID_MISSARG:    dumpMissArgToken();             break;
                        case BIFF_TOKID_STR:        dumpStringToken();              break;
                        case BIFF_TOKID_NLR:        bValid = dumpTableToken();      break;
                        case BIFF_TOKID_ATTR:       bValid = dumpAttrToken();       break;
                        case BIFF_TOKID_ERR:        dumpErrorToken();               break;
                        case BIFF_TOKID_BOOL:       dumpBoolToken();                break;
                        case BIFF_TOKID_INT:        dumpIntToken();                 break;
                        case BIFF_TOKID_NUM:        dumpDoubleToken();              break;
                        default:                    bValid = false;
                    }
                }
                else
                {
                    OUString aTokClass = cfg().getName( mxClasses, nTokClass );
                    switch( nBaseId )
                    {
                        case BIFF_TOKID_ARRAY:      dumpArrayToken( aTokClass );                break;
                        case BIFF_TOKID_FUNC:       dumpFuncToken( aTokClass );                 break;
                        case BIFF_TOKID_FUNCVAR:    dumpFuncVarToken( aTokClass );              break;
                        case BIFF_TOKID_NAME:       dumpNameToken( aTokClass );                 break;
                        case BIFF_TOKID_REF:        dumpRefToken( aTokClass, false );           break;
                        case BIFF_TOKID_AREA:       dumpAreaToken( aTokClass, false );          break;
                        case BIFF_TOKID_MEMAREA:    dumpMemAreaToken( aTokClass, true );        break;
                        case BIFF_TOKID_MEMERR:     dumpMemAreaToken( aTokClass, false );       break;
                        case BIFF_TOKID_MEMNOMEM:   dumpMemAreaToken( aTokClass, false );       break;
                        case BIFF_TOKID_MEMFUNC:    dumpMemFuncToken( aTokClass );              break;
                        case BIFF_TOKID_REFERR:     dumpRefErrToken( aTokClass, false );        break;
                        case BIFF_TOKID_AREAERR:    dumpRefErrToken( aTokClass, true );         break;
                        case BIFF_TOKID_REFN:       dumpRefToken( aTokClass, true );            break;
                        case BIFF_TOKID_AREAN:      dumpAreaToken( aTokClass, true );           break;
                        case BIFF_TOKID_MEMAREAN:   dumpMemFuncToken( aTokClass );              break;
                        case BIFF_TOKID_MEMNOMEMN:  dumpMemFuncToken( aTokClass );              break;
                        case BIFF_TOKID_NAMEX:      dumpNameXToken( aTokClass );                break;
                        case BIFF_TOKID_REF3D:      dumpRef3dToken( aTokClass, mbNameMode );    break;
                        case BIFF_TOKID_AREA3D:     dumpArea3dToken( aTokClass, mbNameMode );   break;
                        case BIFF_TOKID_REFERR3D:   dumpRefErr3dToken( aTokClass, false );      break;
                        case BIFF_TOKID_AREAERR3D:  dumpRefErr3dToken( aTokClass, true );       break;
                        default:                    bValid = false;
                    }
                }
            }
        }
    }

    if( nEndPos == mxStrm->tell() )
    {
        dumpAddTokenData();
        if( mnSize > 0 )
        {
            writeInfoItem( "formula", mxStack->getFormulaString() );
            writeInfoItem( "classes", mxStack->getClassesString() );
        }
    }
    else
    {
        dumpBinary( OOX_DUMP_ERRASCII( "formula-error" ), static_cast< sal_Int32 >( nEndPos - mxStrm->tell() ), false );
        sal_Int32 nAddDataSize = dumpDec< sal_Int32 >( "add-data-size" );
        dumpBinary( "add-data", nAddDataSize, false );
    }

    mnSize = 0;
}

void FormulaObject::dumpFormula( const String& rName, bool bNameMode )
{
    maName = rName( "formula" );
    *mxStrm >> mnSize;
    mbNameMode = bNameMode;
    dump();
}

// private --------------------------------------------------------------------

void FormulaObject::constructFmlaObj()
{
    if( RecordObjectBase::implIsValid() )
    {
        mxFuncProv.reset( new FunctionProvider( FILTER_OOXML, BIFF_UNKNOWN, true ) );

        Config& rCfg = cfg();
        mxClasses   = rCfg.getNameList( "TOKENCLASSES" );
        mxRelFlags  = rCfg.getNameList( "REFRELFLAGS" );
        mxAttrTypes = rCfg.getNameList( "ATTRTYPES" );
        mxSpTypes   = rCfg.getNameList( "ATTRSPACETYPES" );

        // create classified token names
        mxTokens = rCfg.createNameList< ConstList >( "TOKENS" );
        mxTokens->includeList( rCfg.getNameList( "BASETOKENS" ) );

        NameListRef xClassTokens = rCfg.getNameList( "CLASSTOKENS" );
        if( mxClasses.get() && xClassTokens.get() )
            for( NameListBase::const_iterator aCIt = mxClasses->begin(), aCEnd = mxClasses->end(); aCIt != aCEnd; ++aCIt )
                for( NameListBase::const_iterator aTIt = xClassTokens->begin(), aTEnd = xClassTokens->end(); aTIt != aTEnd; ++aTIt )
                    mxTokens->setName( aCIt->first | aTIt->first, aTIt->second + aCIt->second );

        mnColCount = 16384;
        mnRowCount = 1024 * 1024;
    }
}

// ----------------------------------------------------------------------------

namespace {

OUString lclCreateName( const OUString& rRef, sal_Int32 nNameId )
{
    OUStringBuffer aName( rRef );
    StringHelper::appendIndexedText( aName, CREATE_OUSTRING( "NAME" ), nNameId );
    return aName.makeStringAndClear();
}

} // namespace

// ----------------------------------------------------------------------------

TokenAddress FormulaObject::createTokenAddress( sal_Int32 nCol, sal_Int32 nRow, bool bRelC, bool bRelR, bool bNameMode ) const
{
    TokenAddress aPos;
    aPos.mnCol = nCol;
    if( bRelC && bNameMode && (nCol >= mnColCount / 2) ) aPos.mnCol -= mnColCount;
    aPos.mbRelCol = bRelC;
    aPos.mnRow = nRow;
    if( bRelR && bNameMode && (nRow >= mnRowCount / 2) ) aPos.mnRow -= mnRowCount;
    aPos.mbRelRow = bRelR;
    return aPos;
}

OUString FormulaObject::createRef( const OUString& rData ) const
{
    return maRefPrefix + rData;
}

OUString FormulaObject::createName( sal_Int32 nNameId ) const
{
    return lclCreateName( maRefPrefix, nNameId );
}

OUString FormulaObject::createPlaceHolder( size_t nIdx ) const
{
    OUStringBuffer aStr;
    StringHelper::appendDec( aStr, static_cast< sal_uInt32 >( nIdx ) );
    StringHelper::enclose( aStr, OOX_DUMP_PLACEHOLDER );
    return aStr.makeStringAndClear();
}

OUString FormulaObject::createPlaceHolder() const
{
    return createPlaceHolder( maAddData.size() );
}

OUString FormulaObject::writeFuncIdItem( sal_uInt16 nFuncId, const FunctionInfo** oppFuncInfo )
{
    ItemGuard aItem( mxOut, "func-id" );
    writeHexItem( EMPTY_STRING, nFuncId, "FUNCID" );
    OUStringBuffer aBuffer;
    const FunctionInfo* pFuncInfo = mxFuncProv->getFuncInfoFromBiff12FuncId( nFuncId );
    if( pFuncInfo )
        aBuffer.append( pFuncInfo->maOoxFuncName );
    else
    {
        bool bCmd = getFlag( nFuncId, BIFF_TOK_FUNCVAR_CMD );
        aBuffer.appendAscii( bCmd ? "CMD" : "FUNC" );
        StringHelper::appendIndex( aBuffer, nFuncId & BIFF_TOK_FUNCVAR_FUNCIDMASK );
    }
    OUString aFuncName = aBuffer.makeStringAndClear();
    aItem.cont();
    mxOut->writeChar( OOX_DUMP_STRQUOTE );
    mxOut->writeString( aFuncName );
    mxOut->writeChar( OOX_DUMP_STRQUOTE );
    if( oppFuncInfo ) *oppFuncInfo = pFuncInfo;
    return aFuncName;
}

sal_Int32 FormulaObject::dumpTokenCol( const String& rName, bool& rbRelC, bool& rbRelR )
{
    sal_uInt16 nCol = dumpHex< sal_uInt16 >( rName, mxRelFlags );
    rbRelC = getFlag( nCol, BIFF12_TOK_REF_COLREL );
    rbRelR = getFlag( nCol, BIFF12_TOK_REF_ROWREL );
    nCol &= BIFF12_TOK_REF_COLMASK;
    return nCol;
}

sal_Int32 FormulaObject::dumpTokenRow( const String& rName )
{
    return dumpDec< sal_Int32 >( rName );
}

TokenAddress FormulaObject::dumpTokenAddress( bool bNameMode )
{
    bool bRelC = false;
    bool bRelR = false;
    sal_Int32 nRow = dumpTokenRow( "row" );
    sal_Int32 nCol = dumpTokenCol( "col", bRelC, bRelR );
    return createTokenAddress( nCol, nRow, bRelC, bRelR, bNameMode );
}

TokenRange FormulaObject::dumpTokenRange( bool bNameMode )
{
    bool bRelC1 = false;
    bool bRelR1 = false;
    bool bRelC2 = false;
    bool bRelR2 = false;
    sal_Int32 nRow1 = dumpTokenRow( "row1" );
    sal_Int32 nRow2 = dumpTokenRow( "row2" );
    sal_Int32 nCol1 = dumpTokenCol( "col1", bRelC1, bRelR1 );
    sal_Int32 nCol2 = dumpTokenCol( "col2", bRelC2, bRelR2 );
    TokenRange aRange;
    aRange.maFirst = createTokenAddress( nCol1, nRow1, bRelC1, bRelR1, bNameMode );
    aRange.maLast  = createTokenAddress( nCol2, nRow2, bRelC2, bRelR2, bNameMode );
    return aRange;
}

sal_Int16 FormulaObject::readTokenRefId()
{
    return dumpDec< sal_Int16 >( "ref-id" );
}

OUString FormulaObject::dumpTokenRefId()
{
    OUStringBuffer aRef( CREATE_OUSTRING( "REF" ) );
    StringHelper::appendIndex( aRef, readTokenRefId() );
    aRef.append( OOX_DUMP_TABSEP );
    return aRef.makeStringAndClear();
}

void FormulaObject::dumpIntToken()
{
    dumpDec< sal_uInt16 >( "value" );
    mxStack->pushOperand( mxOut->getLastItemValue() );
}

void FormulaObject::dumpDoubleToken()
{
    dumpDec< double >( "value" );
    mxStack->pushOperand( mxOut->getLastItemValue() );
}

void FormulaObject::dumpStringToken()
{
    OUStringBuffer aBuffer( dumpString( "value", false, false ) );
    StringHelper::enclose( aBuffer, OOX_DUMP_FMLASTRQUOTE );
    mxStack->pushOperand( aBuffer.makeStringAndClear() );
}

void FormulaObject::dumpBoolToken()
{
    dumpBoolean( "value" );
    mxStack->pushOperand( mxOut->getLastItemValue() );
}

void FormulaObject::dumpErrorToken()
{
    dumpErrorCode( "value" );
    mxStack->pushOperand( mxOut->getLastItemValue() );
}

void FormulaObject::dumpMissArgToken()
{
    mxStack->pushOperand( OUString( OOX_DUMP_EMPTYVALUE ) );
}

void FormulaObject::dumpArrayToken( const OUString& rTokClass )
{
    dumpUnused( 14 );
    mxStack->pushOperand( createPlaceHolder(), rTokClass );
    maAddData.push_back( ADDDATA_ARRAY );
}

void FormulaObject::dumpNameToken( const OUString& rTokClass )
{
    sal_Int32 nNameId = dumpDec< sal_Int32 >( "name-id" );
    mxStack->pushOperand( createName( nNameId ), rTokClass );
}

void FormulaObject::dumpNameXToken( const OUString& rTokClass )
{
    OUString aRef = dumpTokenRefId();
    sal_Int32 nNameId = dumpDec< sal_Int32 >( "name-id" );
    mxStack->pushOperand( lclCreateName( aRef, nNameId ), rTokClass );
}

void FormulaObject::dumpRefToken( const OUString& rTokClass, bool bNameMode )
{
    TokenAddress aPos = dumpTokenAddress( bNameMode );
    writeTokenAddressItem( "addr", aPos, bNameMode );
    mxStack->pushOperand( createRef( mxOut->getLastItemValue() ), rTokClass );
}

void FormulaObject::dumpAreaToken( const OUString& rTokClass, bool bNameMode )
{
    TokenRange aRange = dumpTokenRange( bNameMode );
    writeTokenRangeItem( "range", aRange, bNameMode );
    mxStack->pushOperand( createRef( mxOut->getLastItemValue() ), rTokClass );
}

void FormulaObject::dumpRefErrToken( const OUString& rTokClass, bool bArea )
{
    dumpUnused( 4 * (bArea ? 2 : 1) );
    mxStack->pushOperand( createRef( getErrorName( BIFF_ERR_REF ) ), rTokClass );
}

void FormulaObject::dumpRef3dToken( const OUString& rTokClass, bool bNameMode )
{
    OUString aRef = dumpTokenRefId();
    TokenAddress aPos = dumpTokenAddress( bNameMode );
    writeTokenAddress3dItem( "addr", aRef, aPos, bNameMode );
    mxStack->pushOperand( mxOut->getLastItemValue(), rTokClass );
}

void FormulaObject::dumpArea3dToken( const OUString& rTokClass, bool bNameMode )
{
    OUString aRef = dumpTokenRefId();
    TokenRange aRange = dumpTokenRange( bNameMode );
    writeTokenRange3dItem( "range", aRef, aRange, bNameMode );
    mxStack->pushOperand( mxOut->getLastItemValue(), rTokClass );
}

void FormulaObject::dumpRefErr3dToken( const OUString& rTokClass, bool bArea )
{
    OUString aRef = dumpTokenRefId();
    dumpUnused( 4 * (bArea ? 2 : 1) );
    mxStack->pushOperand( aRef + getErrorName( BIFF_ERR_REF ), rTokClass );
}

void FormulaObject::dumpMemFuncToken( const OUString& /*rTokClass*/ )
{
    dumpDec< sal_uInt16 >( "size" );
}

void FormulaObject::dumpMemAreaToken( const OUString& rTokClass, bool bAddData )
{
    dumpUnused( 4 );
    dumpMemFuncToken( rTokClass );
    if( bAddData )
        maAddData.push_back( ADDDATA_MEMAREA );
}

void FormulaObject::dumpExpToken( const String& rName )
{
    Address aPos;
    dumpRowIndex( "base-row" );
    OUStringBuffer aOp( rName );
    StringHelper::appendIndex( aOp, createPlaceHolder() + mxOut->getLastItemValue() );
    mxStack->pushOperand( aOp.makeStringAndClear() );
    maAddData.push_back( ADDDATA_EXP );
}

void FormulaObject::dumpUnaryOpToken( const String& rLOp, const String& rROp )
{
    mxStack->pushUnaryOp( rLOp, rROp );
}

void FormulaObject::dumpBinaryOpToken( const String& rOp )
{
    mxStack->pushBinaryOp( rOp );
}

void FormulaObject::dumpFuncToken( const OUString& rTokClass )
{
    sal_uInt16 nFuncId;
    *mxStrm >> nFuncId;
    const FunctionInfo* pFuncInfo = 0;
    OUString aFuncName = writeFuncIdItem( nFuncId, &pFuncInfo );
    if( pFuncInfo && (pFuncInfo->mnMinParamCount == pFuncInfo->mnMaxParamCount) )
        mxStack->pushFuncOp( aFuncName, rTokClass, pFuncInfo->mnMinParamCount );
    else
        mxStack->setError();
}

void FormulaObject::dumpFuncVarToken( const OUString& rTokClass )
{
    sal_uInt8 nParamCount;
    sal_uInt16 nFuncId;
    *mxStrm >> nParamCount >> nFuncId;
    bool bCmd = getFlag( nFuncId, BIFF_TOK_FUNCVAR_CMD );
    if( bCmd )
        writeHexItem( "param-count", nParamCount, "PARAMCOUNT-CMD" );
    else
        writeDecItem( "param-count", nParamCount );
    OUString aFuncName = writeFuncIdItem( nFuncId );
    if( bCmd && getFlag( nParamCount, BIFF_TOK_FUNCVAR_CMDPROMPT ) )
    {
        aFuncName += OUString( OOX_DUMP_CMDPROMPT );
        nParamCount &= BIFF_TOK_FUNCVAR_COUNTMASK;
    }
    mxStack->pushFuncOp( aFuncName, rTokClass, nParamCount );
}

bool FormulaObject::dumpTableToken()
{
    dumpUnused( 3 );
    sal_uInt16 nFlags = dumpHex< sal_uInt16 >( "flags", "TABLEFLAGS" );
    sal_uInt16 nTabId = dumpDec< sal_uInt16 >( "table-id" );
    dumpUnused( 2 );
    {
        sal_uInt16 nCol1, nCol2;
        *mxStrm >> nCol1 >> nCol2;
        ItemGuard aItem( mxOut, "cols" );
        mxOut->writeDec( nCol1 );
        if( nCol1 != nCol2 )
        {
            mxOut->writeChar( OOX_DUMP_RANGESEP );
            mxOut->writeDec( nCol2 );
        }
    }
    OUStringBuffer aColRange;
    StringHelper::appendIndex( aColRange, mxOut->getLastItemValue() );
    OUStringBuffer aParams;
    size_t nParams = 0;
    if( getFlag( nFlags, BIFF12_TOK_TABLE_ALL ) && ++nParams )
        StringHelper::appendToken( aParams, CREATE_OUSTRING( "[#All]" ) );
    if( getFlag( nFlags, BIFF12_TOK_TABLE_HEADERS ) && ++nParams )
        StringHelper::appendToken( aParams, CREATE_OUSTRING( "[#Headers]" ) );
    if( getFlag( nFlags, BIFF12_TOK_TABLE_DATA ) && ++nParams )
        StringHelper::appendToken( aParams, CREATE_OUSTRING( "[#Data]" ) );
    if( getFlag( nFlags, BIFF12_TOK_TABLE_TOTALS ) && ++nParams )
        StringHelper::appendToken( aParams, CREATE_OUSTRING( "[#Totals]" ) );
    if( getFlag( nFlags, BIFF12_TOK_TABLE_THISROW ) && ++nParams )
        StringHelper::appendToken( aParams, CREATE_OUSTRING( "[#This Row]" ) );
    if( (getFlag( nFlags, BIFF12_TOK_TABLE_COLUMN ) || getFlag( nFlags, BIFF12_TOK_TABLE_COLRANGE )) && ++nParams )
        StringHelper::appendToken( aParams, aColRange.makeStringAndClear() );
    OUStringBuffer aOp;
    StringHelper::appendIndexedText( aOp, CREATE_OUSTRING( "TABLE" ), nTabId );
    if( nParams > 1 )
        StringHelper::appendIndex( aOp, aParams.makeStringAndClear() );
    else if( nParams == 1 )
        aOp.append( aParams.makeStringAndClear() );
    mxStack->pushOperand( aOp.makeStringAndClear() );
    return true;
}

bool FormulaObject::dumpAttrToken()
{
    bool bValid = true;
    sal_uInt8 nType = dumpHex< sal_uInt8 >( "type", mxAttrTypes );
    switch( nType )
    {
        case BIFF_TOK_ATTR_VOLATILE:
            dumpUnused( 2 );
        break;
        case BIFF_TOK_ATTR_IF:
            dumpDec< sal_uInt16 >( "skip" );
        break;
        case BIFF_TOK_ATTR_CHOOSE:
        {
            sal_uInt16 nCount = dumpDec< sal_uInt16 >( "choices" );
            mxOut->resetItemIndex();
            for( sal_uInt16 nIdx = 0; nIdx < nCount; ++nIdx )
                dumpDec< sal_uInt16 >( "#skip" );
            dumpDec< sal_uInt16 >( "skip-err" );
        }
        break;
        case BIFF_TOK_ATTR_SKIP:
            dumpDec< sal_uInt16 >( "skip" );
        break;
        case BIFF_TOK_ATTR_SUM:
            dumpUnused( 2 );
            mxStack->pushFuncOp( CREATE_OUSTRING( "SUM" ), OUString( OOX_DUMP_BASECLASS ), 1 );
        break;
        case BIFF_TOK_ATTR_ASSIGN:
            dumpUnused( 2 );
        break;
        case BIFF_TOK_ATTR_SPACE:
        case BIFF_TOK_ATTR_SPACE | BIFF_TOK_ATTR_VOLATILE:
            dumpDec< sal_uInt8 >( "char-type", mxSpTypes );
            dumpDec< sal_uInt8 >( "char-count" );
        break;
        case BIFF_TOK_ATTR_IFERROR:
            dumpDec< sal_uInt16 >( "skip" );
        break;
        default:
            bValid = false;
    }
    return bValid;
}

void FormulaObject::dumpAddTokenData()
{
    mxOut->resetItemIndex();
    sal_Int32 nAddDataSize = (mxStrm->size() - mxStrm->tell() >= 4) ? dumpDec< sal_Int32 >( "add-data-size" ) : 0;
    sal_Int64 nEndPos = ::std::min< sal_Int64 >( mxStrm->tell() + nAddDataSize, mxStrm->size() );
    for( AddDataTypeVec::const_iterator aIt = maAddData.begin(), aEnd = maAddData.end(); (aIt != aEnd) && !mxStrm->isEof() && (mxStrm->tell() < nEndPos); ++aIt )
    {
        AddDataType eType = *aIt;

        {
            ItemGuard aItem( mxOut, "#add-data" );
            switch( eType )
            {
                case ADDDATA_EXP:       mxOut->writeAscii( "tExp" );      break;
                case ADDDATA_ARRAY:     mxOut->writeAscii( "tArray" );    break;
                case ADDDATA_MEMAREA:   mxOut->writeAscii( "tMemArea" );  break;
            }
        }

        size_t nIdx = aIt - maAddData.begin();
        IndentGuard aIndGuard( mxOut );
        switch( eType )
        {
            case ADDDATA_EXP:       dumpAddDataExp( nIdx );     break;
            case ADDDATA_ARRAY:     dumpAddDataArray( nIdx );   break;
            case ADDDATA_MEMAREA:   dumpAddDataMemArea( nIdx ); break;
            default:;
        }
    }
}

void FormulaObject::dumpAddDataExp( size_t nIdx )
{
    dumpColIndex( "base-col" );
    mxStack->replaceOnTop( createPlaceHolder( nIdx ), mxOut->getLastItemValue() );
}

void FormulaObject::dumpAddDataArray( size_t nIdx )
{
    sal_Int32 nCols, nRows;
    dumpaddDataArrayHeader( nCols, nRows );

    OUStringBuffer aOp;
    TableGuard aTabGuard( mxOut, 17 );
    for( sal_Int32 nRow = 0; nRow < nRows; ++nRow )
    {
        OUStringBuffer aArrayLine;
        for( sal_Int32 nCol = 0; nCol < nCols; ++nCol )
            StringHelper::appendToken( aArrayLine, dumpaddDataArrayValue(), OOX_DUMP_LISTSEP );
        StringHelper::appendToken( aOp, aArrayLine.makeStringAndClear(), OOX_DUMP_ARRAYSEP );
    }
    StringHelper::enclose( aOp, '{', '}' );
    mxStack->replaceOnTop( createPlaceHolder( nIdx ), aOp.makeStringAndClear() );
}

void FormulaObject::dumpAddDataMemArea( size_t /*nIdx*/ )
{
    dumpRangeList();
}

void FormulaObject::dumpaddDataArrayHeader( sal_Int32& rnCols, sal_Int32& rnRows )
{
    MultiItemsGuard aMultiGuard( mxOut );
    rnRows = dumpDec< sal_Int32 >( "height" );
    rnCols = dumpDec< sal_Int32 >( "width" );
    ItemGuard aItem( mxOut, "size" );
    mxOut->writeDec( rnCols );
    mxOut->writeChar( 'x' );
    mxOut->writeDec( rnRows );
    aItem.cont();
    mxOut->writeDec( rnCols * rnRows );
}

OUString FormulaObject::dumpaddDataArrayValue()
{
    MultiItemsGuard aMultiGuard( mxOut );
    OUStringBuffer aValue;
    switch( dumpDec< sal_uInt8 >( "type", "ARRAYVALUE-TYPE" ) )
    {
        case BIFF_TOK_ARRAY_DOUBLE:
            dumpDec< double >( "value" );
            aValue.append( mxOut->getLastItemValue() );
        break;
        case BIFF_TOK_ARRAY_STRING:
            aValue.append( dumpString( "value", false, false ) );
            StringHelper::enclose( aValue, OOX_DUMP_STRQUOTE );
        break;
        case BIFF_TOK_ARRAY_BOOL:
            dumpBoolean( "value" );
            aValue.append( mxOut->getLastItemValue() );
        break;
        case BIFF_TOK_ARRAY_ERROR:
            dumpErrorCode( "value" );
            aValue.append( mxOut->getLastItemValue() );
            dumpUnused( 3 );
        break;
    }
    return aValue.makeStringAndClear();
}

// ============================================================================

RecordStreamObject::RecordStreamObject( ObjectBase& rParent, const BinaryInputStreamRef& rxStrm, const OUString& rSysFileName )
{
    RecordObjectBase::construct( rParent, rxStrm, rSysFileName );
    if( RecordObjectBase::implIsValid() )
        mxFmlaObj.reset( new FormulaObject( *this ) );
}

bool RecordStreamObject::implIsValid() const
{
    return isValid( mxFmlaObj ) && RecordObjectBase::implIsValid();
}

void RecordStreamObject::implDumpRecordBody()
{
    switch( getRecId() )
    {
        case BIFF12_ID_ARRAY:
            dumpRange( "array-range" );
            dumpHex< sal_uInt8 >( "flags", "ARRAY-FLAGS" );
            mxFmlaObj->dumpCellFormula();
        break;

        case BIFF12_ID_AUTOFILTER:
            dumpRange( "filter-range" );
        break;
        
        case BIFF12_ID_BINARYINDEXBLOCK:
            dumpRowRange( "row-range" );
            dumpUnknown( 12 );
        break;

        case BIFF12_ID_BINARYINDEXROWS:
        {
            sal_uInt32 nUsedRows = dumpBin< sal_uInt32 >( "used-rows" );
            dumpDec< sal_Int64 >( "stream-offset" );
            for( ; nUsedRows > 0; nUsedRows >>= 1 )
                if( (nUsedRows & 1) != 0 )
                    dumpBin< sal_uInt16 >( "used-columns" );
        }
        break;

        case BIFF12_ID_BORDER:
            dumpHex< sal_uInt8 >( "flags", "BORDER-FLAGS" );
            dumpDec< sal_uInt16 >( "top-style", "BORDERSTYLES" );
            dumpColor( "top-color" );
            dumpDec< sal_uInt16 >( "bottom-style", "BORDERSTYLES" );
            dumpColor( "bottom-color" );
            dumpDec< sal_uInt16 >( "left-style", "BORDERSTYLES" );
            dumpColor( "left-color" );
            dumpDec< sal_uInt16 >( "right-style", "BORDERSTYLES" );
            dumpColor( "right-color" );
            dumpDec< sal_uInt16 >( "diag-style", "BORDERSTYLES" );
            dumpColor( "diag-color" );
        break;

        case BIFF12_ID_BRK:
            dumpDec< sal_Int32 >( "id" );
            dumpDec< sal_Int32 >( "min" );
            dumpDec< sal_Int32 >( "max" );
            dumpDec< sal_Int32 >( "manual-break", "BOOLEAN" );
            dumpDec< sal_Int32 >( "pivot-break", "BOOLEAN" );
        break;

        case BIFF12_ID_CALCPR:
            dumpDec< sal_Int32 >( "calc-id" );
            dumpDec< sal_Int32 >( "calc-mode", "CALCPR-CALCMODE" );
            dumpDec< sal_Int32 >( "iteration-count" );
            dumpDec< double >( "iteration-delta" );
            dumpDec< sal_Int32 >( "processor-count" );
            dumpHex< sal_uInt16 >( "flags", "CALCPR-FLAGS" );
        break;

        case BIFF12_ID_CELL_BLANK:
            dumpCellHeader( true );
        break;

        case BIFF12_ID_CELL_BOOL:
            dumpCellHeader( true );
            dumpBoolean();
        break;

        case BIFF12_ID_CELL_DOUBLE:
            dumpCellHeader( true );
            dumpDec< double >( "value" );
        break;

        case BIFF12_ID_CELL_ERROR:
            dumpCellHeader( true );
            dumpErrorCode();
        break;

        case BIFF12_ID_CELL_RK:
            dumpCellHeader( true );
            dumpRk( "value" );
        break;

        case BIFF12_ID_CELL_RSTRING:
            dumpCellHeader( true );
            dumpString( "value", true );
        break;

        case BIFF12_ID_CELL_SI:
            dumpCellHeader( true );
            dumpDec< sal_Int32 >( "string-id" );
        break;

        case BIFF12_ID_CELL_STRING:
            dumpCellHeader( true );
            dumpString( "value" );
        break;

        case BIFF12_ID_CELLSTYLE:
            dumpDec< sal_Int32 >( "xf-id" );
            dumpHex< sal_uInt16 >( "flags", "CELLSTYLE-FLAGS" );
            dumpDec< sal_uInt8 >( "builtin-id", "CELLSTYLE-BUILTIN" );
            dumpDec< sal_uInt8 >( "outline-level" );
            dumpString( "name" );
        break;

        case BIFF12_ID_CFCOLOR:
            dumpColor();
        break;

        case BIFF12_ID_CFRULE:
        {
            // type/subtype/operator is a mess...
            dumpDec< sal_Int32 >( "type", "CFRULE-TYPE" );
            sal_Int32 nSubType = dumpDec< sal_Int32 >( "sub-type", "CFRULE-SUBTYPE" );
            dumpDec< sal_Int32 >( "dxf-id" );
            dumpDec< sal_Int32 >( "priority" );
            switch( nSubType )
            {
                case 0:     dumpDec< sal_Int32 >( "operator", "CFRULE-CELL-OPERATOR" ); break;
                case 5:     dumpDec< sal_Int32 >( "rank" );                             break;
                case 8:     dumpDec< sal_Int32 >( "operator", "CFRULE-TEXT-OPERATOR" ); break;
                case 15:    dumpDec< sal_Int32 >( "operator", "CFRULE-DATE-OPERATOR" ); break;
                case 16:    dumpDec< sal_Int32 >( "operator", "CFRULE-DATE-OPERATOR" ); break;
                case 17:    dumpDec< sal_Int32 >( "operator", "CFRULE-DATE-OPERATOR" ); break;
                case 18:    dumpDec< sal_Int32 >( "operator", "CFRULE-DATE-OPERATOR" ); break;
                case 19:    dumpDec< sal_Int32 >( "operator", "CFRULE-DATE-OPERATOR" ); break;
                case 20:    dumpDec< sal_Int32 >( "operator", "CFRULE-DATE-OPERATOR" ); break;
                case 21:    dumpDec< sal_Int32 >( "operator", "CFRULE-DATE-OPERATOR" ); break;
                case 22:    dumpDec< sal_Int32 >( "operator", "CFRULE-DATE-OPERATOR" ); break;
                case 23:    dumpDec< sal_Int32 >( "operator", "CFRULE-DATE-OPERATOR" ); break;
                case 24:    dumpDec< sal_Int32 >( "operator", "CFRULE-DATE-OPERATOR" ); break;
                case 25:    dumpDec< sal_Int32 >( "std-dev" );                          break;
                case 26:    dumpDec< sal_Int32 >( "std-dev" );                          break;
                case 29:    dumpDec< sal_Int32 >( "std-dev" );                          break;
                case 30:    dumpDec< sal_Int32 >( "std-dev" );                          break;
                default:    dumpDec< sal_Int32 >( "operator", "CFRULE-OTHER-OPERATOR" );
            }
            dumpUnknown( 8 );
            dumpHex< sal_uInt16 >( "flags", "CFRULE-FLAGS" );
            // for no obvious reason the formula sizes occur twice
            dumpDec< sal_Int32 >( "formula1-size" );
            dumpDec< sal_Int32 >( "formula2-size" );
            dumpDec< sal_Int32 >( "formula3-size" );
            dumpString( "text" );
            if( mxStrm->getRemaining() >= 8 )
                mxFmlaObj->dumpNameFormula( "formula1" );
            if( mxStrm->getRemaining() >= 8 )
                mxFmlaObj->dumpNameFormula( "formula2" );
            if( mxStrm->getRemaining() >= 8 )
                mxFmlaObj->dumpNameFormula( "formula3" );
        }
        break;

        case BIFF12_ID_CHARTPAGESETUP:
            dumpDec< sal_Int32 >( "paper-size", "PAGESETUP-PAPERSIZE" );
            dumpDec< sal_Int32 >( "horizontal-res", "PAGESETUP-DPI" );
            dumpDec< sal_Int32 >( "vertical-res", "PAGESETUP-DPI" );
            dumpDec< sal_Int32 >( "copies" );
            dumpDec< sal_uInt16 >( "first-page" );
            dumpHex< sal_uInt16 >( "flags", "CHARTPAGESETUP-FLAGS" );
            dumpString( "printer-settings-rel-id" );
        break;

        case BIFF12_ID_CHARTPROTECTION:
            dumpHex< sal_uInt16 >( "password-hash" );
            // no flags field for the boolean flags?!?
            dumpDec< sal_Int32 >( "content-locked", "BOOLEAN" );
            dumpDec< sal_Int32 >( "objects-locked", "BOOLEAN" );
        break;

        case BIFF12_ID_CHARTSHEETPR:
            dumpHex< sal_uInt16 >( "flags", "CHARTSHEETPR-FLAGS" );
            dumpColor( "tab-color" );
            dumpString( "codename" );
        break;

        case BIFF12_ID_CHARTSHEETVIEW:
            dumpHex< sal_uInt16 >( "flags", "CHARTSHEETVIEW-FLAGS" );
            dumpDec< sal_Int32 >( "zoom-scale", "CONV-PERCENT" );
            dumpDec< sal_Int32 >( "workbookview-id" );
        break;

        case BIFF12_ID_COL:
            dumpColRange();
            dumpDec< sal_Int32 >( "col-width", "CONV-COLWIDTH" );
            dumpDec< sal_Int32 >( "custom-xf-id" );
            dumpHex< sal_uInt16 >( "flags", "COL-FLAGS" );
        break;

        case BIFF12_ID_COLBREAKS:
            dumpDec< sal_Int32 >( "count" );
            dumpDec< sal_Int32 >( "manual-count" );
        break;

        case BIFF12_ID_COLOR:
            dumpColor();
        break;

        case BIFF12_ID_COMMENT:
            dumpDec< sal_Int32 >( "author-id" );
            dumpRange( "ref" );
            dumpGuid();
        break;

        case BIFF12_ID_COMMENTAUTHOR:
            dumpString( "author" );
        break;

        case BIFF12_ID_COMMENTTEXT:
            dumpString( "text", true );
        break;

        case BIFF12_ID_CONDFORMATTING:
            dumpDec< sal_Int32 >( "cfrule-count" );
            dumpDec< sal_Int32 >( "pivot-table", "BOOLEAN" );
            dumpRangeList();
        break;

        case BIFF12_ID_CONNECTION:
        {
            dumpDec< sal_uInt8 >( "refreshed-version" );
            dumpDec< sal_uInt8 >( "min-refresh-version" );
            dumpDec< sal_uInt8 >( "save-password", "CONNECTION-SAVEPASSWORD" );
            dumpUnused( 1 );
            dumpDec< sal_uInt16 >( "refresh-interval", "CONNECTION-INTERVAL" );
            dumpHex< sal_uInt16 >( "flags", "CONNECTION-FLAGS" );
            sal_uInt16 nStrFlags = dumpHex< sal_uInt16 >( "string-flags", "CONNECTION-STRINGFLAGS" );
            dumpDec< sal_Int32 >( "data-source-type", "CONNECTION-SOURCETYPE" );
            dumpDec< sal_Int32 >( "reconnect-type", "CONNECTION-RECONNECTTYPE" );
            dumpDec< sal_Int32 >( "id" );
            dumpDec< sal_uInt8 >( "credentials", "CONNECTION-CREDENTIALS" );
            if( nStrFlags & 0x0001 ) dumpString( "source-file" );
            if( nStrFlags & 0x0002 ) dumpString( "source-conn-file" );
            if( nStrFlags & 0x0004 ) dumpString( "description" );
            if( nStrFlags & 0x0008 ) dumpString( "name" );
            if( nStrFlags & 0x0010 ) dumpString( "sso-id" );
        }
        break;

        case BIFF12_ID_CONTROL:
            dumpDec< sal_Int32 >( "shape-id" );
            dumpString( "rel-id" );
            dumpString( "name" );
        break;

        case BIFF12_ID_CUSTOMFILTER:
        {
            sal_uInt8 nType = dumpDec< sal_uInt8 >( "data-type", "CUSTOMFILTER-DATATYPE" );
            dumpDec< sal_uInt8 >( "operator", "CUSTOMFILTER-OPERATOR" );
            switch( nType )
            {
                case 4:     dumpDec< double >( "value" );               break;
                case 6:     dumpUnused( 8 ); dumpString( "value" );     break;
                case 8:     dumpBoolean( "value" ); dumpUnused( 7 );    break;
                default:    dumpUnused( 8 );
            }
        }
        break;

        case BIFF12_ID_DATATABLE:
            dumpRange( "table-range" );
            dumpAddress( "ref1" );
            dumpAddress( "ref2" );
            dumpHex< sal_uInt8 >( "flags", "DATATABLE-FLAGS" );
        break;

        case BIFF12_ID_DATAVALIDATION:
            dumpHex< sal_uInt32 >( "flags", "DATAVALIDATION-FLAGS" );
            dumpRangeList();
            dumpString( "error-title" );
            dumpString( "error-message" );
            dumpString( "input-title" );
            dumpString( "input-message" );
            mxFmlaObj->dumpNameFormula( "formula1" );
            mxFmlaObj->dumpNameFormula( "formula2" );
        break;

        case BIFF12_ID_DATAVALIDATIONS:
            dumpHex< sal_uInt16 >( "flags", "DATAVALIDATIONS-FLAGS" );
            dumpDec< sal_Int32 >( "input-x" );
            dumpDec< sal_Int32 >( "input-y" );
            dumpUnused( 4 );
            dumpDec< sal_Int32 >( "count" );
        break;

        case BIFF12_ID_DDEITEMVALUES:
            dumpDec< sal_Int32 >( "rows" );
            dumpDec< sal_Int32 >( "columns" );
        break;

        case BIFF12_ID_DDEITEM_STRING:
            dumpString( "value" );
        break;

        case BIFF12_ID_DEFINEDNAME:
            dumpHex< sal_uInt32 >( "flags", "DEFINEDNAME-FLAGS" );
            dumpChar( "accelerator", RTL_TEXTENCODING_ISO_8859_1 );
            dumpDec< sal_Int32 >( "sheet-id", "DEFINEDNAME-SHEETID" );
            dumpString( "name" );
            mxFmlaObj->dumpNameFormula();
            dumpString( "comment" );
            if( mxStrm->getRemaining() >= 4 ) dumpString( "menu-text" );
            if( mxStrm->getRemaining() >= 4 ) dumpString( "description-text" );
            if( mxStrm->getRemaining() >= 4 ) dumpString( "help-text" );
            if( mxStrm->getRemaining() >= 4 ) dumpString( "statusbar-text" );
        break;

        case BIFF12_ID_DIMENSION:
            dumpRange( "used-range" );
        break;

        case BIFF12_ID_DISCRETEFILTER:
            dumpString( "value" );
        break;

        case BIFF12_ID_DISCRETEFILTERS:
            dumpBool< sal_Int32 >( "show-blank" );
            dumpDec< sal_Int32 >( "calendar-type", "DISCRETEFILTERS-CALTYPE" );
        break;

        case BIFF12_ID_DRAWING:
            dumpString( "rel-id" );
        break;

        case BIFF12_ID_DXF:
            dumpHex< sal_uInt32 >( "flags", "DXF-FLAGS" );
            for( sal_uInt16 nIndex = 0, nCount = dumpDec< sal_uInt16 >( "subrec-count" ); !mxStrm->isEof() && (nIndex < nCount); ++nIndex )
            {
                mxOut->startMultiItems();
                sal_Int64 nStartPos = mxStrm->tell();
                writeEmptyItem( "SUBREC" );
                sal_uInt16 nSubRecId = dumpDec< sal_uInt16 >( "id", "DXF-SUBREC" );
                sal_uInt16 nSubRecSize = dumpDec< sal_uInt16 >( "size" );
                sal_Int64 nEndPos = nStartPos + nSubRecSize;
                mxOut->endMultiItems();
                IndentGuard aIndGuard( mxOut );
                switch( nSubRecId )
                {
                    case 0:
                        dumpDec< sal_uInt8 >( "pattern", "FILLPATTERNS" );
                    break;
                    case 1: case 2: case 5:
                        dumpColor();
                    break;
                    case 3:
                        dumpGradientHead();
                    break;
                    case 4:
                        dumpDec< sal_uInt16 >( "index" );
                        dumpDec< double >( "stop-position" );
                        dumpColor( "stop-color" );
                    break;
                    case 6: case 7: case 8: case 9: case 10: case 11: case 12:
                        dumpColor( "color" );
                        dumpDec< sal_uInt16 >( "style", "BORDERSTYLES" );
                    break;
                    case 13: case 14:
                        dumpBoolean( "value" );
                    break;
                    case 15:
                        dumpDec< sal_uInt8 >( "alignment", "XF-HORALIGN" );
                    break;
                    case 16:
                        dumpDec< sal_uInt8 >( "alignment", "XF-VERALIGN" );
                    break;
                    case 17:
                        dumpDec< sal_uInt8 >( "rotation", "TEXTROTATION" );
                    break;
                    case 18:
                        dumpDec< sal_uInt16 >( "indent" );
                    break;
                    case 19:
                        dumpDec< sal_uInt8 >( "text-dir", "XF-TEXTDIRECTION" );
                    break;
                    case 20: case 21: case 22:
                        dumpBoolean( "value" );
                    break;
                    case 24:
                        dumpString( "name", false, false );
                    break;
                    case 25:
                        dumpDec< sal_uInt16 >( "weight", "FONT-WEIGHT" );
                    break;
                    case 26:
                        dumpDec< sal_uInt16 >( "underline", "FONT-UNDERLINE" );
                    break;
                    case 27:
                        dumpDec< sal_uInt16 >( "escapement", "FONT-ESCAPEMENT" );
                    break;
                    case 28: case 29: case 30: case 31: case 32: case 33:
                        dumpBoolean( "value" );
                    break;
                    case 34:
                        dumpDec< sal_uInt8 >( "charset", "CHARSET" );
                    break;
                    case 35:
                        dumpDec< sal_uInt8 >( "family", "FONT-FAMILY" );
                    break;
                    case 36:
                        dumpDec< sal_Int32 >( "height", "CONV-TWIP-TO-PT" );
                    break;
                    case 37:
                        dumpDec< sal_uInt8 >( "scheme", "FONT-SCHEME" );
                    break;
                    case 38:
                        dumpString( "numfmt", false, false );
                    break;
                    case 41:
                        dumpDec< sal_uInt16 >( "numfmt-id" );
                    break;
                    case 42:
                        dumpDec< sal_Int16 >( "relative-indent" );
                    break;
                    case 43: case 44:
                        dumpBoolean( "value" );
                    break;
                }
                dumpRemainingTo( nEndPos );
            }
        break;

        case BIFF12_ID_EXTCELL_BOOL:
            dumpColIndex();
            dumpBoolean();
        break;

        case BIFF12_ID_EXTCELL_DOUBLE:
            dumpColIndex();
            dumpDec< double >( "value" );
        break;

        case BIFF12_ID_EXTCELL_ERROR:
            dumpColIndex();
            dumpErrorCode();
        break;

        case BIFF12_ID_EXTCELL_STRING:
            dumpColIndex();
            dumpString( "value" );
        break;

        case BIFF12_ID_EXTERNALBOOK:
            switch( dumpDec< sal_uInt16 >( "type", "EXTERNALBOOK-TYPE" ) )
            {
                case 0:
                    dumpString( "rel-id" );
                    dumpDec< sal_Int32 >( "unused" );
                break;
                case 1:
                    dumpString( "dde-service" );
                    dumpString( "dde-topic" );
                break;
                case 2:
                    dumpString( "rel-id" );
                    dumpString( "prog-id" );
                break;
            }
        break;

        case BIFF12_ID_EXTERNALNAME:
            dumpString( "name" );
        break;

        case BIFF12_ID_EXTERNALNAMEFLAGS:
            dumpHex< sal_uInt16 >( "flags", "EXTERNALNAMEFLAGS-FLAGS" );
            dumpDec< sal_Int32 >( "sheet-id" );
            dumpBoolean( "is-dde-ole" );
        break;

        case BIFF12_ID_EXTERNALREF:
            dumpString( "rel-id" );
        break;

        case BIFF12_ID_EXTERNALSHEETS:
        {
            sal_Int32 nCount = dumpDec< sal_Int32 >( "ref-count" );
            TableGuard aTabGuard( mxOut, 13, 17, 24 );
            mxOut->resetItemIndex();
            for( sal_Int32 nRefId = 0; !mxStrm->isEof() && (nRefId < nCount); ++nRefId )
            {
                MultiItemsGuard aMultiGuard( mxOut );
                writeEmptyItem( "#ref" );
                dumpDec< sal_Int32 >( "extref-id" );
                dumpDec< sal_Int32 >( "first-sheet", "EXTERNALSHEETS-ID" );
                dumpDec< sal_Int32 >( "last-sheet", "EXTERNALSHEETS-ID" );
            }
        }
        break;

        case BIFF12_ID_EXTROW:
            dumpRowIndex();
        break;

        case BIFF12_ID_EXTSHEETDATA:
            dumpDec< sal_Int32 >( "sheet-id" );
            dumpHex< sal_uInt8 >( "flags", "EXTSHEETDATA-FLAGS" );
        break;

        case BIFF12_ID_EXTSHEETNAMES:
            mxOut->resetItemIndex();
            for( sal_Int32 nSheet = 0, nCount = dumpDec< sal_Int32 >( "sheet-count" ); !mxStrm->isEof() && (nSheet < nCount); ++nSheet )
                dumpString( "#sheet-name" );
        break;

        case BIFF12_ID_FILESHARING:
            dumpBool< sal_uInt16 >( "recommend-read-only" );
            dumpHex< sal_uInt16 >( "password-hash" );
            dumpString( "password-creator" );
        break;

        case BIFF12_ID_FILL:
            dumpDec< sal_Int32 >( "fill-pattern", "FILLPATTERNS" );
            dumpColor( "fg-color" );
            dumpColor( "bg-color" );
            dumpGradientHead();
            mxOut->resetItemIndex();
            for( sal_Int32 nStop = 0, nStopCount = dumpDec< sal_Int32 >( "stop-count" ); (nStop < nStopCount) && !mxStrm->isEof(); ++nStop )
            {
                writeEmptyItem( "#stop" );
                IndentGuard aIndGuard( mxOut );
                dumpColor( "stop-color" );
                dumpDec< double >( "stop-position" );
            }
        break;

        case BIFF12_ID_FILEVERSION:
            dumpGuid( "codename" );
            dumpString( "app-name" );
            dumpString( "last-edited" );
            dumpString( "lowest-edited" );
            dumpString( "build-version" );
        break;

        case BIFF12_ID_FILTERCOLUMN:
            dumpDec< sal_Int32 >( "column-index" );
            dumpHex< sal_uInt16 >( "flags", "FILTERCOLUMN-FLAGS" );
        break;

        case BIFF12_ID_FONT:
            dumpDec< sal_uInt16 >( "height", "CONV-TWIP-TO-PT" );
            dumpHex< sal_uInt16 >( "flags", "FONT-FLAGS" );
            dumpDec< sal_uInt16 >( "weight", "FONT-WEIGHT" );
            dumpDec< sal_uInt16 >( "escapement", "FONT-ESCAPEMENT" );
            dumpDec< sal_uInt8 >( "underline", "FONT-UNDERLINE" );
            dumpDec< sal_uInt8 >( "family", "FONT-FAMILY" );
            dumpDec< sal_uInt8 >( "charset", "CHARSET" );
            dumpUnused( 1 );
            dumpColor();
            dumpDec< sal_uInt8 >( "scheme", "FONT-SCHEME" );
            dumpString( "name" );
        break;

        case BIFF12_ID_FORMULA_BOOL:
            dumpCellHeader( true );
            dumpBoolean();
            dumpHex< sal_uInt16 >( "flags", "FORMULA-FLAGS" );
            mxFmlaObj->dumpCellFormula();
        break;

        case BIFF12_ID_FORMULA_DOUBLE:
            dumpCellHeader( true );
            dumpDec< double >( "value" );
            dumpHex< sal_uInt16 >( "flags", "FORMULA-FLAGS" );
            mxFmlaObj->dumpCellFormula();
        break;

        case BIFF12_ID_FORMULA_ERROR:
            dumpCellHeader( true );
            dumpErrorCode();
            dumpHex< sal_uInt16 >( "flags", "FORMULA-FLAGS" );
            mxFmlaObj->dumpCellFormula();
        break;

        case BIFF12_ID_FORMULA_STRING:
            dumpCellHeader( true );
            dumpString( "value" );
            dumpHex< sal_uInt16 >( "flags", "FORMULA-FLAGS" );
            mxFmlaObj->dumpCellFormula();
        break;

        case BIFF12_ID_FUNCTIONGROUP:
            dumpString( "name" );
        break;

        case BIFF12_ID_HEADERFOOTER:
            dumpHex< sal_uInt16 >( "flags", "HEADERFOOTER-FLAGS" );
            dumpString( "odd-header" );
            dumpString( "odd-footer" );
            dumpString( "even-header" );
            dumpString( "even-footer" );
            dumpString( "first-header" );
            dumpString( "first-footer" );
        break;

        case BIFF12_ID_HYPERLINK:
            dumpRange();
            dumpString( "rel-id" );
            dumpString( "location" );
            dumpString( "tooltip" );
            dumpString( "display" );
        break;

        case BIFF12_ID_INPUTCELLS:
            dumpAddress( "pos" );
            dumpUnused( 8 );
            dumpDec< sal_uInt16 >( "numfmt-id" );
            dumpString( "value" );
        break;

        case BIFF12_ID_LEGACYDRAWING:
            dumpString( "rel-id" );
        break;

        case BIFF12_ID_MERGECELL:
            dumpRange();
        break;

        case BIFF12_ID_MULTCELL_BLANK:
            dumpCellHeader( false );
        break;

        case BIFF12_ID_MULTCELL_BOOL:
            dumpCellHeader( false );
            dumpBoolean();
        break;

        case BIFF12_ID_MULTCELL_DOUBLE:
            dumpCellHeader( false );
            dumpDec< double >( "value" );
        break;

        case BIFF12_ID_MULTCELL_ERROR:
            dumpCellHeader( false );
            dumpErrorCode();
        break;

        case BIFF12_ID_MULTCELL_RK:
            dumpCellHeader( false );
            dumpRk( "value" );
        break;

        case BIFF12_ID_MULTCELL_RSTRING:
            dumpCellHeader( false );
            dumpString( "value", true );
        break;

        case BIFF12_ID_MULTCELL_SI:
            dumpCellHeader( false );
            dumpDec< sal_Int32 >( "string-id" );
        break;

        case BIFF12_ID_MULTCELL_STRING:
            dumpCellHeader( false );
            dumpString( "value" );
        break;

        case BIFF12_ID_NUMFMT:
            dumpDec< sal_uInt16 >( "numfmt-id" );
            dumpString( "format" );
        break;

        case BIFF12_ID_OLEOBJECT:
        {
            dumpDec< sal_Int32 >( "aspect", "OLEOBJECT-ASPECT" );
            dumpDec< sal_Int32 >( "update", "OLEOBJECT-UPDATE" );
            dumpDec< sal_Int32 >( "shape-id" );
            sal_uInt16 nFlags = dumpHex< sal_uInt16 >( "flags", "OLEOBJECT-FLAGS" );
            dumpString( "prog-id" );
            if( getFlag( nFlags, BIFF12_OLEOBJECT_LINKED ) )
                mxFmlaObj->dumpNameFormula( "link" );
            else
                dumpString( "rel-id" );
        }
        break;

        case BIFF12_ID_OLESIZE:
            dumpRange( "visible-range" );
        break;

        case BIFF12_ID_PAGEMARGINS:
            dumpDec< double >( "left-margin" );
            dumpDec< double >( "right-margin" );
            dumpDec< double >( "top-margin" );
            dumpDec< double >( "bottom-margin" );
            dumpDec< double >( "header-margin" );
            dumpDec< double >( "footer-margin" );
        break;

        case BIFF12_ID_PAGESETUP:
            dumpDec< sal_Int32 >( "paper-size", "PAGESETUP-PAPERSIZE" );
            dumpDec< sal_Int32 >( "scaling", "CONV-PERCENT" );
            dumpDec< sal_Int32 >( "horizontal-res", "PAGESETUP-DPI" );
            dumpDec< sal_Int32 >( "vertical-res", "PAGESETUP-DPI" );
            dumpDec< sal_Int32 >( "copies" );
            dumpDec< sal_Int32 >( "first-page" );
            dumpDec< sal_Int32 >( "scale-to-width", "PAGESETUP-SCALETOPAGES" );
            dumpDec< sal_Int32 >( "scale-to-height", "PAGESETUP-SCALETOPAGES" );
            dumpHex< sal_uInt16 >( "flags", "PAGESETUP-FLAGS" );
            dumpString( "printer-settings-rel-id" );
        break;

        case BIFF12_ID_PANE:
            dumpDec< double >( "x-split-pos" );
            dumpDec< double >( "y-split-pos" );
            dumpAddress( "second-top-left" );
            dumpDec< sal_Int32 >( "active-pane", "PANE-ID" );
            dumpHex< sal_uInt8 >( "flags", "PANE-FLAGS" );
        break;

        case BIFF12_ID_PCDEFINITION:
        {
            dumpDec< sal_uInt8 >( "refreshed-version" );
            dumpDec< sal_uInt8 >( "min-refresh-version" );
            dumpDec< sal_uInt8 >( "created-version" );
            dumpHex< sal_uInt8 >( "flags-1", "PCDEFINITION-FLAGS1" );
            dumpDec< sal_Int32 >( "missing-items-limit", "PCDEFINITION-MISSINGITEMS" );
            dumpDec< double >( "refreshed-date" );
            sal_uInt8 nFlags2 = dumpHex< sal_uInt8 >( "flags-2", "PCDEFINITION-FLAGS2" );
            dumpDec< sal_Int32 >( "record-count" );
            if( nFlags2 & 0x01 ) dumpString( "refreshed-by" );
            if( nFlags2 & 0x02 ) dumpString( "rel-id" );
        }
        break;

        case BIFF12_ID_PCDFIELD:
        {
            sal_uInt16 nFlags = dumpHex< sal_uInt16 >( "flags", "PCDFIELD-FLAGS" );
            dumpDec< sal_Int32 >( "numfmt-id" );
            dumpDec< sal_Int16 >( "sql-datatype" );
            dumpDec< sal_Int32 >( "hierarchy" );
            dumpDec< sal_Int32 >( "hierarchy-level" );
            sal_Int32 nMappingCount = dumpDec< sal_Int32 >( "property-mapping-count" );
            dumpString( "name" );
            if( nFlags & 0x0008 ) dumpString( "caption" );
            if( nFlags & 0x0100 ) mxFmlaObj->dumpNameFormula( "formula" );
            if( nMappingCount > 0 )
            {
                sal_Int32 nBytes = dumpDec< sal_Int32 >( "property-mapping-size" );
                dumpArray( "property-mapping-indexes", nBytes );
            }
            if( nFlags & 0x0200 ) dumpString( "property-name" );
        }
        break;

        case BIFF12_ID_PCDFIELDGROUP:
            dumpDec< sal_Int32 >( "parent-field" );
            dumpDec< sal_Int32 >( "base-field" );
        break;

        case BIFF12_ID_PCDFRANGEPR:
            dumpDec< sal_uInt8 >( "group-by", "PCDFRANGEPR-GROUPBY" );
            dumpHex< sal_uInt8 >( "flags", "PCDFRANGEPR-FLAGS" );
            dumpDec< double >( "start-value" );
            dumpDec< double >( "end-value" );
            dumpDec< double >( "interval" );
        break;

        case BIFF12_ID_PCDFSHAREDITEMS:
        {
            sal_uInt16 nFlags = dumpHex< sal_uInt16 >( "flags", "PCDFSHAREDITEMS-FLAGS" );
            dumpDec< sal_Int32 >( "count" );
            if( nFlags & 0x0100 ) dumpDec< double >( "min-value" );
            if( nFlags & 0x0100 ) dumpDec< double >( "max-value" );
        }
        break;

        case BIFF12_ID_PCDSHEETSOURCE:
        {
            sal_uInt8 nIsDefName = dumpBoolean( "is-def-name" );
            dumpBoolean( "is-builtin-def-name" );
            sal_uInt8 nFlags = dumpHex< sal_uInt8 >( "flags", "PCDWORKSHEETSOURCE-FLAGS" );
            if( nFlags & 0x02 ) dumpString( "sheet-name" );
            if( nFlags & 0x01 ) dumpString( "rel-id" );
            if( nIsDefName == 0 ) dumpRange(); else dumpString( "def-name" );
        }
        break;

        case BIFF12_ID_PCDSOURCE:
            dumpDec< sal_Int32 >( "source-type", "PCDSOURCE-TYPE" );
            dumpDec< sal_Int32 >( "connection-id" );
        break;

        case BIFF12_ID_PCITEM_ARRAY:
        {
            sal_uInt16 nType = dumpDec< sal_uInt16 >( "type", "PCITEM_ARRAY-TYPE" );
            sal_Int32 nCount = dumpDec< sal_Int32 >( "count" );
            mxOut->resetItemIndex();
            for( sal_Int32 nIdx = 0; nIdx < nCount; ++nIdx )
            {
                switch( nType )
                {
                    case 1:     dumpDec< double >( "#value" );  break;
                    case 2:     dumpString( "#value" );         break;
                    case 16:    dumpErrorCode( "#value" );      break;
                    case 32:    dumpPivotDateTime( "#value" );  break;
                    default:    nIdx = nCount;
                }
            }
        }
        break;

        case BIFF12_ID_PCITEM_BOOL:
            dumpBoolean( "value" );
        break;

        case BIFF12_ID_PCITEM_DATE:
            dumpPivotDateTime( "value" );
        break;

        case BIFF12_ID_PCITEM_DOUBLE:
            dumpDec< double >( "value" );
            // TODO: server formatting
        break;

        case BIFF12_ID_PCITEM_ERROR:
            dumpErrorCode( "value" );
            // TODO: server formatting
        break;

        case BIFF12_ID_PCITEM_INDEX:
            dumpDec< sal_Int32 >( "index" );
        break;

        case BIFF12_ID_PCITEM_MISSING:
            // TODO: server formatting
        break;


        case BIFF12_ID_PCITEM_STRING:
            dumpString( "value" );
            // TODO: server formatting
        break;

        case BIFF12_ID_PCITEMA_BOOL:
            dumpBoolean( "value" );
            // TODO: additional info
        break;

        case BIFF12_ID_PCITEMA_DATE:
            dumpPivotDateTime( "value" );
            // TODO: additional info
        break;

        case BIFF12_ID_PCITEMA_DOUBLE:
            dumpDec< double >( "value" );
            // TODO: additional info
        break;

        case BIFF12_ID_PCITEMA_ERROR:
            dumpErrorCode( "value" );
            // TODO: additional info
        break;

        case BIFF12_ID_PCITEMA_MISSING:
            // TODO: additional info
        break;

        case BIFF12_ID_PCITEMA_STRING:
            dumpString( "value" );
            // TODO: additional info
        break;

        case BIFF12_ID_PHONETICPR:
            dumpDec< sal_uInt16 >( "font-id", "FONTNAMES" );
            dumpDec< sal_Int32 >( "type", "PHONETICPR-TYPE" );
            dumpDec< sal_Int32 >( "alignment", "PHONETICPR-ALIGNMENT" );
        break;

        case BIFF12_ID_PICTURE:
            dumpString( "rel-id" );
        break;

        case BIFF12_ID_PIVOTAREA:
            dumpDec< sal_Int32 >( "field" );
            dumpDec< sal_uInt8 >( "type", "PIVOTAREA-TYPE" );
            dumpHex< sal_uInt8 >( "flags-1", "PIVOTAREA-FLAGS1" );
            dumpHex< sal_uInt16 >( "flags-2", "PIVOTAREA-FLAGS2" );
        break;

        case BIFF12_ID_PIVOTCACHE:
            dumpDec< sal_Int32 >( "cache-id" );
            dumpString( "rel-id" );
        break;

        case BIFF12_ID_PTCOLFIELDS:
            dumpDec< sal_Int32 >( "count" );
            mxOut->resetItemIndex();
            while( mxStrm->getRemaining() >= 4 )
                dumpDec< sal_Int32 >( "#field", "PT-FIELDINDEX" );
        break;

        case BIFF12_ID_PTDATAFIELD:
            dumpDec< sal_Int32 >( "field" );
            dumpDec< sal_Int32 >( "subtotal", "PTDATAFIELD-SUBTOTAL" );
            dumpDec< sal_Int32 >( "show-data-as", "PTDATAFIELD-SHOWDATAAS" );
            dumpDec< sal_Int32 >( "base-field" );
            dumpDec< sal_Int32 >( "base-item", "PTDATAFIELD-BASEITEM" );
            dumpDec< sal_Int32 >( "number-format" );
            if( dumpBool< sal_uInt8 >( "has-name" ) )
                dumpString( "name" );
        break;

        case BIFF12_ID_PTDEFINITION:
        {
            dumpDec< sal_uInt8 >( "created-version" );
            dumpHex< sal_uInt8 >( "flags-1", "PTDEFINITION-FLAGS1" );
            dumpHex< sal_uInt16 >( "flags-2", "PTDEFINITION-FLAGS2" );
            sal_uInt32 nFlags3 = dumpHex< sal_uInt32 >( "flags-3", "PTDEFINITION-FLAGS3" );
            sal_uInt32 nFlags4 = dumpHex< sal_uInt32 >( "flags-4", "PTDEFINITION-FLAGS4" );
            dumpDec< sal_uInt8 >( "datafield-axis", "PTDEFINITION-DATAFIELD-AXIS" );
            dumpDec< sal_uInt8 >( "page-wrap" );
            dumpDec< sal_uInt8 >( "refreshed-version" );
            dumpDec< sal_uInt8 >( "min-refresh-version" );
            dumpDec< sal_Int32 >( "datafield-position", "PTDEFINITION-DATAFIELD-POS" );
            dumpDec< sal_Int16 >( "autoformat-id" );
            dumpUnused( 2 );
            dumpDec< sal_Int32 >( "next-chart-id" );
            dumpDec< sal_Int32 >( "cache-id" );
            dumpString( "name" );
            if( nFlags3 & 0x00080000 ) dumpString( "data-caption" );
            if( nFlags3 & 0x00100000 ) dumpString( "grand-total-caption" );
            if( (nFlags4 & 0x00000040) == 0 ) dumpString( "error-caption" );
            if( (nFlags4 & 0x00000080) == 0 ) dumpString( "missing-caption" );
            if( nFlags3 & 0x00200000 ) dumpString( "page-field-style" );
            if( nFlags3 & 0x00400000 ) dumpString( "pivot-table-style" );
            if( nFlags3 & 0x00800000 ) dumpString( "vacated-style" );
            if( nFlags3 & 0x40000000 ) dumpString( "tag" );
            if( nFlags4 & 0x00000800 ) dumpString( "col-header-caption" );
            if( nFlags4 & 0x00000400 ) dumpString( "row-header-caption" );
        }
        break;

        case BIFF12_ID_PTFIELD:
            dumpHex< sal_uInt32 >( "flags-1", "PTFIELD-FLAGS1" );
            dumpDec< sal_Int32 >( "num-fmt" );
            dumpHex< sal_uInt32 >( "flags-2", "PTFIELD-FLAGS2" );
            dumpDec< sal_Int32 >( "autoshow-items" );
            dumpDec< sal_Int32 >( "autoshow-datafield-idx" );
        break;

        case BIFF12_ID_PTFILTER:
        {
            dumpDec< sal_Int32 >( "field" );
            dumpDec< sal_Int32 >( "member-prop-field" );
            dumpDec< sal_Int32 >( "type", "PTFILTER-TYPE" );
            dumpUnused( 4 );
            dumpDec< sal_Int32 >( "unique-id" );
            dumpDec< sal_Int32 >( "measure-data-field" );
            dumpDec< sal_Int32 >( "measure-data-hierarchy" );
            sal_uInt16 nFlags = dumpHex< sal_uInt16 >( "flags", "PTFILTER-FLAGS" );
            if( nFlags & 0x0001 ) dumpString( "name" );
            if( nFlags & 0x0002 ) dumpString( "description" );
            if( nFlags & 0x0004 ) dumpString( "str-value1" );
            if( nFlags & 0x0008 ) dumpString( "str-value2" );
        }
        break;

        case BIFF12_ID_PTFITEM:
        {
            dumpDec< sal_uInt8 >( "type", "PTFITEM-TYPE" );
            sal_uInt16 nFlags = dumpHex< sal_uInt16 >( "flags", "PTFITEM-FLAGS" );
            dumpDec< sal_Int32 >( "cache-idx" );
            if( nFlags & 0x0010 ) dumpString( "display-name" );
        }
        break;

        case BIFF12_ID_PTLOCATION:
            dumpRange( "location" );
            dumpDec< sal_Int32 >( "first-header-row" );
            dumpDec< sal_Int32 >( "first-data-row" );
            dumpDec< sal_Int32 >( "first-data-col" );
            dumpDec< sal_Int32 >( "page-row-count" );
            dumpDec< sal_Int32 >( "page-col-count" );
        break;

        case BIFF12_ID_PTPAGEFIELD:
        {
            dumpDec< sal_Int32 >( "field" );
            dumpDec< sal_Int32 >( "cache-item", "PTPAGEFIELD-ITEM" );
            dumpDec< sal_Int32 >( "olap-hierarchy" );
            sal_uInt8 nFlags = dumpHex< sal_uInt8 >( "flags", "PTPAGEFIELD-FLAGS" );
            if( nFlags & 0x01 ) dumpString( "unique-name" );
            if( nFlags & 0x02 ) dumpString( "olap-caption" );
        }
        break;

        case BIFF12_ID_PTREFERENCE:
            dumpDec< sal_Int32 >( "field", "PT-FIELDINDEX" );
            dumpDec< sal_Int32 >( "item-count" );
            dumpHex< sal_uInt16 >( "flags-1", "PTREFERENCE-FLAGS1" );
            dumpHex< sal_uInt8 >( "flags-2", "PTREFERENCE-FLAGS2" );
        break;

        case BIFF12_ID_PTROWFIELDS:
            dumpDec< sal_Int32 >( "count" );
            mxOut->resetItemIndex();
            while( mxStrm->getRemaining() >= 4 )
                dumpDec< sal_Int32 >( "#field", "PT-FIELDINDEX" );
        break;

        case BIFF12_ID_QUERYTABLE:
            dumpHex< sal_uInt32 >( "flags", "QUERYTABLE-FLAGS" );
            dumpDec< sal_uInt16 >( "autoformat-id" );
            dumpDec< sal_Int32 >( "connection-id" );
            dumpString( "defined-name" );
        break;
        
        case BIFF12_ID_ROW:
            dumpRowIndex();
            dumpDec< sal_Int32 >( "custom-xf-id" );
            dumpDec< sal_uInt16 >( "height", "CONV-TWIP-TO-PT" );
            dumpHex< sal_uInt16 >( "flags", "ROW-FLAGS1" );
            dumpHex< sal_uInt8 >( "flags", "ROW-FLAGS2" );
            mxOut->resetItemIndex();
            for( sal_Int32 nSpan = 0, nSpanCount = dumpDec< sal_Int32 >( "row-spans-count" ); !mxStrm->isEof() && (nSpan < nSpanCount); ++nSpan )
                dumpRowRange( "#row-spans" );
        break;

        case BIFF12_ID_ROWBREAKS:
            dumpDec< sal_Int32 >( "count" );
            dumpDec< sal_Int32 >( "manual-count" );
        break;

        case BIFF12_ID_SCENARIO:
            dumpDec< sal_uInt16 >( "cell-count" );
            // two longs instead of flag field
            dumpDec< sal_Int32 >( "locked", "BOOLEAN" );
            dumpDec< sal_Int32 >( "hidden", "BOOLEAN" );
            dumpString( "name" );
            dumpString( "comment" );
            dumpString( "user" );
        break;

        case BIFF12_ID_SCENARIOS:
            dumpDec< sal_uInt16 >( "selected" );
            dumpDec< sal_uInt16 >( "shown" );
            dumpRangeList( "result-cells" );
        break;

        case BIFF12_ID_SELECTION:
            dumpDec< sal_Int32 >( "pane", "PANE-ID" );
            dumpAddress( "active-cell" );
            dumpDec< sal_Int32 >( "active-cell-id" );
            dumpRangeList( "selection" );
        break;

        case BIFF12_ID_SHAREDFMLA:
            dumpRange( "formula-range" );
            mxFmlaObj->dumpCellFormula();
        break;

        case BIFF12_ID_SHEET:
            dumpDec< sal_Int32 >( "sheet-state", "SHEET-STATE" );
            dumpDec< sal_Int32 >( "sheet-id" );
            dumpString( "rel-id" );
            dumpString( "sheet-name" );
        break;

        case BIFF12_ID_SHEETFORMATPR:
            dumpDec< sal_Int32 >( "default-col-width", "CONV-COLWIDTH" );
            dumpDec< sal_uInt16 >( "base-col-width" );
            dumpDec< sal_uInt16 >( "default-row-height", "CONV-TWIP-TO-PT" );
            dumpHex< sal_uInt16 >( "flags", "SHEETFORMATPR-FLAGS" );
            dumpDec< sal_uInt8 >( "max-row-outline" );
            dumpDec< sal_uInt8 >( "max-col-outline" );
        break;

        case BIFF12_ID_SHEETPR:
            dumpHex< sal_uInt16 >( "flags1", "SHEETPR-FLAGS1" );
            dumpHex< sal_uInt8 >( "flags2", "SHEETPR-FLAGS2" );
            dumpColor( "tab-color" );
            dumpAddress( "window-anchor" );
            dumpString( "codename" );
        break;

        case BIFF12_ID_SHEETPROTECTION:
            dumpHex< sal_uInt16 >( "password-hash" );
            // no flags field for all these boolean flags?!?
            dumpDec< sal_Int32 >( "sheet-locked", "BOOLEAN" );
            dumpDec< sal_Int32 >( "objects-locked", "BOOLEAN" );
            dumpDec< sal_Int32 >( "scenarios-locked", "BOOLEAN" );
            dumpDec< sal_Int32 >( "format-cells-locked", "BOOLEAN" );
            dumpDec< sal_Int32 >( "format-columns-locked", "BOOLEAN" );
            dumpDec< sal_Int32 >( "format-rows-locked", "BOOLEAN" );
            dumpDec< sal_Int32 >( "insert-columns-locked", "BOOLEAN" );
            dumpDec< sal_Int32 >( "insert-rows-locked", "BOOLEAN" );
            dumpDec< sal_Int32 >( "insert-hyperlinks-locked", "BOOLEAN" );
            dumpDec< sal_Int32 >( "delete-columns-locked", "BOOLEAN" );
            dumpDec< sal_Int32 >( "delete-rows-locked", "BOOLEAN" );
            dumpDec< sal_Int32 >( "select-locked-cells-locked", "BOOLEAN" );
            dumpDec< sal_Int32 >( "sort-locked", "BOOLEAN" );
            dumpDec< sal_Int32 >( "autofilter-locked", "BOOLEAN" );
            dumpDec< sal_Int32 >( "pivot-tables-locked", "BOOLEAN" );
            dumpDec< sal_Int32 >( "select-unlocked-cells-locked", "BOOLEAN" );
        break;

        case BIFF12_ID_SHEETVIEW:
            dumpHex< sal_uInt16 >( "flags", "SHEETVIEW-FLAGS" );
            dumpDec< sal_Int32 >( "view-type", "SHEETVIEW-TYPE" );
            dumpAddress( "top-left" );
            dumpDec< sal_Int32 >( "gridcolor-id", "PALETTE-COLORS" );
            dumpDec< sal_uInt16 >( "zoom-scale", "CONV-PERCENT" );
            dumpDec< sal_uInt16 >( "zoom-scale-normal", "CONV-PERCENT" );
            dumpDec< sal_uInt16 >( "zoom-scale-sheet-layout", "CONV-PERCENT" );
            dumpDec< sal_uInt16 >( "zoom-scale-page-layout", "CONV-PERCENT" );
            dumpDec< sal_Int32 >( "workbookview-id" );
        break;

        case BIFF12_ID_SI:
            dumpString( "string", true );
        break;

        case BIFF12_ID_SST:
            dumpDec< sal_Int32 >( "string-cell-count" );
            dumpDec< sal_Int32 >( "sst-size" );
        break;

        case BIFF12_ID_TABLE:
            dumpRange();
            dumpDec< sal_Int32 >( "type", "TABLE-TYPE" );
            dumpDec< sal_Int32 >( "id" );
            dumpDec< sal_Int32 >( "header-rows" );
            dumpDec< sal_Int32 >( "totals-rows" );
            dumpHex< sal_uInt32 >( "flags", "TABLE-FLAGS" );
            dumpDec< sal_Int32 >( "headerrow-dxf-id" );
            dumpDec< sal_Int32 >( "data-dxf-id" );
            dumpDec< sal_Int32 >( "totalsrow-dxf-id" );
            dumpDec< sal_Int32 >( "table-border-dxf-id" );
            dumpDec< sal_Int32 >( "headerrow-border-dxf-id" );
            dumpDec< sal_Int32 >( "totalsrow-border-dxf-id" );
            dumpDec< sal_Int32 >( "connection-id" );
            dumpString( "name" );
            dumpString( "display-name" );
            dumpString( "comment" );
            dumpString( "headerrow-cell-style" );
            dumpString( "data-cell-style" );
            dumpString( "totalsrow-cell-style" );
        break;

        case BIFF12_ID_TABLEPART:
            dumpString( "rel-id" );
        break;

        case BIFF12_ID_TABLESTYLEINFO:
            dumpHex< sal_uInt16 >( "flags", "TABLESTYLEINFO-FLAGS" );
            dumpString( "style-name" );
        break;

        case BIFF12_ID_TOP10FILTER:
            dumpHex< sal_uInt8 >( "flags", "TOP10FILTER-FLAGS" );
            dumpDec< double >( "value" );
            dumpDec< double >( "cell-value" );
        break;

        case BIFF12_ID_VOLTYPEMAIN:
            dumpString( "first" );
        break;

        case BIFF12_ID_VOLTYPESTP:
            dumpString( "topic-value" );
        break;

        case BIFF12_ID_VOLTYPETR:
            dumpAddress( "ref" );
            dumpDec< sal_Int32 >( "sheet-id" );
        break;

        case BIFF12_ID_WEBPR:
        {
            dumpHex< sal_uInt32 >( "flags", "WEBPR-FLAGS" );
            sal_uInt8 nStrFlags = dumpHex< sal_uInt8 >( "string-flags", "WEBPR-STRINGFLAGS" );
            if( nStrFlags & 0x04 ) dumpString( "url" );
            if( nStrFlags & 0x01 ) dumpString( "post-method" );
            if( nStrFlags & 0x02 ) dumpString( "edit-page" );
        }
        break;
        
        case BIFF12_ID_WORKBOOKPR:
            dumpHex< sal_uInt32 >( "flags", "WORKBBOKPR-FLAGS" );
            dumpDec< sal_Int32 >( "default-theme-version" );
            dumpString( "codename" );
        break;

        case BIFF12_ID_WORKBOOKVIEW:
            dumpDec< sal_Int32 >( "x-window" );
            dumpDec< sal_Int32 >( "y-window" );
            dumpDec< sal_Int32 >( "win-width" );
            dumpDec< sal_Int32 >( "win-height" );
            dumpDec< sal_Int32 >( "tabbar-ratio" );
            dumpDec< sal_Int32 >( "first-sheet" );
            dumpDec< sal_Int32 >( "active-sheet" );
            dumpHex< sal_uInt8 >( "flags", "WORKBOOKVIEW-FLAGS" );
        break;

        case BIFF12_ID_XF:
            dumpDec< sal_uInt16 >( "parent-xf-id" );
            dumpDec< sal_uInt16 >( "numfmt-id" );
            dumpDec< sal_uInt16 >( "font-id", "FONTNAMES" );
            dumpDec< sal_uInt16 >( "fill-id" );
            dumpDec< sal_uInt16 >( "border-id" );
            dumpHex< sal_uInt32 >( "alignment", "XF-ALIGNMENT" );
            dumpHex< sal_uInt16 >( "used-flags", "XF-USEDFLAGS" );
        break;
    }
}

void RecordStreamObject::dumpGradientHead()
{
    dumpDec< sal_Int32 >( "gradient-type", "FILL-GRADIENTTYPE" );
    dumpDec< double >( "linear-angle" );
    dumpDec< double >( "pos-left" );
    dumpDec< double >( "pos-right" );
    dumpDec< double >( "pos-top" );
    dumpDec< double >( "pos-bottom" );
}

void RecordStreamObject::dumpCellHeader( bool bWithColumn )
{
    if( bWithColumn ) dumpColIndex();
    dumpHex< sal_uInt32 >( "xf-id", "CELL-XFID" );
}

// ============================================================================

RootStorageObject::RootStorageObject( const DumperBase& rParent )
{
    StorageObjectBase::construct( rParent );
}

void RootStorageObject::implDumpStream( const Reference< XInputStream >& rxStrm, const OUString& rStrgPath, const OUString& rStrmName, const OUString& rSysFileName )
{
    OUString aExt = InputOutputHelper::getFileNameExtension( rStrmName );
    if(
        aExt.equalsIgnoreAsciiCaseAscii( "xlsb" ) ||
        aExt.equalsIgnoreAsciiCaseAscii( "xlsm" ) ||
        aExt.equalsIgnoreAsciiCaseAscii( "xlsx" ) ||
        aExt.equalsIgnoreAsciiCaseAscii( "xltm" ) ||
        aExt.equalsIgnoreAsciiCaseAscii( "xltx" ) )
    {
        Dumper( getContext(), rxStrm, rSysFileName ).dump();
    }
    else if(
        aExt.equalsIgnoreAsciiCaseAscii( "xla" ) ||
        aExt.equalsIgnoreAsciiCaseAscii( "xlc" ) ||
        aExt.equalsIgnoreAsciiCaseAscii( "xlm" ) ||
        aExt.equalsIgnoreAsciiCaseAscii( "xls" ) ||
        aExt.equalsIgnoreAsciiCaseAscii( "xlt" ) ||
        aExt.equalsIgnoreAsciiCaseAscii( "xlw" ) )
    {
        ::oox::dump::biff::Dumper( getContext(), rxStrm, rSysFileName ).dump();
    }
    else if(
        aExt.equalsIgnoreAsciiCaseAscii( "pptx" ) ||
        aExt.equalsIgnoreAsciiCaseAscii( "potx" ) )
    {
        ::oox::dump::pptx::Dumper( getContext(), rxStrm, rSysFileName ).dump();
    }
    else if(
        aExt.equalsIgnoreAsciiCaseAscii( "xml" ) ||
        aExt.equalsIgnoreAsciiCaseAscii( "vml" ) ||
        aExt.equalsIgnoreAsciiCaseAscii( "rels" ) )
    {
        XmlStreamObject( *this, rxStrm, rSysFileName ).dump();
    }
    else if( aExt.equalsIgnoreAsciiCaseAscii( "bin" ) )
    {
        if( rStrgPath.equalsAscii( "xl" ) && rStrmName.equalsAscii( "vbaProject.bin" ) )
        {
            StorageRef xStrg( new ::oox::ole::OleStorage( getContext(), rxStrm, false ) );
            VbaProjectStorageObject( *this, xStrg, rSysFileName ).dump();
        }
        else if( rStrgPath.equalsAscii( "xl/embeddings" ) )
        {
            StorageRef xStrg( new ::oox::ole::OleStorage( getContext(), rxStrm, false ) );
            OleStorageObject( *this, xStrg, rSysFileName ).dump();
        }
        else if(
            rStrgPath.equalsAscii( "xl" ) ||
            rStrgPath.equalsAscii( "xl/chartsheets" ) ||
            rStrgPath.equalsAscii( "xl/dialogsheets" ) ||
            rStrgPath.equalsAscii( "xl/externalLinks" ) ||
            rStrgPath.equalsAscii( "xl/macrosheets" ) ||
            rStrgPath.equalsAscii( "xl/pivotCache" ) ||
            rStrgPath.equalsAscii( "xl/pivotTables" ) ||
            rStrgPath.equalsAscii( "xl/queryTables" ) ||
            rStrgPath.equalsAscii( "xl/tables" ) ||
            rStrgPath.equalsAscii( "xl/worksheets" ) )
        {
            RecordStreamObject( *this, rxStrm, rSysFileName ).dump();
        }
        else if( rStrgPath.equalsAscii( "xl/activeX" ) )
        {
            StorageRef xStrg( new ::oox::ole::OleStorage( getContext(), rxStrm, true ) );
            ActiveXStorageObject( *this, xStrg, rSysFileName ).dump();
        }
        else
        {
            BinaryStreamObject( *this, rxStrm, rSysFileName ).dump();
        }
    }
}

// ============================================================================

#define DUMP_XLSB_CONFIG_ENVVAR "OOO_XLSBDUMPER"

Dumper::Dumper( const FilterBase& rFilter )
{
    ConfigRef xCfg( new Config( DUMP_XLSB_CONFIG_ENVVAR, rFilter ) );
    DumperBase::construct( xCfg );
}

Dumper::Dumper( const Reference< XComponentContext >& rxContext, const Reference< XInputStream >& rxInStrm, const OUString& rSysFileName )
{
    if( rxContext.is() && rxInStrm.is() )
    {
        StorageRef xStrg( new ZipStorage( getContext(), rxInStrm ) );
        MediaDescriptor aMediaDesc;
        ConfigRef xCfg( new Config( DUMP_XLSB_CONFIG_ENVVAR, rxContext, xStrg, rSysFileName, aMediaDesc ) );
        DumperBase::construct( xCfg );
    }
}

void Dumper::implDump()
{
    RootStorageObject( *this ).dump();
}

// ============================================================================

} // namespace xlsb
} // namespace dump
} // namespace oox

#endif
