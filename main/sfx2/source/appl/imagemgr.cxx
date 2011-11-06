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
#include "precompiled_sfx2.hxx"
#include "sfx2/imagemgr.hxx"
#include <com/sun/star/frame/XController.hpp>
#include <com/sun/star/ui/XImageManager.hpp>
#include <com/sun/star/frame/XModuleManager.hpp>
#include <com/sun/star/ui/XModuleUIConfigurationManagerSupplier.hpp>
#include <com/sun/star/ui/ImageType.hpp>
#include <com/sun/star/ui/XUIConfigurationManagerSupplier.hpp>

#include <tools/urlobj.hxx>
#include <svtools/imagemgr.hxx>
#include <comphelper/processfactory.hxx>
#include <rtl/ustring.hxx>
#include <rtl/logfile.hxx>

#include "sfx2/imgmgr.hxx"
#include <sfx2/app.hxx>
#include <sfx2/unoctitm.hxx>
#include <sfx2/dispatch.hxx>
#include <sfx2/msg.hxx>
#include <sfx2/msgpool.hxx>
#include <sfx2/viewfrm.hxx>
#include <sfx2/module.hxx>
#include <sfx2/objsh.hxx>
#include <sfx2/docfac.hxx>

#include <hash_map>

using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::frame;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::util;
using namespace ::com::sun::star::ui;
using namespace ::com::sun::star::frame;

typedef std::hash_map< ::rtl::OUString,
                       WeakReference< XImageManager >,
                       ::rtl::OUStringHash,
                       ::std::equal_to< ::rtl::OUString > > ModuleIdToImagegMgr;

static WeakReference< XModuleManager >                        m_xModuleManager;
static WeakReference< XModuleUIConfigurationManagerSupplier > m_xModuleCfgMgrSupplier;
static WeakReference< XURLTransformer >                       m_xURLTransformer;
static ModuleIdToImagegMgr                                    m_aModuleIdToImageMgrMap;

