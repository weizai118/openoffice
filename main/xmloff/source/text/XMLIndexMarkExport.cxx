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



// MARKER(update_precomp.py): autogen include statement, do not remove
#include "precompiled_xmloff.hxx"
#include "XMLIndexMarkExport.hxx"
#include <tools/debug.hxx>
#include <rtl/ustring.hxx>
#include <rtl/ustrbuf.hxx>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/beans/XPropertySetInfo.hpp>
#include <xmloff/xmltoken.hxx>
#include "xmloff/xmlnmspe.hxx"
#include <xmloff/xmlexp.hxx>
#include <xmloff/xmluconv.hxx>


using namespace ::xmloff::token;

using ::rtl::OUString;
using ::rtl::OUStringBuffer;
using ::com::sun::star::beans::XPropertySet;
using ::com::sun::star::beans::XPropertySetInfo;
using ::com::sun::star::uno::Reference;
using ::com::sun::star::uno::Any;



XMLIndexMarkExport::XMLIndexMarkExport(
	SvXMLExport& rExp,
	XMLTextParagraphExport& rParaExp)
:	sLevel(RTL_CONSTASCII_USTRINGPARAM("Level"))
,	sUserIndexName(RTL_CONSTASCII_USTRINGPARAM("UserIndexName"))
,	sPrimaryKey(RTL_CONSTASCII_USTRINGPARAM("PrimaryKey"))
,	sSecondaryKey(RTL_CONSTASCII_USTRINGPARAM("SecondaryKey"))
,	sDocumentIndexMark(RTL_CONSTASCII_USTRINGPARAM("DocumentIndexMark"))
,	sIsStart(RTL_CONSTASCII_USTRINGPARAM("IsStart"))
,	sIsCollapsed(RTL_CONSTASCII_USTRINGPARAM("IsCollapsed"))
,	sAlternativeText(RTL_CONSTASCII_USTRINGPARAM("AlternativeText"))
,	sTextReading(RTL_CONSTASCII_USTRINGPARAM("TextReading"))
,	sPrimaryKeyReading(RTL_CONSTASCII_USTRINGPARAM("PrimaryKeyReading"))
,	sSecondaryKeyReading(RTL_CONSTASCII_USTRINGPARAM("SecondaryKeyReading"))
,	sMainEntry(RTL_CONSTASCII_USTRINGPARAM("IsMainEntry"))
,	rExport(rExp)
,	rParaExport(rParaExp)
{
}

const enum XMLTokenEnum lcl_pTocMarkNames[] = 
	{ XML_TOC_MARK, XML_TOC_MARK_START, XML_TOC_MARK_END };
const enum XMLTokenEnum lcl_pUserIndexMarkName[] =
	{ XML_USER_INDEX_MARK, 
		  XML_USER_INDEX_MARK_START, XML_USER_INDEX_MARK_END };
const enum XMLTokenEnum lcl_pAlphaIndexMarkName[] = 
	{ XML_ALPHABETICAL_INDEX_MARK, 
		  XML_ALPHABETICAL_INDEX_MARK_START, 
		  XML_ALPHABETICAL_INDEX_MARK_END };


XMLIndexMarkExport::~XMLIndexMarkExport()
{
}

void XMLIndexMarkExport::ExportIndexMark(
	const Reference<XPropertySet> & rPropSet,
	sal_Bool bAutoStyles)
{
	/// index marks have no styles!
	if (!bAutoStyles)
	{
		const enum XMLTokenEnum * pElements = NULL;
		sal_Int8 nElementNo = -1;

		// get index mark
		Any aAny;
		aAny = rPropSet->getPropertyValue(sDocumentIndexMark);
		Reference<XPropertySet> xIndexMarkPropSet;
		aAny >>= xIndexMarkPropSet;

		// common: handling of start, end, collapsed entries and
		// alternative text

		// collapsed/alternative text entry?
		aAny = rPropSet->getPropertyValue(sIsCollapsed);
		if (*(sal_Bool *)aAny.getValue())
		{
			// collapsed entry: needs alternative text
			nElementNo = 0;

			aAny = xIndexMarkPropSet->getPropertyValue(sAlternativeText);
			OUString sTmp;
			aAny >>= sTmp;
			DBG_ASSERT(sTmp.getLength() > 0, 
					   "collapsed index mark without alternative text");
			rExport.AddAttribute(XML_NAMESPACE_TEXT, XML_STRING_VALUE, sTmp);
		}
		else
		{
			// start and end entries: has ID
			aAny = rPropSet->getPropertyValue(sIsStart);
			nElementNo = *(sal_Bool *)aAny.getValue() ? 1 : 2;

			// generate ID
			OUStringBuffer sBuf;
			GetID(sBuf, xIndexMarkPropSet);
			rExport.AddAttribute(XML_NAMESPACE_TEXT, XML_ID, 
                                 sBuf.makeStringAndClear());
		}

		// distinguish between TOC, user, alphab. index marks by
		// asking for specific properties
		// Export attributes for -mark-start and -mark elements, 
		// but not for -mark-end
		Reference<XPropertySetInfo> xPropertySetInfo =
			xIndexMarkPropSet->getPropertySetInfo();
		if (xPropertySetInfo->hasPropertyByName(sUserIndexName))
		{
			// user index mark
			pElements = lcl_pUserIndexMarkName;
			if (nElementNo != 2)
			{
				ExportUserIndexMarkAttributes(xIndexMarkPropSet);
			}
		}
		else if (xPropertySetInfo->hasPropertyByName(sPrimaryKey))
		{
			// alphabetical index mark
			pElements = lcl_pAlphaIndexMarkName;
			if (nElementNo != 2)
			{
				ExportAlphabeticalIndexMarkAttributes(xIndexMarkPropSet);
			}
		}
		else
		{
			// table of content:
			pElements = lcl_pTocMarkNames;
			if (nElementNo != 2)
			{
				ExportTOCMarkAttributes(xIndexMarkPropSet);
			}
		}

		// export element
		DBG_ASSERT(pElements != NULL, "illegal element array");
		DBG_ASSERT(nElementNo >= 0, "illegal name array index");
		DBG_ASSERT(nElementNo <= 2, "illegal name array index");
		
		if ((pElements != NULL) && (nElementNo != -1))
		{
			SvXMLElementExport aElem(rExport,
									 XML_NAMESPACE_TEXT, 
									 pElements[nElementNo],
									 sal_False, sal_False);
		}
	}
}

