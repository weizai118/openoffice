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
#include "precompiled_cppuhelper.hxx"

#include <cppuhelper/stdidlclass.hxx>

#include <com/sun/star/reflection/XIdlClassProvider.hpp>
#include <com/sun/star/lang/XServiceInfo.hpp>

#include "testhelper.hxx"

using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::reflection;
using namespace ::rtl;




void testidlclass( const Reference < XMultiServiceFactory > &rSMgr)
{
	const OUString sImplName( OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.comp.cpputest") ) );

	// this reference is static to test behaviour during exiting application
	Reference < XIdlClass > r =
				::cppu::createStandardClass(
								rSMgr ,
								sImplName,
								Reference < XIdlClass > () ,
								(XMultiServiceFactory * ) 0 ,
								(XServiceInfo * ) 0
											);

	OSL_ASSERT( r.is() );


	{  // test the xidlclassprovider interface !
		Reference< XIdlClassProvider > rProv( r , UNO_QUERY );
		OSL_ASSERT( rProv.is() );

		{
			Sequence < Reference < XIdlClass > > seq = rProv->getIdlClasses();

			// is always one
			OSL_ASSERT( seq.getLength() == 1 );

			// test the weak reference
			rProv->getIdlClasses();

		}

		rProv->getIdlClasses();


	}


	OSL_ASSERT( r->getName() == sImplName );

	// test equals
	Reference < XIdlClass > r2 =
				::cppu::createStandardClass(
								rSMgr ,
								sImplName,
								Reference < XIdlClass > () ,
								(XMultiServiceFactory * ) 0 ,
								(XServiceInfo * ) 0
											);
	// test for implementation name
	OSL_ASSERT( r2->equals( r ) );

	Sequence < Reference < XIdlClass > > seqIdlClass = r->getInterfaces();

	//TODO !
	// one idl class for one interface
	// this test fails, if core reflection fails !
//	OSL_ASSERT( 1 == seqIdlClass.getLength() );
//	Reference < XIdlClass > rIdlInterface = seqIdlClass.getArray()[0];

	// check for IdlClass interface returned by Core Reflection
//	OSL_ASSERT( rIdlInterface.is() );



	// Test all ten templates
	Reference< XIdlClass > x = ::cppu::createStandardClass(
								rSMgr ,
								sImplName,
								Reference < XIdlClass > () ,
								(XMultiServiceFactory * ) 0 ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0
											);

	// Test all ten templates
	x = ::cppu::createStandardClass(
								rSMgr ,
								sImplName,
								Reference < XIdlClass > () ,
								(XMultiServiceFactory * ) 0 ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0
											);


	// Test all ten templates
	x = ::cppu::createStandardClass(
								rSMgr ,
								sImplName,
								Reference < XIdlClass > () ,
								(XMultiServiceFactory * ) 0 ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0
											);



	// Test all ten templates
	x = ::cppu::createStandardClass(
								rSMgr ,
								sImplName,
								Reference < XIdlClass > () ,
								(XMultiServiceFactory * ) 0 ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0
											);



	// Test all ten templates
	x = ::cppu::createStandardClass(
								rSMgr ,
								sImplName,
								Reference < XIdlClass > () ,
								(XMultiServiceFactory * ) 0 ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0
											);


	// Test all ten templates
	x = ::cppu::createStandardClass(
								rSMgr ,
								sImplName,
								Reference < XIdlClass > () ,
								(XMultiServiceFactory * ) 0 ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0
											);




	// Test all ten templates
	x = ::cppu::createStandardClass(
								rSMgr ,
								sImplName,
								Reference < XIdlClass > () ,
								(XMultiServiceFactory * ) 0 ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0
											);

	// Test all ten templates
	x = ::cppu::createStandardClass(
								rSMgr ,
								sImplName,
								Reference < XIdlClass > () ,
								(XMultiServiceFactory * ) 0 ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0  ,
								(XServiceInfo * ) 0
											);
}
