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



#include "vclxtabpage.hxx"
#include "forward.hxx"

#include <com/sun/star/awt/PosSize.hpp>
#include <toolkit/helper/convert.hxx>
#include <vcl/tabpage.hxx>
#include <vcl/tabctrl.hxx>

#if !defined (__GNUC__)
#define __PRETTY_FUNCTION__ __FUNCTION__
#endif /* !__GNUC__ */

namespace layoutimpl
{

using namespace ::com::sun::star;

// XInterface
IMPLEMENT_FORWARD_XINTERFACE2( VCLXTabPage, VCLXWindow, Bin );

// XTypeProvider
IMPLEMENT_FORWARD_XTYPEPROVIDER1( VCLXTabPage, VCLXWindow );

VCLXTabPage::VCLXTabPage( Window *p )
    : VCLXWindow()
    , Bin()
    , bRealized( false )
{
    /* FIXME: before Window is set, setLabel, setProperty->setImage
     * are silent no-ops.  */
    p->SetComponentInterface( this );
}

VCLXTabPage::~VCLXTabPage()
{
}

void SAL_CALL VCLXTabPage::dispose() throw(uno::RuntimeException)
{
    {
        ::vos::OGuard aGuard( GetMutex() );
        
        lang::EventObject aDisposeEvent;
        aDisposeEvent.Source = W3K_EXPLICIT_CAST (*this);
    }
    
    VCLXWindow::dispose();
}

void SAL_CALL VCLXTabPage::allocateArea( awt::Rectangle const& area )
    throw (uno::RuntimeException)
{
    awt::Size currentSize = getSize();
    awt::Size requestedSize = getMinimumSize();
    requestedSize.Height = getHeightForWidth( area.Width );

    if ( currentSize.Width > 0 && currentSize.Height > 0
         && requestedSize.Width > currentSize.Width )
        requestedSize.Width = currentSize.Width;
    if ( currentSize.Width > 0 && currentSize.Height > 0
         && requestedSize.Height > currentSize.Height )
        requestedSize.Height = currentSize.Height;

    // FIXME: missing destructor?
    if ( !GetWindow() )
        return;

    Size windowSize = GetWindow()->GetSizePixel();
    Window *parent = GetWindow()->GetParent();
    Size parentSize = parent->GetSizePixel();

    Point pos = GetWindow()->GetPosPixel();
#ifndef __SUNPRO_CC
    OSL_TRACE ("\n%s", __PRETTY_FUNCTION__);
    OSL_TRACE ("%s: curpos: %d ,%d", __FUNCTION__, pos.X(), pos.Y() );

    OSL_TRACE ("%s: cursize: %d ,%d", __FUNCTION__, currentSize.Width, currentSize.Height );
    OSL_TRACE ("%s: area: %d, %d", __FUNCTION__, area.Width, area.Height );
    OSL_TRACE ("%s: requestedSize: %d, %d", __FUNCTION__, requestedSize.Width, requestedSize.Height );
    OSL_TRACE ("%s: parent: %d, %d", __FUNCTION__, parentSize.Width(), parentSize.Height() );
    OSL_TRACE ("%s: window: %d, %d", __FUNCTION__, windowSize.Width(), windowSize.Height() );
#endif

#if 0
    if (requestedSize.Width > parentSize.Width ()
        || requestedSize.Height > parentSize.Height ())
    {
#ifndef __SUNPRO_CC
        OSL_TRACE ("%s: ***setting parent: %d, %d", __FUNCTION__, requestedSize.Width, requestedSize.Height );
#endif
        parent->SetSizePixel ( Size (requestedSize.Width, requestedSize.Height) );

        if (Window *grand_parent = parent->GetParent ())
            grand_parent->SetSizePixel ( Size (requestedSize.Width, requestedSize.Height) );
    }
#endif

    if ( !bRealized )
    {
        setPosSize( area.X, area.Y, requestedSize.Width, requestedSize.Height, awt::PosSize::SIZE );
        bRealized = true;
    }
    else
    {
        if ( requestedSize.Width > currentSize.Width + 10)
            setPosSize( 0, 0, requestedSize.Width, 0, awt::PosSize::WIDTH );
        if ( requestedSize.Height > currentSize.Height + 10)
            setPosSize( 0, 0, 0, requestedSize.Height, awt::PosSize::HEIGHT );
    }

    awt::Size newSize = getSize();
#ifndef __SUNPRO_CC
    OSL_TRACE ("%s: newSize: %d, %d", __FUNCTION__, newSize.Width, newSize.Height );
#endif
    maAllocation.Width = newSize.Width;
    maAllocation.Height = newSize.Height;

    Bin::allocateArea( maAllocation );
}

awt::Size SAL_CALL VCLXTabPage::getMinimumSize()
    throw(uno::RuntimeException)
{
    ::vos::OGuard aGuard( GetMutex() );

    return Bin::getMinimumSize();
}

} // namespace layoutimpl