Image SAL_CALL GetImage( const ::com::sun::star::uno::Reference< ::com::sun::star::frame::XFrame >& rFrame, const ::rtl::OUString& aURL, sal_Bool bBig, sal_Bool bHiContrast )
{
    // TODO/LATeR: shouldn't this become a method at SfxViewFrame?! That would save the UnoTunnel
    if ( !rFrame.is() )
        return Image();

    INetURLObject aObj( aURL );
    INetProtocol  nProtocol = aObj.GetProtocol();

    Reference < XController > xController;
    Reference < XModel > xModel;
    if ( rFrame.is() )
        xController = rFrame->getController();
    if ( xController.is() )
        xModel = xController->getModel();

    rtl::OUString aCommandURL( aURL );
    if ( nProtocol == INET_PROT_SLOT )
    {
        /*
        // Support old way to retrieve image via slot URL
        Reference< XURLTransformer > xURLTransformer = m_xURLTransformer;
        if ( !xURLTransformer.is() )
        {
            xURLTransformer = Reference< XURLTransformer >(
                                ::comphelper::getProcessServiceFactory()->createInstance(
                                    rtl::OUString::createFromAscii("com.sun.star.util.URLTransformer" )),
                                UNO_QUERY );
            m_xURLTransformer = xURLTransformer;
        }

        URL aTargetURL;
        aTargetURL.Complete = aURL;
        xURLTransformer->parseStrict( aTargetURL );
        sal_uInt16 nId = ( sal_uInt16 ) aTargetURL.Path.toInt32();*/
        sal_uInt16 nId = ( sal_uInt16 ) String(aURL).Copy(5).ToInt32();
        const SfxSlot* pSlot = 0;
        if ( xModel.is() )
        {
            Reference < XUnoTunnel > xObj( xModel, UNO_QUERY );
            Sequence < sal_Int8 > aSeq( SvGlobalName( SFX_GLOBAL_CLASSID ).GetByteSequence() );
            sal_Int64 nHandle = xObj.is() ? xObj->getSomething( aSeq ) : 0;
            if ( nHandle )
            {
                SfxObjectShell* pDoc = reinterpret_cast<SfxObjectShell*>(sal::static_int_cast<sal_IntPtr>( nHandle ));
                SfxModule* pModule = pDoc->GetFactory().GetModule();
                pSlot = pModule->GetSlotPool()->GetSlot( nId );
            }
        }
        else
            pSlot = SfxSlotPool::GetSlotPool().GetSlot( nId );

        if ( pSlot )
        {
            aCommandURL = rtl::OUString( RTL_CONSTASCII_USTRINGPARAM( ".uno:" ));
            aCommandURL += rtl::OUString::createFromAscii( pSlot->GetUnoName() );
        }
        else
            aCommandURL = rtl::OUString();
    }

    Reference< XImageManager > xDocImgMgr;
    if ( xModel.is() )
    {
        Reference< XUIConfigurationManagerSupplier > xSupplier( xModel, UNO_QUERY );
        if ( xSupplier.is() )
        {
            Reference< XUIConfigurationManager > xDocUICfgMgr( xSupplier->getUIConfigurationManager(), UNO_QUERY );
            xDocImgMgr = Reference< XImageManager >( xDocUICfgMgr->getImageManager(), UNO_QUERY );
        }
    }

    sal_Int16 nImageType( ::com::sun::star::ui::ImageType::COLOR_NORMAL|
                            ::com::sun::star::ui::ImageType::SIZE_DEFAULT );
    if ( bBig )
        nImageType |= ::com::sun::star::ui::ImageType::SIZE_LARGE;
    if ( bHiContrast )
        nImageType |= ::com::sun::star::ui::ImageType::COLOR_HIGHCONTRAST;

    if ( xDocImgMgr.is() )
    {
        Sequence< Reference< ::com::sun::star::graphic::XGraphic > > aGraphicSeq;
        Sequence< rtl::OUString > aImageCmdSeq( 1 );
        aImageCmdSeq[0] = aCommandURL;

        try
        {
            aGraphicSeq = xDocImgMgr->getImages( nImageType, aImageCmdSeq );
            Reference< ::com::sun::star::graphic::XGraphic > xGraphic = aGraphicSeq[0];
            Image aImage( xGraphic );

            if ( !!aImage )
                return aImage;
        }
        catch ( Exception& )
        {
        }
    }

    Reference< XModuleManager > xModuleManager = m_xModuleManager;

    if ( !xModuleManager.is() )
    {
        xModuleManager = Reference< XModuleManager >(
                            ::comphelper::getProcessServiceFactory()->createInstance(
                                rtl::OUString( RTL_CONSTASCII_USTRINGPARAM(
                                    "com.sun.star.frame.ModuleManager" ))),
                            UNO_QUERY );
        m_xModuleManager = xModuleManager;
    }

    try
    {
        if ( aCommandURL.getLength() > 0 )
        {
            Reference< XImageManager > xModuleImageManager;
            rtl::OUString aModuleId = xModuleManager->identify( rFrame );
            ModuleIdToImagegMgr::iterator pIter = m_aModuleIdToImageMgrMap.find( aModuleId );
            if ( pIter != m_aModuleIdToImageMgrMap.end() )
                xModuleImageManager = pIter->second;
            else
            {
                Reference< XModuleUIConfigurationManagerSupplier > xModuleCfgMgrSupplier = m_xModuleCfgMgrSupplier;

                if ( !xModuleCfgMgrSupplier.is() )
                {
                    xModuleCfgMgrSupplier = Reference< XModuleUIConfigurationManagerSupplier >(
                                                ::comphelper::getProcessServiceFactory()->createInstance(
                                                    rtl::OUString( RTL_CONSTASCII_USTRINGPARAM(
                                                        "com.sun.star.ui.ModuleUIConfigurationManagerSupplier" ))),
                                                UNO_QUERY );

                    m_xModuleCfgMgrSupplier = xModuleCfgMgrSupplier;
                }

                Reference< XUIConfigurationManager > xUICfgMgr = xModuleCfgMgrSupplier->getUIConfigurationManager( aModuleId );
                xModuleImageManager = Reference< XImageManager >( xUICfgMgr->getImageManager(), UNO_QUERY );
                m_aModuleIdToImageMgrMap.insert( ModuleIdToImagegMgr::value_type( aModuleId, xModuleImageManager ));
            }

            Sequence< Reference< ::com::sun::star::graphic::XGraphic > > aGraphicSeq;
            Sequence< rtl::OUString > aImageCmdSeq( 1 );
            aImageCmdSeq[0] = aCommandURL;

            aGraphicSeq = xModuleImageManager->getImages( nImageType, aImageCmdSeq );

            Reference< ::com::sun::star::graphic::XGraphic > xGraphic = aGraphicSeq[0];
            Image aImage( xGraphic );

            if ( !!aImage )
                return aImage;
            else if ( nProtocol != INET_PROT_UNO && nProtocol != INET_PROT_SLOT )
                return SvFileInformationManager::GetImageNoDefault( aObj, bBig, bHiContrast );
        }
    }
    catch ( Exception& )
    {
    }

    return Image();
}
