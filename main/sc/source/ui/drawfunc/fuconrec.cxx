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
#include "precompiled_sc.hxx"

#include "fuconrec.hxx"
#include "tabvwsh.hxx"
#include "sc.hrc"
#include "drawview.hxx"

#include <editeng/outlobj.hxx>
// #98185# Create default drawing objects via keyboard
#include <svx/svdopath.hxx>
#include <svx/svdocapt.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>
#include <basegfx/point/b2dpoint.hxx>

#include "scresid.hxx"

//------------------------------------------------------------------------


/*************************************************************************
|*
|* Konstruktor
|*
\************************************************************************/

FuConstRectangle::FuConstRectangle(ScTabViewShell* pViewSh, Window* pWin, ScDrawView* pViewP,
				   SdrModel* pDoc, SfxRequest& rReq)
    : FuConstruct(pViewSh, pWin, pViewP, pDoc, rReq)
{
}

/*************************************************************************
|*
|* Destruktor
|*
\************************************************************************/

FuConstRectangle::~FuConstRectangle()
{
}

/*************************************************************************
|*
|* MouseButtonDown-event
|*
\************************************************************************/

sal_Bool __EXPORT FuConstRectangle::MouseButtonDown(const MouseEvent& rMEvt)
{
	// #95491# remember button state for creation of own MouseEvents
	SetMouseButtonCode(rMEvt.GetButtons());

	sal_Bool bReturn = FuConstruct::MouseButtonDown(rMEvt);

	if ( rMEvt.IsLeft() && !pView->IsAction() )
	{
		Point aPos( pWindow->PixelToLogic( rMEvt.GetPosPixel() ) );

		pWindow->CaptureMouse();

		if ( pView->GetCurrentObjIdentifier() == OBJ_CAPTION )
		{
			Size aCaptionSize ( 2268, 1134 ); // 4x2cm

			bReturn = pView->BegCreateCaptionObj( aPos, aCaptionSize );

			// wie stellt man den Font ein, mit dem geschrieben wird
		}
		else
			bReturn = pView->BegCreateObj(aPos);
	}
	return bReturn;
}

/*************************************************************************
|*
|* MouseMove-event
|*
\************************************************************************/

sal_Bool __EXPORT FuConstRectangle::MouseMove(const MouseEvent& rMEvt)
{
	return FuConstruct::MouseMove(rMEvt);
}

/*************************************************************************
|*
|* MouseButtonUp-event
|*
\************************************************************************/

sal_Bool __EXPORT FuConstRectangle::MouseButtonUp(const MouseEvent& rMEvt)
{
	// #95491# remember button state for creation of own MouseEvents
	SetMouseButtonCode(rMEvt.GetButtons());

	sal_Bool bReturn = sal_False;

	if ( pView->IsCreateObj() && rMEvt.IsLeft() )
	{
		Point aPnt( pWindow->PixelToLogic( rMEvt.GetPosPixel() ) );
		pView->EndCreateObj(SDRCREATE_FORCEEND);

		if (aSfxRequest.GetSlot() == SID_DRAW_CAPTION_VERTICAL)
		{
			//	set vertical flag for caption object

			const SdrMarkList& rMarkList = pView->GetMarkedObjectList();
			if (rMarkList.GetMark(0))
			{
				SdrObject* pObj = rMarkList.GetMark(0)->GetMarkedSdrObj();
				//	create OutlinerParaObject now so it can be set to vertical
				if ( pObj->ISA(SdrTextObj) )
					((SdrTextObj*)pObj)->ForceOutlinerParaObject();
				OutlinerParaObject* pOPO = pObj->GetOutlinerParaObject();
				if( pOPO && !pOPO->IsVertical() )
					pOPO->SetVertical( sal_True );
			}
		}

		bReturn = sal_True;
	}
	return (FuConstruct::MouseButtonUp(rMEvt) || bReturn);
}

/*************************************************************************
|*
|* Tastaturereignisse bearbeiten
|*
|* Wird ein KeyEvent bearbeitet, so ist der Return-Wert sal_True, andernfalls
|* FALSE.
|*
\************************************************************************/

sal_Bool __EXPORT FuConstRectangle::KeyInput(const KeyEvent& rKEvt)
{
	sal_Bool bReturn = FuConstruct::KeyInput(rKEvt);
	return(bReturn);
}

/*************************************************************************
|*
|* Function aktivieren
|*
\************************************************************************/

