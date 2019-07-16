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

#ifndef J9_METHOD_INCL
#define J9_METHOD_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_METHOD_CONNECTOR
#define J9_METHOD_CONNECTOR
namespace J9 { class Method; }
namespace J9 { typedef J9::Method MethodConnector; }
#endif

#include "compile/OMRMethod.hpp"


namespace J9
{

class Method : public OMR::MethodConnector
   {
   public:

   Method(Type t = J9) :
      OMR::MethodConnector(t) {}

   };

}

#endif
