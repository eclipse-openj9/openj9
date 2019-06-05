/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#ifndef J9WATCHEDSTATICFIELDSNIPPET_INCL
#define J9WATCHEDSTATICFIELDSNIPPET_INCL

#include "codegen/Snippet.hpp"
#include "codegen/CodeGenerator.hpp"

namespace TR {

class J9WatchedStaticFieldSnippet : public TR::Snippet
   {
   private :

   J9JITWatchedStaticFieldData staticFieldData;

   public :

   J9WatchedStaticFieldSnippet(TR::CodeGenerator *cg, TR::Node *node, J9Method *m, UDATA loc, void *fieldAddress, J9Class *fieldClass);

   J9Method *getMethod() { return staticFieldData.method; }
   UDATA getLocation() { return staticFieldData.location; }
   void *getFieldAddress() { return staticFieldData.fieldAddress; }
   J9Class *getFieldClass() { return staticFieldData.fieldClass; }

   virtual uint8_t *emitSnippetBody();
   virtual uint32_t getLength(int32_t val) { return sizeof(J9JITWatchedStaticFieldData); }

   virtual void print(TR::FILE *pOutFile, TR_Debug *debug);
   };
}

#endif
