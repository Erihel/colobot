/*
 * This file is part of the Colobot: Gold Edition source code
 * Copyright (C) 2001-2015, Daniel Roux, EPSITEC SA & TerranovaTeam
 * http://epsitec.ch; http://colobot.info; http://github.com/colobot
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see http://gnu.org/licenses
 */


// Modules inlcude
#include "CBotFunction.h"

#include "CBot.h"

#include "CBotInstr/CBotBlock.h"
#include "CBotInstr/CBotTwoOpExpr.h"
#include "CBotInstr/CBotExpression.h"
#include "CBotInstr/CBotEmpty.h"
#include "CBotInstr/CBotListArray.h"

#include "CBotStack.h"
#include "CBotClass.h"
#include "CBotDefParam.h"
#include "CBotUtils.h"

// Local include

// Global include
#include <cassert>


////////////////////////////////////////////////////////////////////////////////
CBotFunction::CBotFunction()
{
    m_Param      = nullptr;            // empty parameter list
    m_Block      = nullptr;            // the instruction block
    m_next       = nullptr;            // functions can be chained
    m_bPublic    = false;           // function not public
    m_bExtern    = false;           // function not extern
    m_nextpublic = nullptr;
    m_prevpublic = nullptr;
    m_pProg      = nullptr;
//  m_nThisIdent = 0;
    m_nFuncIdent = 0;
    m_bSynchro    = false;
}

////////////////////////////////////////////////////////////////////////////////
CBotFunction* CBotFunction::m_listPublic = nullptr;

