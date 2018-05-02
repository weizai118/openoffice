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
#include "FrameOASISTContext.hxx"
#include "IgnoreTContext.hxx"
#include "MutableAttrList.hxx"
#include "xmloff/xmlnmspe.hxx"
#include "ActionMapTypesOASIS.hxx"
#include "ElemTransformerAction.hxx"
#include "TransformerActions.hxx"
#ifndef _XMLOFF_TRANSFORMERBASE_HXX
#include "TransformerBase.hxx"
#endif

using ::rtl::OUString;

using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::xml::sax;
using namespace ::xmloff::token;

TYPEINIT1( XMLFrameOASISTransformerContext, XMLTransformerContext );

sal_Bool XMLFrameOASISTransformerContext::IsLinkedEmbeddedObject(
			const OUString& rLocalName,
			const Reference< XAttributeList >& rAttrList )
{
	if( !(IsXMLToken( rLocalName, XML_OBJECT ) ||
		  IsXMLToken( rLocalName, XML_OBJECT_OLE)  ) )
		return sal_False;

	sal_Int16 nAttrCount = rAttrList.is() ? rAttrList->getLength() : 0;
	for( sal_Int16 i=0; i < nAttrCount; i++ )
	{
		OUString aAttrName( rAttrList->getNameByIndex( i ) );
		OUString aLocalName;
		sal_uInt16 nPrefix =
			GetTransformer().GetNamespaceMap().GetKeyByAttrName( aAttrName, 
																 &aLocalName );
		if( XML_NAMESPACE_XLINK == nPrefix &&
			IsXMLToken( aLocalName, XML_HREF ) )
		{
			OUString sHRef( rAttrList->getValueByIndex( i ) );
            if (sHRef.getLength() == 0)
            {
                // When the href is empty then the object is not linked but
                // a placeholder.
                return sal_False;
            }
			GetTransformer().ConvertURIToOOo( sHRef, sal_True );
			return !(sHRef.getLength() && '#'==sHRef[0]);
		}
	}

	return sal_False;
}
			

XMLFrameOASISTransformerContext::XMLFrameOASISTransformerContext( 
		XMLTransformerBase& rImp, 
		const OUString& rQName ) :
	XMLTransformerContext( rImp, rQName ),
	m_bIgnoreElement( false )
{
}

XMLFrameOASISTransformerContext::~XMLFrameOASISTransformerContext()
{
}

void XMLFrameOASISTransformerContext::StartElement( 
	const Reference< XAttributeList >& rAttrList )
{
	m_xAttrList = new XMLMutableAttributeList( rAttrList, sal_True );

	sal_Int16 nAttrCount = rAttrList.is() ? rAttrList->getLength() : 0;
	for( sal_Int16 i=0; i < nAttrCount; i++ )
	{
		const OUString& rAttrName = rAttrList->getNameByIndex( i );
		OUString aLocalName;
		sal_uInt16 nPrefix =
			GetTransformer().GetNamespaceMap().GetKeyByAttrName( rAttrName, &aLocalName );
	
		if( (nPrefix == XML_NAMESPACE_PRESENTATION) && IsXMLToken( aLocalName, XML_CLASS ) )
		{
			const OUString& rAttrValue = rAttrList->getValueByIndex( i );
			if( IsXMLToken( rAttrValue, XML_HEADER ) ||	IsXMLToken( rAttrValue, XML_FOOTER ) ||
				IsXMLToken( rAttrValue, XML_PAGE_NUMBER ) || IsXMLToken( rAttrValue, XML_DATE_TIME ) )
			{
				m_bIgnoreElement = true;
				break;
			}
		}
	}
}

XMLTransformerContext *XMLFrameOASISTransformerContext::CreateChildContext(
		sal_uInt16 nPrefix,
		const OUString& rLocalName,
		const OUString& rQName,
		const Reference< XAttributeList >& rAttrList )
{
	XMLTransformerContext *pContext = 0;

	if( m_bIgnoreElement )
	{
		// do not export the frame element and all of its children
		pContext = new XMLIgnoreTransformerContext( GetTransformer(), 
																rQName,
																sal_True, sal_True );
	}
	else
	{
		XMLTransformerActions *pActions =
			GetTransformer().GetUserDefinedActions( OASIS_FRAME_ELEM_ACTIONS );
		OSL_ENSURE( pActions, "go no actions" );
		XMLTransformerActions::key_type aKey( nPrefix, rLocalName );
		XMLTransformerActions::const_iterator aIter = pActions->find( aKey );

		if( !(aIter == pActions->end()) )
		{
			switch( (*aIter).second.m_nActionType )
			{
			case XML_ETACTION_COPY:
				if( !m_aElemQName.getLength() && 
					!IsLinkedEmbeddedObject( rLocalName, rAttrList ) )
				{
					pContext = new XMLIgnoreTransformerContext( GetTransformer(), 
																rQName,
																sal_False, sal_False );
					m_aElemQName = rQName;
					static_cast< XMLMutableAttributeList * >( m_xAttrList.get() )
						->AppendAttributeList( rAttrList );
					GetTransformer().ProcessAttrList( m_xAttrList, 
													  OASIS_SHAPE_ACTIONS,
													  sal_False	);
					GetTransformer().GetDocHandler()->startElement( m_aElemQName, 
																	m_xAttrList );
				}
				else
				{
					pContext = new XMLIgnoreTransformerContext( GetTransformer(), 
																rQName,
																sal_True, sal_True );
				}
				break;
			default:
				OSL_ENSURE( sal_False, "unknown action" );
				break;
			}
		}
	}

	// default is copying
	if( !pContext )
		pContext = XMLTransformerContext::CreateChildContext( nPrefix,
															  rLocalName,
															  rQName,
															  rAttrList );

	return pContext;
}

void XMLFrameOASISTransformerContext::EndElement()
{
	if( !m_bIgnoreElement )
		GetTransformer().GetDocHandler()->endElement( m_aElemQName );
}

void XMLFrameOASISTransformerContext::Characters( const OUString& rChars )
{
	// ignore
	if( m_aElemQName.getLength() && !m_bIgnoreElement )
		XMLTransformerContext::Characters( rChars );
}