void XMLIndexMarkExport::ExportTOCMarkAttributes(
	const Reference<XPropertySet> & rPropSet)
{
	// outline level
	sal_Int16 nLevel = 0;
	Any aAny = rPropSet->getPropertyValue(sLevel);
	aAny >>= nLevel;
	OUStringBuffer sBuf;
	SvXMLUnitConverter::convertNumber(sBuf, (sal_Int32)nLevel + 1);
	rExport.AddAttribute(XML_NAMESPACE_TEXT, XML_OUTLINE_LEVEL, 
							 sBuf.makeStringAndClear());
}

void lcl_ExportPropertyString( SvXMLExport& rExport,
                               const Reference<XPropertySet> & rPropSet,
                               const OUString sProperty,
                               XMLTokenEnum eToken,
                               Any& rAny )
{
    rAny = rPropSet->getPropertyValue( sProperty );

	OUString sValue;
	if( rAny >>= sValue )
    {
        if( sValue.getLength() > 0 )
        {
            rExport.AddAttribute( XML_NAMESPACE_TEXT, eToken, sValue );
        }
	}
}

void lcl_ExportPropertyBool( SvXMLExport& rExport,
                             const Reference<XPropertySet> & rPropSet,
                             const OUString sProperty,
                             XMLTokenEnum eToken,
                             Any& rAny )
{
    rAny = rPropSet->getPropertyValue( sProperty );

	sal_Bool bValue = sal_Bool();
	if( rAny >>= bValue )
    {
        if( bValue )
        {
            rExport.AddAttribute( XML_NAMESPACE_TEXT, eToken, XML_TRUE );
        }
	}
}

void XMLIndexMarkExport::ExportUserIndexMarkAttributes(
	const Reference<XPropertySet> & rPropSet)
{
	// name of user index 
	// (unless it's the default index; then it has no name)
    Any aAny;
    lcl_ExportPropertyString( rExport, rPropSet, sUserIndexName, XML_INDEX_NAME, aAny );
    
    // additionally export outline level; just reuse ExportTOCMarkAttributes
    ExportTOCMarkAttributes( rPropSet );
}

void XMLIndexMarkExport::ExportAlphabeticalIndexMarkAttributes(
	const Reference<XPropertySet> & rPropSet)
{
	// primary and secondary keys (if available)
    Any aAny;
    lcl_ExportPropertyString( rExport, rPropSet, sTextReading, XML_STRING_VALUE_PHONETIC, aAny );
    lcl_ExportPropertyString( rExport, rPropSet, sPrimaryKey, XML_KEY1, aAny );
    lcl_ExportPropertyString( rExport, rPropSet, sPrimaryKeyReading, XML_KEY1_PHONETIC, aAny );
    lcl_ExportPropertyString( rExport, rPropSet, sSecondaryKey, XML_KEY2, aAny );
    lcl_ExportPropertyString( rExport, rPropSet, sSecondaryKeyReading, XML_KEY2_PHONETIC, aAny );
    lcl_ExportPropertyBool( rExport, rPropSet, sMainEntry, XML_MAIN_ENTRY, aAny );
}

void XMLIndexMarkExport::GetID(
	OUStringBuffer& sBuf, 
	const Reference<XPropertySet> & rPropSet)
{
	static const sal_Char sPrefix[] = "IMark";

	// HACK: use address of object to form identifier
	sal_Int64 nId = sal::static_int_cast<sal_Int64>(reinterpret_cast<sal_uIntPtr>(rPropSet.get()));
	sBuf.appendAscii(sPrefix, sizeof(sPrefix)-1);
	sBuf.append(nId);
}