////////////////////////////////////////////////////////////////////////////////
CBotFunction::~CBotFunction()
{
    delete  m_Param;                // empty parameter list
    delete  m_Block;                // the instruction block
    delete  m_next;

    // remove public list if there is
    if ( m_bPublic )
    {
        if ( m_nextpublic != nullptr )
        {
            m_nextpublic->m_prevpublic = m_prevpublic;
        }
        if ( m_prevpublic != nullptr)
        {
            m_prevpublic->m_nextpublic = m_nextpublic;
        }
        else
        {
            // if prev = next = null may not be in the list!
            if ( m_listPublic == this ) m_listPublic = m_nextpublic;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
bool CBotFunction::IsPublic()
{
    return m_bPublic;
}

////////////////////////////////////////////////////////////////////////////////
bool CBotFunction::IsExtern()
{
    return m_bExtern;
}

////////////////////////////////////////////////////////////////////////////////
bool CBotFunction::GetPosition(int& start, int& stop, CBotGet modestart, CBotGet modestop)
{
    start = m_extern.GetStart();
    stop = m_closeblk.GetEnd();

    if (modestart == GetPosExtern)
    {
        start = m_extern.GetStart();
    }
    if (modestop == GetPosExtern)
    {
        stop = m_extern.GetEnd();
    }
    if (modestart == GetPosNom)
    {
        start = m_token.GetStart();
    }
    if (modestop == GetPosNom)
    {
        stop = m_token.GetEnd();
    }
    if (modestart == GetPosParam)
    {
        start = m_openpar.GetStart();
    }
    if (modestop == GetPosParam)
    {
        stop = m_closepar.GetEnd();
    }
    if (modestart == GetPosBloc)
    {
        start = m_openblk.GetStart();
    }
    if (modestop == GetPosBloc)
    {
        stop = m_closeblk.GetEnd();
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
CBotFunction* CBotFunction::Compile(CBotToken* &p, CBotCStack* pStack, CBotFunction* finput, bool bLocal)
{
    CBotToken*      pp;
    CBotFunction* func = finput;
    if ( func == nullptr ) func = new CBotFunction();

    CBotCStack* pStk = pStack->TokenStack(p, bLocal);

//  func->m_nFuncIdent = CBotVar::NextUniqNum();

    while (true)
    {
        if ( IsOfType(p, ID_PUBLIC) )
        {
            func->m_bPublic = true;
            continue;
        }
        pp = p;
        if ( IsOfType(p, ID_EXTERN) )
        {
            func->m_extern = pp;        // for the position of the word "extern"
            func->m_bExtern = true;
//          func->m_bPublic = true;     // therefore also public!
            continue;
        }
        break;
    }

    func->m_retToken = *p;
//  CBotClass*  pClass;
    func->m_retTyp = TypeParam(p, pStk);        // type of the result

    if (func->m_retTyp.GetType() >= 0)
    {
        CBotToken*  pp = p;
        func->m_token = *p;

        if ( IsOfType(p, ID_NOT) )
        {
            CBotToken d(CBotString("~") + p->GetString());
            func->m_token = d;
        }

        // un nom de fonction est-il là ?
        if (IsOfType(p, TokenTypVar))
        {
            if ( IsOfType( p, ID_DBLDOTS ) )        // method for a class
            {
                func->m_MasterClass = pp->GetString();
                CBotClass* pClass = CBotClass::Find(pp);
                if ( pClass == nullptr ) goto bad;

//              pp = p;
                func->m_token = *p;
                if (!IsOfType(p, TokenTypVar)) goto bad;

            }
            func->m_openpar = p;
            func->m_Param = CBotDefParam::Compile( p, pStk );
            func->m_closepar = p->GetPrev();
            if (pStk->IsOk())
            {
                pStk->SetRetType(func->m_retTyp);   // for knowledge what type returns

                if (!func->m_MasterClass.IsEmpty())
                {
                    // return "this" known
                    CBotVar* pThis = CBotVar::Create("this", CBotTypResult( CBotTypClass, func->m_MasterClass ));
                    pThis->SetInit(CBotVar::InitType::IS_POINTER);
//                  pThis->SetUniqNum(func->m_nThisIdent = -2); //CBotVar::NextUniqNum() will not
                    pThis->SetUniqNum(-2);
                    pStk->AddVar(pThis);

                    // initialize variables acording to This
                    // only saves the pointer to the first,
                    // the rest is chained
                    CBotVar* pv = pThis->GetItemList();
//                  int num = 1;
                    while (pv != nullptr)
                    {
                        CBotVar* pcopy = CBotVar::Create(pv);
//                      pcopy->SetInit(2);
                        pcopy->Copy(pv);
                        pcopy->SetPrivate(pv->GetPrivate());
//                      pcopy->SetUniqNum(pv->GetUniqNum()); //num++);
                        pStk->AddVar(pcopy);
                        pv = pv->GetNext();
                    }
                }

                // and compiles the following instruction block
                func->m_openblk = p;
                func->m_Block   = CBotBlock::Compile(p, pStk, false);
                func->m_closeblk = p->GetPrev();
                if ( pStk->IsOk() )
                {
                    if ( func->m_bPublic )  // public function, return known for all
                    {
                        CBotFunction::AddPublic(func);
                    }
                    return pStack->ReturnFunc(func, pStk);
                }
            }
        }
bad:
        pStk->SetError(TX_NOFONC, p);
    }
    pStk->SetError(TX_NOTYP, p);
    if ( finput == nullptr ) delete func;
    return pStack->ReturnFunc(nullptr, pStk);
}

////////////////////////////////////////////////////////////////////////////////
CBotFunction* CBotFunction::Compile1(CBotToken* &p, CBotCStack* pStack, CBotClass*  pClass)
{
    CBotFunction* func = new CBotFunction();
    func->m_nFuncIdent = CBotVar::NextUniqNum();

    CBotCStack* pStk = pStack->TokenStack(p, true);

    while (true)
    {
        if ( IsOfType(p, ID_PUBLIC) )
        {
        //  func->m_bPublic = true;     // will be done in two passes
            continue;
        }
        if ( IsOfType(p, ID_EXTERN) )
        {
            func->m_bExtern = true;
            continue;
        }
        break;
    }

    func->m_retToken = *p;
    func->m_retTyp = TypeParam(p, pStack);      // type of the result

    if (func->m_retTyp.GetType() >= 0)
    {
        CBotToken*  pp = p;
        func->m_token = *p;
        // un nom de fonction est-il là ?
        if (IsOfType(p, TokenTypVar))
        {
            if ( IsOfType( p, ID_DBLDOTS ) )        // method for a class
            {
                func->m_MasterClass = pp->GetString();
                CBotClass* pClass = CBotClass::Find(pp);
                if ( pClass == nullptr )
                {
                    pStk->SetError(TX_NOCLASS, pp);
                    goto bad;
                }

                pp = p;
                func->m_token = *p;
                if (!IsOfType(p, TokenTypVar)) goto bad;

            }
            func->m_Param = CBotDefParam::Compile( p, pStk );
            if (pStk->IsOk())
            {
                // looks if the function exists elsewhere
                if (( pClass != nullptr || !pStack->CheckCall(pp, func->m_Param)) &&
                    ( pClass == nullptr || !pClass->CheckCall(pp, func->m_Param)) )
                {
                    if (IsOfType(p, ID_OPBLK))
                    {
                        int level = 1;
                        // and skips the following instruction block
                        do
                        {
                            int type = p->GetType();
                            p = p->GetNext();
                            if (type == ID_OPBLK) level++;
                            if (type == ID_CLBLK) level--;
                        }
                        while (level > 0 && p != nullptr);

                        return pStack->ReturnFunc(func, pStk);
                    }
                    pStk->SetError(TX_OPENBLK, p);
                }
            }
            pStk->SetError(TX_REDEF, pp);
        }
bad:
        pStk->SetError(TX_NOFONC, p);
    }
    pStk->SetError(TX_NOTYP, p);
    delete func;
    return pStack->ReturnFunc(nullptr, pStk);
}

#ifdef  _DEBUG
static int xx = 0;
#endif

////////////////////////////////////////////////////////////////////////////////
bool CBotFunction::Execute(CBotVar** ppVars, CBotStack* &pj, CBotVar* pInstance)
{
    CBotStack*  pile = pj->AddStack(this, 2);               // one end of stack local to this function
//  if ( pile == EOX ) return true;

    pile->SetBotCall(m_pProg);                              // bases for routines

    if ( pile->GetState() == 0 )
    {
        if ( !m_Param->Execute(ppVars, pile) ) return false;    // define parameters
        pile->IncState();
    }

    if ( pile->GetState() == 1 && !m_MasterClass.IsEmpty() )
    {
        // makes "this" known
        CBotVar* pThis = nullptr;
        if ( pInstance == nullptr )
        {
            pThis = CBotVar::Create("this", CBotTypResult( CBotTypClass, m_MasterClass ));
        }
        else
        {
            pThis = CBotVar::Create("this", CBotTypResult( CBotTypPointer, m_MasterClass ));
            pThis->SetPointer(pInstance);
        }
        assert(pThis);
        pThis->SetInit(CBotVar::InitType::IS_POINTER);

//      pThis->SetUniqNum(m_nThisIdent);
        pThis->SetUniqNum(-2);
        pile->AddVar(pThis);

        pile->IncState();
    }

    if ( pile->IfStep() ) return false;

    if ( !m_Block->Execute(pile) )
    {
        if ( pile->GetError() < 0 )
            pile->SetError( 0 );
        else
            return false;
    }

    return pj->Return(pile);
}

////////////////////////////////////////////////////////////////////////////////
void CBotFunction::RestoreState(CBotVar** ppVars, CBotStack* &pj, CBotVar* pInstance)
{
    CBotStack*  pile = pj->RestoreStack(this);          // one end of stack local to this function
    if ( pile == nullptr ) return;
    CBotStack*  pile2 = pile;

    pile->SetBotCall(m_pProg);                          // bases for routines

    if ( pile->GetBlock() < 2 )
    {
        CBotStack*  pile2 = pile->RestoreStack(nullptr);       // one end of stack local to this function
        if ( pile2 == nullptr ) return;
        pile->SetState(pile->GetState() + pile2->GetState());
        pile2->Delete();
    }

    m_Param->RestoreState(pile2, true);                 // parameters

    if ( !m_MasterClass.IsEmpty() )
    {
        CBotVar* pThis = pile->FindVar("this");
        pThis->SetInit(CBotVar::InitType::IS_POINTER);
        pThis->SetUniqNum(-2);
    }

    m_Block->RestoreState(pile2, true);
}

////////////////////////////////////////////////////////////////////////////////
void CBotFunction::AddNext(CBotFunction* p)
{
    CBotFunction*   pp = this;
    while (pp->m_next != nullptr) pp = pp->m_next;

    pp->m_next = p;
}

////////////////////////////////////////////////////////////////////////////////
CBotTypResult CBotFunction::CompileCall(const char* name, CBotVar** ppVars, long& nIdent)
{
    nIdent = 0;
    CBotTypResult   type;

//    CBotFunction*   pt = FindLocalOrPublic(nIdent, name, ppVars, type);
    FindLocalOrPublic(nIdent, name, ppVars, type);
    return type;
}

////////////////////////////////////////////////////////////////////////////////
CBotFunction* CBotFunction::FindLocalOrPublic(long& nIdent, const char* name, CBotVar** ppVars, CBotTypResult& TypeOrError, bool bPublic)
{
    TypeOrError.SetType(TX_UNDEFCALL);      // no routine of the name
    CBotFunction*   pt;

    if ( nIdent )
    {
        if ( this != nullptr ) for ( pt = this ; pt != nullptr ; pt = pt->m_next )
        {
            if ( pt->m_nFuncIdent == nIdent )
            {
                TypeOrError = pt->m_retTyp;
                return pt;
            }
        }

        // search the list of public functions

        for ( pt = m_listPublic ; pt != nullptr ; pt = pt->m_nextpublic )
        {
            if ( pt->m_nFuncIdent == nIdent )
            {
                TypeOrError = pt->m_retTyp;
                return pt;
            }
        }
    }

    if ( name == nullptr ) return nullptr;

    int     delta   = 99999;                // seeks the lowest signature
    CBotFunction*   pFunc = nullptr;           // the best function found

    if ( this != nullptr )
    {
        for ( pt = this ; pt != nullptr ; pt = pt->m_next )
        {
            if ( pt->m_token.GetString() == name )
            {
                int i = 0;
                int alpha = 0;                          // signature of parameters
                // parameters are compatible?
                CBotDefParam* pv = pt->m_Param;         // expected list of parameters
                CBotVar* pw = ppVars[i++];              // provided list parameter
                while ( pv != nullptr && pw != nullptr)
                {
                    if (!TypesCompatibles(pv->GetTypResult(), pw->GetTypResult()))
                    {
                        if ( pFunc == nullptr ) TypeOrError = TX_BADPARAM;
                        break;
                    }
                    int d = pv->GetType() - pw->GetType(2);
                    alpha += d>0 ? d : -10*d;       // quality loss, 10 times more expensive!

                    pv = pv->GetNext();
                    pw = ppVars[i++];
                }
                if ( pw != nullptr )
                {
                    if ( pFunc != nullptr ) continue;
                    if ( TypeOrError.Eq(TX_LOWPARAM) ) TypeOrError.SetType(TX_NUMPARAM);
                    if ( TypeOrError.Eq(TX_UNDEFCALL)) TypeOrError.SetType(TX_OVERPARAM);
                    continue;                   // too many parameters
                }
                if ( pv != nullptr )
                {
                    if ( pFunc != nullptr ) continue;
                    if ( TypeOrError.Eq(TX_OVERPARAM) ) TypeOrError.SetType(TX_NUMPARAM);
                    if ( TypeOrError.Eq(TX_UNDEFCALL) ) TypeOrError.SetType(TX_LOWPARAM);
                    continue;                   // not enough parameters
                }

                if (alpha == 0)                 // perfect signature
                {
                    nIdent = pt->m_nFuncIdent;
                    TypeOrError = pt->m_retTyp;
                    return pt;
                }

                if ( alpha < delta )            // a better signature?
                {
                    pFunc = pt;
                    delta = alpha;
                }
            }
        }
    }

    if ( bPublic )
    {
        for ( pt = m_listPublic ; pt != nullptr ; pt = pt->m_nextpublic )
        {
            if ( pt->m_token.GetString() == name )
            {
                int i = 0;
                int alpha = 0;                          // signature of parameters
                // parameters sont-ils compatibles ?
                CBotDefParam* pv = pt->m_Param;         // list of expected parameters
                CBotVar* pw = ppVars[i++];              // list of provided parameters
                while ( pv != nullptr && pw != nullptr)
                {
                    if (!TypesCompatibles(pv->GetTypResult(), pw->GetTypResult()))
                    {
                        if ( pFunc == nullptr ) TypeOrError = TX_BADPARAM;
                        break;
                    }
                    int d = pv->GetType() - pw->GetType(2);
                    alpha += d>0 ? d : -10*d;       // quality loss, 10 times more expensive!

                    pv = pv->GetNext();
                    pw = ppVars[i++];
                }
                if ( pw != nullptr )
                {
                    if ( pFunc != nullptr ) continue;
                    if ( TypeOrError.Eq(TX_LOWPARAM) ) TypeOrError.SetType(TX_NUMPARAM);
                    if ( TypeOrError.Eq(TX_UNDEFCALL)) TypeOrError.SetType(TX_OVERPARAM);
                    continue;                   // to many parameters
                }
                if ( pv != nullptr )
                {
                    if ( pFunc != nullptr ) continue;
                    if ( TypeOrError.Eq(TX_OVERPARAM) ) TypeOrError.SetType(TX_NUMPARAM);
                    if ( TypeOrError.Eq(TX_UNDEFCALL) ) TypeOrError.SetType(TX_LOWPARAM);
                    continue;                   // not enough parameters
                }

                if (alpha == 0)                 // perfect signature
                {
                    nIdent = pt->m_nFuncIdent;
                    TypeOrError = pt->m_retTyp;
                    return pt;
                }

                if ( alpha < delta )            // a better signature?
                {
                    pFunc = pt;
                    delta = alpha;
                }
            }
        }
    }

    if ( pFunc != nullptr )
    {
        nIdent = pFunc->m_nFuncIdent;
        TypeOrError = pFunc->m_retTyp;
        return pFunc;
    }
    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
int CBotFunction::DoCall(long& nIdent, const char* name, CBotVar** ppVars, CBotStack* pStack, CBotToken* pToken)
{
    CBotTypResult   type;
    CBotFunction*   pt = nullptr;

    pt = FindLocalOrPublic(nIdent, name, ppVars, type);

    if ( pt != nullptr )
    {
        CBotStack*  pStk1 = pStack->AddStack(pt, 2);    // to put "this"
//      if ( pStk1 == EOX ) return true;

        pStk1->SetBotCall(pt->m_pProg);                 // it may have changed module

        if ( pStk1->IfStep() ) return false;

        CBotStack*  pStk3 = pStk1->AddStack(nullptr, true);    // parameters

        // preparing parameters on the stack

        if ( pStk1->GetState() == 0 )
        {
            if ( !pt->m_MasterClass.IsEmpty() )
            {
                CBotVar* pInstance = m_pProg->m_pInstance;
                // make "this" known
                CBotVar* pThis ;
                if ( pInstance == nullptr )
                {
                    pThis = CBotVar::Create("this", CBotTypResult( CBotTypClass, pt->m_MasterClass ));
                }
                else
                {
                    pThis = CBotVar::Create("this", CBotTypResult( CBotTypPointer, pt->m_MasterClass ));
                    pThis->SetPointer(pInstance);
                }
                assert(pThis);
                pThis->SetInit(CBotVar::InitType::IS_POINTER);

                pThis->SetUniqNum(-2);
                pStk1->AddVar(pThis);
            }

            // initializes the variables as parameters
            pt->m_Param->Execute(ppVars, pStk3);            // cannot be interrupted

            pStk1->IncState();
        }

        // finally execution of the found function

        if ( !pStk3->GetRetVar(                     // puts the result on the stack
            pt->m_Block->Execute(pStk3) ))          // GetRetVar said if it is interrupted
        {
            if ( !pStk3->IsOk() && pt->m_pProg != m_pProg )
            {
#ifdef _DEBUG
                if ( m_pProg->GetFunctions()->GetName() == "LaCommande" ) return false;
#endif
                pStk3->SetPosError(pToken);         // indicates the error on the procedure call
            }
            return false;   // interrupt !
        }

        return pStack->Return( pStk3 );
    }
    return -1;
}

////////////////////////////////////////////////////////////////////////////////
void CBotFunction::RestoreCall(long& nIdent, const char* name, CBotVar** ppVars, CBotStack* pStack)
{
    CBotTypResult   type;
    CBotFunction*   pt = nullptr;
    CBotStack*      pStk1;
    CBotStack*      pStk3;

    // search function to return the ok identifier

    pt = FindLocalOrPublic(nIdent, name, ppVars, type);

    if ( pt != nullptr )
    {
        pStk1 = pStack->RestoreStack(pt);
        if ( pStk1 == nullptr ) return;

        pStk1->SetBotCall(pt->m_pProg);                 // it may have changed module

        if ( pStk1->GetBlock() < 2 )
        {
            CBotStack* pStk2 = pStk1->RestoreStack(nullptr); // used more
            if ( pStk2 == nullptr ) return;
            pStk3 = pStk2->RestoreStack(nullptr);
            if ( pStk3 == nullptr ) return;
        }
        else
        {
            pStk3 = pStk1->RestoreStack(nullptr);
            if ( pStk3 == nullptr ) return;
        }

        // preparing parameters on the stack

        {
            if ( !pt->m_MasterClass.IsEmpty() )
            {
//                CBotVar* pInstance = m_pProg->m_pInstance;
                // make "this" known
                CBotVar* pThis = pStk1->FindVar("this");
                pThis->SetInit(CBotVar::InitType::IS_POINTER);
                pThis->SetUniqNum(-2);
            }
        }

        if ( pStk1->GetState() == 0 )
        {
            pt->m_Param->RestoreState(pStk3, true);
            return;
        }

        // initializes the variables as parameters
        pt->m_Param->RestoreState(pStk3, false);
        pt->m_Block->RestoreState(pStk3, true);
    }
}

////////////////////////////////////////////////////////////////////////////////
int CBotFunction::DoCall(long& nIdent, const char* name, CBotVar* pThis, CBotVar** ppVars, CBotStack* pStack, CBotToken* pToken, CBotClass* pClass)
{
    CBotTypResult   type;
    CBotProgram*    pProgCurrent = pStack->GetBotCall();

    CBotFunction*   pt = FindLocalOrPublic(nIdent, name, ppVars, type, false);

    if ( pt != nullptr )
    {
//      DEBUG( "CBotFunction::DoCall" + pt->GetName(), 0, pStack);

        CBotStack*  pStk = pStack->AddStack(pt, 2);
//      if ( pStk == EOX ) return true;

        pStk->SetBotCall(pt->m_pProg);                  // it may have changed module
        CBotStack*  pStk3 = pStk->AddStack(nullptr, true); // to set parameters passed

        // preparing parameters on the stack

        if ( pStk->GetState() == 0 )
        {
            // sets the variable "this" on the stack
            CBotVar* pthis = CBotVar::Create("this", CBotTypNullPointer);
            pthis->Copy(pThis, false);
            pthis->SetUniqNum(-2);      // special value
            pStk->AddVar(pthis);

            CBotClass*  pClass = pThis->GetClass()->GetParent();
            if ( pClass )
            {
                // sets the variable "super" on the stack
                CBotVar* psuper = CBotVar::Create("super", CBotTypNullPointer);
                psuper->Copy(pThis, false); // in fact identical to "this"
                psuper->SetUniqNum(-3);     // special value
                pStk->AddVar(psuper);
            }
            // initializes the variables as parameters
            pt->m_Param->Execute(ppVars, pStk3);            // cannot be interrupted
            pStk->IncState();
        }

        if ( pStk->GetState() == 1 )
        {
            if ( pt->m_bSynchro )
            {
                CBotProgram* pProgBase = pStk->GetBotCall(true);
                if ( !pClass->Lock(pProgBase) ) return false;       // expected to power \TODO attend de pouvoir
            }
            pStk->IncState();
        }
        // finally calls the found function

        if ( !pStk3->GetRetVar(                         // puts the result on the stack
            pt->m_Block->Execute(pStk3) ))          // GetRetVar said if it is interrupted
        {
            if ( !pStk3->IsOk() )
            {
                if ( pt->m_bSynchro )
                {
                    pClass->Unlock();                   // release function
                }

                if ( pt->m_pProg != pProgCurrent )
                {
                    pStk3->SetPosError(pToken);         // indicates the error on the procedure call
                }
            }
            return false;   // interrupt !
        }

        if ( pt->m_bSynchro )
        {
            pClass->Unlock();                           // release function
        }

        return pStack->Return( pStk3 );
    }
    return -1;
}

////////////////////////////////////////////////////////////////////////////////
void CBotFunction::RestoreCall(long& nIdent, const char* name, CBotVar* pThis, CBotVar** ppVars, CBotStack* pStack, CBotClass* pClass)
{
    CBotTypResult   type;
    CBotFunction*   pt = FindLocalOrPublic(nIdent, name, ppVars, type);

    if ( pt != nullptr )
    {
        CBotStack*  pStk = pStack->RestoreStack(pt);
        if ( pStk == nullptr ) return;
        pStk->SetBotCall(pt->m_pProg);                  // it may have changed module

        CBotVar*    pthis = pStk->FindVar("this");
        pthis->SetUniqNum(-2);

        CBotStack*  pStk3 = pStk->RestoreStack(nullptr);   // to set parameters passed
        if ( pStk3 == nullptr ) return;

        pt->m_Param->RestoreState(pStk3, true);                 // parameters

        if ( pStk->GetState() > 1 &&                        // latching is effective?
             pt->m_bSynchro )
            {
                CBotProgram* pProgBase = pStk->GetBotCall(true);
                pClass->Lock(pProgBase);                    // locks the class
            }

        // finally calls the found function

        pt->m_Block->RestoreState(pStk3, true);                 // interrupt !
    }
}

////////////////////////////////////////////////////////////////////////////////
bool CBotFunction::CheckParam(CBotDefParam* pParam)
{
    CBotDefParam*   pp = m_Param;
    while ( pp != nullptr && pParam != nullptr )
    {
        CBotTypResult type1 = pp->GetType();
        CBotTypResult type2 = pParam->GetType();
        if ( !type1.Compare(type2) ) return false;
        pp = pp->GetNext();
        pParam = pParam->GetNext();
    }
    return ( pp == nullptr && pParam == nullptr );
}

////////////////////////////////////////////////////////////////////////////////
CBotString CBotFunction::GetName()
{
    return  m_token.GetString();
}

////////////////////////////////////////////////////////////////////////////////
CBotString CBotFunction::GetParams()
{
    if ( m_Param == nullptr ) return CBotString("()");

    CBotString      params = "( ";
    CBotDefParam*   p = m_Param;        // list of parameters

    while (p != nullptr)
    {
        params += p->GetParamString();
        p = p->GetNext();
        if ( p != nullptr ) params += ", ";
    }

    params += " )";
    return params;
}

////////////////////////////////////////////////////////////////////////////////
CBotFunction* CBotFunction::Next()
{
    return  m_next;
}

////////////////////////////////////////////////////////////////////////////////
void CBotFunction::AddPublic(CBotFunction* func)
{
    if ( m_listPublic != nullptr )
    {
        func->m_nextpublic = m_listPublic;
        m_listPublic->m_prevpublic = func;
    }
    m_listPublic = func;
}
