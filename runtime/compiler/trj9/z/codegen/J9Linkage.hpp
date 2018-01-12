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

#ifndef J9_Z_LINKAGE_INCL
#define J9_Z_LINKAGE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_LINKAGE_CONNECTOR
#define J9_LINKAGE_CONNECTOR
namespace J9 { namespace Z { class Linkage; } }
namespace J9 { typedef J9::Z::Linkage LinkageConnector; }
#endif

#include "codegen/OMRLinkage.hpp"

/***********************************************************************************
 * In regular extensible classes, an architecture specialization should not        *
 * inherit from the OMR layer directly. Normally, it is the general version of the *
 * class that inherits from the OMR specialization.                                *
 *                                                                                 *
 *     OMR:: <--- OMR::Z:: <--- J9:: <--- J9::Z::                                  *
 *                                                                                 *
 * However, in this case, each specialization in the OMR layer specifies different *
 * constructors. So, creating a common implementation in this project is not       *
 * possible. Instead, specializations have to inherit directly from the OMR layer. *
 *                                                                                 *
 *     OMR:: <--- OMR::Z:: <--- J9::Z::                                            *
 *                                                                                 *
 ***********************************************************************************/

namespace J9
{
    
namespace Z
{

class OMR_EXTENSIBLE Linkage : public OMR::LinkageConnector
   {
   public:
      
   Linkage(TR::CodeGenerator * codeGen,TR_S390LinkageConventions elc, TR_LinkageConventions lc)
     : OMR::LinkageConnector(codeGen,elc,lc) {}
         
   TR::Instruction *loadUpArguments(TR::Instruction * cursor);
   };

} // namespace Z

} // namespace J9

#endif // J9_Z_LINKAGE_INCL
