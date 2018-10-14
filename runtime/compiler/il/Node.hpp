/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#ifndef TR_NODE_INCL
#define TR_NODE_INCL

#include "il/J9Node.hpp"

#include <stddef.h>               // for NULL
#include <stdint.h>               // for uint16_t
#include "il/ILOpCodes.hpp"       // for ILOpCodes
#include "infra/Annotations.hpp"  // for OMR_EXTENSIBLE

namespace TR
{

class OMR_EXTENSIBLE Node : public J9::NodeConnector
{

public:

   Node()
      : J9::NodeConnector()
      {}

   Node(TR::Node *originatingByteCodeNode, TR::ILOpCodes op,
        uint16_t numChildren)
      : J9::NodeConnector(originatingByteCodeNode, op, numChildren)
      {}

   Node(Node *from, uint16_t numChildren = 0)
      : J9::NodeConnector(from, numChildren)
      {}

   Node(Node& from)
      : J9::NodeConnector(from)
      {}

};

}

#endif

