/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifndef TR_METHOD_INCL
#define TR_METHOD_INCL

#include "compile/J9Method.hpp"
#include "infra/Annotations.hpp"
#include <stdint.h>

class TR_FrontEnd;
class TR_Memory;
class TR_OpaqueMethodBlock;
extern "C" { struct J9Class; }

namespace TR
{

class Method : public J9::MethodConnector
   {
public:

   Method(Type t = J9) : J9::MethodConnector(t) {}

   Method(TR_FrontEnd *trvm, TR_Memory *m, J9Class *aClazz, uintptr_t cpIndex) :
      J9::MethodConnector(trvm, m, aClazz, cpIndex) {}

   Method(TR_FrontEnd *trvm, TR_Memory *m, TR_OpaqueMethodBlock *aMethod) :
      J9::MethodConnector(trvm, m, aMethod) {}

#if defined(JITSERVER_SUPPORT)
protected:
   // To be used by JITServer.
   // Warning: some initialization must be done manually after calling this constructor
   Method() {}
#endif /* defined(JITSERVER_SUPPORT) */

   };

}

#endif