void FuConstRectangle::Activate()
{
	SdrObjKind aObjKind;

	switch (aSfxRequest.GetSlot() )
	{
		case SID_DRAW_LINE:
			aNewPointer = Pointer( POINTER_DRAW_LINE );
			aObjKind = OBJ_LINE;
			break;

		case SID_DRAW_RECT:
			aNewPointer = Pointer( POINTER_DRAW_RECT );
			aObjKind = OBJ_RECT;
			break;

		case SID_DRAW_ELLIPSE:
			aNewPointer = Pointer( POINTER_DRAW_ELLIPSE );
			aObjKind = OBJ_CIRC;
			break;

		case SID_DRAW_CAPTION:
		case SID_DRAW_CAPTION_VERTICAL:
			aNewPointer = Pointer( POINTER_DRAW_CAPTION );
			aObjKind = OBJ_CAPTION;
			break;

		default:
			aNewPointer = Pointer( POINTER_CROSS );
			aObjKind = OBJ_RECT;
			break;
	}

    pView->SetCurrentObj(sal::static_int_cast<sal_uInt16>(aObjKind));

	aOldPointer = pWindow->GetPointer();
	pViewShell->SetActivePointer( aNewPointer );

	FuConstruct::Activate();
}

/*************************************************************************
|*
|* Function deaktivieren
|*
\************************************************************************/

void FuConstRectangle::Deactivate()
{
	FuConstruct::Deactivate();
	pViewShell->SetActivePointer( aOldPointer );
}

// #98185# Create default drawing objects via keyboard
SdrObject* FuConstRectangle::CreateDefaultObject(const sal_uInt16 nID, const Rectangle& rRectangle)
{
	// case SID_DRAW_LINE:
	// case SID_DRAW_RECT:
	// case SID_DRAW_ELLIPSE:
	// case SID_DRAW_CAPTION:
	// case SID_DRAW_CAPTION_VERTICAL:

	SdrObject* pObj = SdrObjFactory::MakeNewObject(
		pView->GetCurrentObjInventor(), pView->GetCurrentObjIdentifier(),
		0L, pDrDoc);

	if(pObj)
	{
		Rectangle aRect(rRectangle);
		Point aStart = aRect.TopLeft();
		Point aEnd = aRect.BottomRight();

		switch(nID)
		{
			case SID_DRAW_LINE:
			{
				if(pObj->ISA(SdrPathObj))
				{
					sal_Int32 nYMiddle((aRect.Top() + aRect.Bottom()) / 2);
					basegfx::B2DPolygon aPoly;
					aPoly.append(basegfx::B2DPoint(aStart.X(), nYMiddle));
					aPoly.append(basegfx::B2DPoint(aEnd.X(), nYMiddle));
					((SdrPathObj*)pObj)->SetPathPoly(basegfx::B2DPolyPolygon(aPoly));
				}
				else
				{
					DBG_ERROR("Object is NO line object");
				}

				break;
			}
			case SID_DRAW_CAPTION:
			case SID_DRAW_CAPTION_VERTICAL:
			{
				if(pObj->ISA(SdrCaptionObj))
				{
					sal_Bool bIsVertical(SID_DRAW_CAPTION_VERTICAL == nID);

					((SdrTextObj*)pObj)->SetVerticalWriting(bIsVertical);
						
					if(bIsVertical)
					{
						SfxItemSet aSet(pObj->GetMergedItemSet());
						aSet.Put(SdrTextVertAdjustItem(SDRTEXTVERTADJUST_CENTER));
						aSet.Put(SdrTextHorzAdjustItem(SDRTEXTHORZADJUST_RIGHT));
						pObj->SetMergedItemSet(aSet);
					}

					//  #105815# don't set default text, start edit mode instead
					//  (Edit mode is started in ScTabViewShell::ExecDraw, because
					//  it must be handled by FuText)
					// String aText(ScResId(STR_CAPTION_DEFAULT_TEXT));
					// ((SdrCaptionObj*)pObj)->SetText(aText);

					((SdrCaptionObj*)pObj)->SetLogicRect(aRect);
					((SdrCaptionObj*)pObj)->SetTailPos(
						aRect.TopLeft() - Point(aRect.GetWidth() / 2, aRect.GetHeight() / 2));
				}
				else
				{
					DBG_ERROR("Object is NO caption object");
				}

				break;
			}

			default:
			{
				pObj->SetLogicRect(aRect);

				break;
			}
		}

		SfxItemSet aAttr(pDrDoc->GetItemPool());
		pObj->SetMergedItemSet(aAttr);
	}

	return pObj;
}

