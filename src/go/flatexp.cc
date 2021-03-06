/*
 *  Copyright 2004-2006 Adrian Thurston <thurston@complang.org>
 *            2004 Erich Ocean <eric.ocean@ampede.com>
 *            2005 Alan West <alan@alanz.com>
 */

/*  This file is part of Ragel.
 *
 *  Ragel is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 *  Ragel is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with Ragel; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */

#include "ragel.h"
#include "flatexp.h"
#include "redfsm.h"
#include "gendata.h"

namespace Go {

void FlatExpanded::tableDataPass()
{
    taKeys();
    taKeySpans();
    taFlatIndexOffset();

    taIndicies();
    taTransCondSpaces();
    taTransOffsets();
    taTransLengths();
    taCondKeys();
    taCondTargs();
    taCondActions();

    taToStateActions();
    taFromStateActions();
    taEofActions();
    taEofTrans();
}

void FlatExpanded::genAnalysis()
{
    redFsm->sortByStateId();

    /* Choose default transitions and the single transition. */
    redFsm->chooseDefaultSpan();

    /* Do flat expand. */
    redFsm->makeFlat();

    /* If any errors have occured in the input file then don't write anything. */
    if ( gblErrorCount > 0 )
        return;

    /* Anlayze Machine will find the final action reference counts, among other
     * things. We will use these in reporting the usage of fsm directives in
     * action code. */
    analyzeMachine();

    setKeyType();

    /* Run the analysis pass over the table data. */
    setTableState( TableArray::AnalyzePass );
    tableDataPass();

    /* Switch the tables over to the code gen mode. */
    setTableState( TableArray::GeneratePass );
}

void FlatExpanded::TO_STATE_ACTION( RedStateAp *state )
{
    int act = 0;
    if ( state->toStateAction != 0 )
        act = state->toStateAction->actListId+1;
    toStateActions.value( act );
}

void FlatExpanded::FROM_STATE_ACTION( RedStateAp *state )
{
    int act = 0;
    if ( state->fromStateAction != 0 )
        act = state->fromStateAction->actListId+1;
    fromStateActions.value( act );
}

void FlatExpanded::EOF_ACTION( RedStateAp *state )
{
    int act = 0;
    if ( state->eofAction != 0 )
        act = state->eofAction->actListId+1;
    eofActions.value( act );
}

void FlatExpanded::COND_ACTION( RedCondAp *cond )
{
    int action = 0;
    if ( cond->action != 0 )
        action = cond->action->actListId+1;
    condActions.value( action );
}

/* Write out the function switch. This switch is keyed on the values
 * of the func index. */
std::ostream &FlatExpanded::TO_STATE_ACTION_SWITCH()
{
    /* Loop the actions. */
    for ( GenActionTableMap::Iter redAct = redFsm->actionMap; redAct.lte(); redAct++ ) {
        if ( redAct->numToStateRefs > 0 ) {
            /* Write the entry label. */
            out << "\tcase " << redAct->actListId+1 << ":\n";

            /* Write each action in the list of action items. */
            for ( GenActionTable::Iter item = redAct->key; item.lte(); item++ )
                ACTION( out, item->value, 0, false, false );
        }
    }

    genLineDirective( out );
    return out;
}

/* Write out the function switch. This switch is keyed on the values
 * of the func index. */
std::ostream &FlatExpanded::FROM_STATE_ACTION_SWITCH()
{
    /* Loop the actions. */
    for ( GenActionTableMap::Iter redAct = redFsm->actionMap; redAct.lte(); redAct++ ) {
        if ( redAct->numFromStateRefs > 0 ) {
            /* Write the entry label. */
            out << "\tcase " << redAct->actListId+1 << ":\n";

            /* Write each action in the list of action items. */
            for ( GenActionTable::Iter item = redAct->key; item.lte(); item++ )
                ACTION( out, item->value, 0, false, false );
        }
    }

    genLineDirective( out );
    return out;
}

std::ostream &FlatExpanded::EOF_ACTION_SWITCH()
{
    /* Loop the actions. */
    for ( GenActionTableMap::Iter redAct = redFsm->actionMap; redAct.lte(); redAct++ ) {
        if ( redAct->numEofRefs > 0 ) {
            /* Write the entry label. */
            out << "\tcase " << redAct->actListId+1 << ":\n";

            /* Write each action in the list of action items. */
            for ( GenActionTable::Iter item = redAct->key; item.lte(); item++ )
                ACTION( out, item->value, 0, true, false );
        }
    }

    genLineDirective( out );
    return out;
}

/* Write out the function switch. This switch is keyed on the values
 * of the func index. */
std::ostream &FlatExpanded::ACTION_SWITCH()
{
    /* Loop the actions. */
    for ( GenActionTableMap::Iter redAct = redFsm->actionMap; redAct.lte(); redAct++ ) {
        if ( redAct->numTransRefs > 0 ) {
            /* Write the entry label. */
            out << "\tcase " << redAct->actListId+1 << ":\n";

            /* Write each action in the list of action items. */
            for ( GenActionTable::Iter item = redAct->key; item.lte(); item++ )
                ACTION( out, item->value, 0, false, false );
        }
    }

    genLineDirective( out );
    return out;
}

void FlatExpanded::writeData()
{
    taKeys();
    taKeySpans();
    taFlatIndexOffset();

    taIndicies();
    taTransCondSpaces();
    taTransOffsets();
    taTransLengths();
    taCondKeys();
    taCondTargs();
    taCondActions();

    if ( redFsm->anyToStateActions() )
        taToStateActions();

    if ( redFsm->anyFromStateActions() )
        taFromStateActions();

    if ( redFsm->anyEofActions() )
        taEofActions();

    if ( redFsm->anyEofTrans() )
        taEofTrans();

    STATE_IDS();
}

void FlatExpanded::writeExec()
{
    testEofUsed = false;
    outLabelUsed = false;

    out <<
        "	{\n"
        "	var _slen";

    if ( redFsm->anyRegCurStateRef() )
        out << ", _ps";

    out << " int\n";
    out << "	var _trans, _cond int\n";

    out <<
        "	var _keys uint\n" // ALPH_TYPE array index
        "	var _inds uint\n" // indicies array index
        "	var _ckeys uint\n" // condKeys array index
        "	var _klen int\n"
        "	var _cpc int\n";

    if ( !noEnd ) {
        testEofUsed = true;
        out <<
            "	if " << P() << " == " << PE() << " {\n"
            "		goto _test_eof\n"
            "	}\n";
    }

    if ( redFsm->errState != 0 ) {
        outLabelUsed = true;
        out <<
            "	if " << vCS() << " == " << redFsm->errState->id << " {\n"
            "       goto _out\n"
            "	}\n";
    }

    out << "_resume:\n";

    if ( redFsm->anyFromStateActions() ) {
        out <<
            "	switch " << ARR_REF( fromStateActions ) << "[" << vCS() << "] {\n";
            FROM_STATE_ACTION_SWITCH() <<
            "	}\n"
            "\n";
    }

    LOCATE_TRANS();

    out << "_match_cond:\n";

    if ( redFsm->anyEofTrans() )
        out << "_eof_trans:\n";

    if ( redFsm->anyRegCurStateRef() )
        out << "	_ps = " << vCS() << "\n";

    out <<
        "	" << vCS() << " = " << "int(" << ARR_REF( condTargs ) << "[_cond])\n\n";

    if ( redFsm->anyRegActions() ) {
        out <<
            "	if " << ARR_REF( condActions ) << "[_cond] == 0 {\n"
            "		goto _again\n"
            "	}\n"
            "\n"
            "	switch " << ARR_REF( condActions ) << "[_cond] {\n";
            ACTION_SWITCH() <<
            "	}\n"
            "\n";
    }

//  if ( redFsm->anyRegActions() || redFsm->anyActionGotos() ||
//          redFsm->anyActionCalls() || redFsm->anyActionRets() )
        out << "_again:\n";

    if ( redFsm->anyToStateActions() ) {
        out <<
            "	switch " << ARR_REF( toStateActions ) << "[" << vCS() << "] {\n";
            TO_STATE_ACTION_SWITCH() <<
            "	}\n"
            "\n";
    }

    if ( redFsm->errState != 0 ) {
        outLabelUsed = true;
        out <<
            "	if " << vCS() << " == " << redFsm->errState->id << " {\n"
            "		goto _out\n"
            "	}\n";
    }

    if ( !noEnd ) {
        out <<
            "	if " << P() << "++; " << P() << " != " << PE() << " {\n"
            "       goto _resume\n"
            "	}\n";
    }
    else {
        out <<
            "	" << P() << "++\n"
            "	goto _resume\n";
    }

    if ( testEofUsed )
        out << "	_test_eof: {}\n";

    if ( redFsm->anyEofTrans() || redFsm->anyEofActions() ) {
        out <<
            "	if " << P() << " == " << vEOF() << " {\n";

        if ( redFsm->anyEofTrans() ) {
            out <<
                "	if " << ARR_REF( eofTrans ) << "[" << vCS() << "] > 0 {\n"
                "		_trans = " << "int(" << ARR_REF( eofTrans ) << "[" << vCS() << "] - 1" << ")\n"
                "		_cond = " << "int(" << ARR_REF( transOffsets ) << "[_trans]" << ")\n"
                "		goto _eof_trans\n"
                "	}\n";
        }

        if ( redFsm->anyEofActions() ) {
            out <<
                "	switch " << ARR_REF( eofActions ) << "[" << vCS() << "] {\n";
                EOF_ACTION_SWITCH() <<
                "	}\n";
        }

        out <<
            "	}\n"
            "\n";
    }

    if ( outLabelUsed )
        out << "	_out: {}\n";

    out << "	}\n";
}

}
