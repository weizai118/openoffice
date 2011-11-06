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


#ifndef _COLEX_HXX
#define _COLEX_HXX

#include <svx/pagectrl.hxx>
#include <editeng/paperinf.hxx>
#include "swdllapi.h"
#include <fmtclds.hxx>
#include "frmatr.hxx"

class SwColMgr;
class SfxItemSet;

/* -----------------------------08.02.2002 11:32------------------------------

 ---------------------------------------------------------------------------*/
class SW_DLLPUBLIC SwPageExample : public SvxPageWindow
{
public:
    SwPageExample(Window* pPar, const ResId& rResId ) :
                                SvxPageWindow(pPar, rResId )
                                {SetSize(SvxPaperInfo::GetPaperSize(PAPER_A4));/*DIN A4*/}

    void UpdateExample( const SfxItemSet& rSet );
};
/* -----------------------------08.02.2002 11:34------------------------------

 ---------------------------------------------------------------------------*/
class SwTextGridItem;
class SW_DLLPUBLIC SwPageGridExample : public SwPageExample
{
    SwTextGridItem*     pGridItem;
    sal_Bool            m_bVertical;
protected:
	virtual void DrawPage( const Point& rPoint,
						   const sal_Bool bSecond,
						   const sal_Bool bEnabled );
public:
    SwPageGridExample(Window* pPar, const ResId& rResId ) :
                                SwPageExample(pPar, rResId ),
                                pGridItem(0),
                                m_bVertical(sal_False){}
    ~SwPageGridExample();
    void UpdateExample( const SfxItemSet& rSet );
};
/*--------------------------------------------------------------------
	Beschreibung:
 --------------------------------------------------------------------*/

class SW_DLLPUBLIC SwColExample : public SwPageExample
{
	SwColMgr* 	pColMgr;
    
    using SwPageExample::UpdateExample;

protected:
	virtual void DrawPage( const Point& rPoint,
						   const sal_Bool bSecond,
						   const sal_Bool bEnabled );

public:
        SwColExample(Window* pPar, const ResId& rResId ) :
                                SwPageExample(pPar, rResId ),
                                pColMgr(0){}

    void UpdateExample( const SfxItemSet& rSet, SwColMgr* pMgr  )
        {   pColMgr = pMgr;
            SwPageExample::UpdateExample(rSet);
        }
};

/*-----------------25.10.96 08.23-------------------

--------------------------------------------------*/
class SW_DLLPUBLIC SwColumnOnlyExample : public Window
{
private:
    Size        m_aWinSize;

    Size        m_aFrmSize;
    SwFmtCol    m_aCols;

protected:
	virtual void Paint( const Rectangle& rRect );

public:
	SwColumnOnlyExample( Window* , const ResId& );

    void        SetColumns(const SwFmtCol& rCol);

};

#endif // _COLEX_HXX
