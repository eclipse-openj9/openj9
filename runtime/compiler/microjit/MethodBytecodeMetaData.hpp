/*******************************************************************************
 * Copyright (c) 2022, 2022 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef MJIT_METHOD_BYTECODE_META_DATA
#define MJIT_METHOD_BYTECODE_META_DATA

#include "ilgen/J9ByteCodeIterator.hpp"
#include "microjit/ByteCodeIterator.hpp"
#include "microjit/SideTables.hpp"


namespace MJIT {

#if defined(J9VM_OPT_MJIT_Standalone)
class InternalMetaData{
public:
    MJIT::LocalTable _localTable;
    InternalMetaData(MJIT::LocalTable table)
        :_localTable(table)
    {}
};
#else
class InternalMetaData{
public:
    MJIT::LocalTable _localTable;
    TR::CFG *_cfg;
    uint16_t _maxCalleeArgSize;
    InternalMetaData(MJIT::LocalTable table, TR::CFG *cfg, uint16_t maxCalleeArgSize)
        :_localTable(table)
        ,_cfg(cfg)
        ,_maxCalleeArgSize(maxCalleeArgSize)
        {}
};
#endif

MJIT::InternalMetaData
createInternalMethodMetadata(
    MJIT::ByteCodeIterator *bci,
    MJIT::LocalTableEntry *localTableEntries,
    U_16 entries,
    int32_t offsetToFirstLocal,
    TR_ResolvedMethod *compilee,
    TR::Compilation *comp,
    ParamTable* paramTable,
    uint8_t pointerSize,
    bool *resolvedAllCallees
);

}

#endif /* MJIT_METHOD_BYTECODE_META_DATA */