/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef J9_IL_INCL
#define J9_IL_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_IL_CONNECTOR
#define J9_IL_CONNECTOR
namespace J9 { class IL; }
namespace J9 { typedef J9::IL ILConnector; }
#endif

#include "il/OMRIL.hpp"

#include "infra/Annotations.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"

namespace J9
{

class OMR_EXTENSIBLE IL : public OMR::ILConnector
   {

   public:

   static TR::ILOpCodes opCodesForConst[];
   static TR::ILOpCodes opCodesForDirectLoad[];
   static TR::ILOpCodes opCodesForDirectStore[];
   static TR::ILOpCodes opCodesForIndirectLoad[];
   static TR::ILOpCodes opCodesForIndirectStore[];
   static TR::ILOpCodes opCodesForIndirectArrayLoad[];
   static TR::ILOpCodes opCodesForIndirectArrayStore[];
   static TR::ILOpCodes opCodesForRegisterLoad[];
   static TR::ILOpCodes opCodesForRegisterStore[];
   static TR::ILOpCodes opCodesForCompareEquals[];
   static TR::ILOpCodes opCodesForCompareNotEquals[];

   TR::ILOpCodes opCodeForCorrespondingIndirectLoad(TR::ILOpCodes loadOpCode);
   TR::ILOpCodes opCodeForCorrespondingIndirectStore(TR::ILOpCodes storeOpCode);

   TR::ILOpCodes opCodeForConst(TR::DataType dt);
   TR::ILOpCodes opCodeForDirectLoad(TR::DataType dt);
   TR::ILOpCodes opCodeForDirectReadBarrier(TR::DataType dt);
   TR::ILOpCodes opCodeForDirectStore(TR::DataType dt);
   TR::ILOpCodes opCodeForDirectWriteBarrier(TR::DataType dt);
   TR::ILOpCodes opCodeForIndirectLoad(TR::DataType dt);
   TR::ILOpCodes opCodeForIndirectReadBarrier(TR::DataType dt);
   TR::ILOpCodes opCodeForIndirectStore(TR::DataType dt);
   TR::ILOpCodes opCodeForIndirectWriteBarrier(TR::DataType dt);
   TR::ILOpCodes opCodeForIndirectArrayLoad(TR::DataType dt);
   TR::ILOpCodes opCodeForIndirectArrayStore(TR::DataType dt);
   TR::ILOpCodes opCodeForRegisterLoad(TR::DataType dt);
   TR::ILOpCodes opCodeForRegisterStore(TR::DataType dt);
   TR::ILOpCodes opCodeForCompareEquals(TR::DataType dt);
   TR::ILOpCodes opCodeForCompareNotEquals(TR::DataType dt);

   };

}

#endif
