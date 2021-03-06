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



#ifndef ADC_CPP_CTX2ARY_HXX
#define ADC_CPP_CTX2ARY_HXX



// USED SERVICES
	// BASE CLASSES
#include <ary/cpp/inpcontx.hxx>
#include <doc_deal.hxx>
#include "pev.hxx"
#include "fevnthdl.hxx"
	// COMPONENTS
	// PARAMETERS

namespace ary
{
namespace loc
{
    class File;
}
}



namespace cpp
{


/** @descr
    This class provides information about the context of an
    CodeEntity, which is going to be stored in the repository.
    The information is used mainly by class ary::cpp::Gate.

    Also it provides information for the parser about actual
    state of several public variables.

    @todo
    Include events, which allow correct storing of inline
    documentation after enum values, parameters,
    base classes.
*/
class ContextForAry : public ary::cpp::InputContext,
                      public cpp::PeEnvironment,
                      public cpp::FileScope_EventHandler,
                      public DocuDealer
{
  public:
    // LIFECYCLE
                        ContextForAry(
                            ary::cpp::Gate &  io_rAryGate );
	virtual				~ContextForAry();

    // OPERATIONS
    void                ResetResult()           { aTokenResult.Reset(); }

    // INQUIRY
    const TokenProcessing_Result &
                        CurResult() const       { return aTokenResult; }
    // ACCESS
    TokenProcessing_Result &
                        CurResult()             { return aTokenResult; }

  private:
    // Interface ary::cpp::InputContext:
    virtual ary::loc::File &
                        inq_CurFile() const;
    virtual ary::cpp::Namespace &
                        inq_CurNamespace() const;
    virtual ary::cpp::Class *
                        inq_CurClass() const;
    virtual ary::cpp::Enum *
                        inq_CurEnum() const;

    virtual Owner &     inq_CurOwner() const;
    virtual ary::cpp::E_Protection
                        inq_CurProtection() const;

    // Interface PeEnvironment
	virtual void        do_SetTokenResult(
							E_TokenDone			i_eDone,
							E_EnvStackAction	i_eWhat2DoWithEnvStack,
							ParseEnvironment *	i_pParseEnv2Push );
	virtual void        do_OpenNamespace(
							ary::cpp::Namespace &
                                                io_rOpenedNamespace );
	virtual void        do_OpenExternC(
		                	bool				i_bOnlyForOneDeclaration );
	virtual void        do_OpenClass(
							ary::cpp::Class &	io_rOpenedClass );
	virtual void        do_OpenEnum(
							ary::cpp::Enum &	io_rOpenedEnum );
	virtual void        do_CloseBlock();
	virtual void        do_CloseClass();
	virtual void        do_CloseEnum();
    virtual void        do_SetCurProtection(
                            ary::cpp::E_Protection
                                                i_eProtection );
    virtual void        do_OpenTemplate(
                            const StringVector &
                                                i_rParameters );
    virtual DYN StringVector *
                        do_Get_CurTemplateParameters();
    virtual void        do_Close_OpenTemplate();
    virtual void        do_Event_Class_FinishedBase(
                            const String  &     i_sBaseName );

    virtual void        do_Event_Store_Typedef(
                            ary::cpp::Typedef & io_rTypedef );
    virtual void        do_Event_Store_EnumValue(
                            ary::cpp::EnumValue &
                                                io_rEnumValue );
    virtual void        do_Event_Store_CppDefinition(
                            ary::cpp::DefineEntity &
                                                io_rDefinition );
    virtual void        do_Event_EnterFunction_ParameterList();
    virtual void        do_Event_Function_FinishedParameter(
                            const String  &     i_sParameterName );
    virtual void        do_Event_LeaveFunction_ParameterList();
    virtual void        do_Event_EnterFunction_Implementation();
    virtual void        do_Event_LeaveFunction_Implementation();
    virtual void        do_Event_Store_Function(
                            ary::cpp::Function &
                                                io_rFunction );
    virtual void        do_Event_Store_Variable(
                            ary::cpp::Variable &
                                                io_rVariable );
    virtual void        do_TakeDocu(
							DYN ary::doc::OldCppDocu &
                                                let_drInfo );
    virtual void        do_StartWaitingFor_Recovery();
    virtual ary::cpp::Gate &
                        inq_AryGate() const;
	virtual const ary::cpp::InputContext &
						inq_Context() const;
	virtual String		inq_CurFileName() const;
    virtual uintt       inq_LineCount() const;
    virtual bool        inq_IsWaitingFor_Recovery() const;
	virtual bool        inq_IsExternC() const;

    // Interface FileScope_EventHandler
    virtual void        do_SetCurFile(
                            ary::loc::File &    io_rCurFile );
    virtual void        do_Event_IncrLineCount();
	virtual void        do_Event_SwBracketOpen();
	virtual void        do_Event_SwBracketClose();
	virtual void        do_Event_Semicolon();

	// Local types
    struct S_FileScopeInfo;
    struct S_OwnerStack;
    struct S_DocuDistributor;
	struct S_RecoveryGuard;

    // DATA
    ary::cpp::Gate *    pGate;
	TokenProcessing_Result
						aTokenResult;
    Dyn<S_FileScopeInfo>
						pFileScopeInfo;
    Dyn<S_OwnerStack>   pOwnerStack;
    Dyn<S_DocuDistributor>
                        pDocuDistributor;
    Dyn<S_RecoveryGuard>
                        pRecoveryGuard;
};




}   // namespace cpp
#endif
