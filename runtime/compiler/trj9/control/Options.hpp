/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef TR_OPTIONS_INCL
#define TR_OPTIONS_INCL

#include "control/J9Options.hpp"

namespace TR { class Options; }

namespace TR
{

class OMR_EXTENSIBLE Options : public J9::OptionsConnector
{
public:

   Options() : J9::OptionsConnector() {}

   Options(TR_Memory * m,
           int32_t index,
           int32_t lineNumber,
           TR_ResolvedMethod *compilee,
           void *oldStartPC,
           TR_OptimizationPlan *optimizationPlan,
           bool isAOT=false,
           int32_t compThreadID=-1)
      : J9::OptionsConnector(m,index,lineNumber,compilee,oldStartPC,optimizationPlan,isAOT,compThreadID)
      {}

   Options(TR::Options &other) : J9::OptionsConnector(other) {}

};

}

#endif
