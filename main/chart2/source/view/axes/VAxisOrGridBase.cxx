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
#include "precompiled_chart2.hxx"

#include "VAxisOrGridBase.hxx"
#include "ShapeFactory.hxx"
#include "CommonConverters.hxx"
#include "Tickmarks.hxx"

// header for define DBG_ASSERT
#include <tools/debug.hxx>

//.............................................................................
namespace chart
{
//.............................................................................
using namespace ::com::sun::star;
using namespace ::com::sun::star::chart2;
using ::com::sun::star::uno::Reference;

VAxisOrGridBase::VAxisOrGridBase( sal_Int32 nDimensionIndex, sal_Int32 nDimensionCount )
            : PlotterBase( nDimensionCount )
            , m_nDimensionIndex( nDimensionIndex )
            , m_eLeftWallPos(CuboidPlanePosition_Left)
            , m_eBackWallPos(CuboidPlanePosition_Back)
            , m_eBottomPos(CuboidPlanePosition_Bottom)
{
}

VAxisOrGridBase::~VAxisOrGridBase()
{
}

void VAxisOrGridBase::setExplicitScaleAndIncrement(
              const ExplicitScaleData& rScale
            , const ExplicitIncrementData& rIncrement )
            throw (uno::RuntimeException)
{
    m_aScale = rScale;
    m_aIncrement = rIncrement;
}

void VAxisOrGridBase::setTransformationSceneToScreen( const drawing::HomogenMatrix& rMatrix )
{
    m_aMatrixScreenToScene = HomogenMatrixToB3DHomMatrix(rMatrix);
    PlotterBase::setTransformationSceneToScreen( rMatrix);
}

void VAxisOrGridBase::set3DWallPositions( CuboidPlanePosition eLeftWallPos, CuboidPlanePosition eBackWallPos, CuboidPlanePosition eBottomPos )
{
    m_eLeftWallPos = eLeftWallPos;
    m_eBackWallPos = eBackWallPos;
    m_eBottomPos = eBottomPos;
}

TickFactory* VAxisOrGridBase::createTickFactory()
{
    return new TickFactory( m_aScale, m_aIncrement );
}

//.............................................................................
} //namespace chart
//.............................................................................
