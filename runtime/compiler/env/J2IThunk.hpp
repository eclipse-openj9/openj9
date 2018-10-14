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

#ifndef TR_J2ITHUNK_INCL
#define TR_J2ITHUNK_INCL

#include "infra/Array.hpp"
#include "env/TRMemory.hpp"
#include "env/IO.hpp"

class TR_J2IThunkTable;
namespace TR { class CodeGenerator; }
namespace TR { class Monitor; }

class TR_J2IThunk
   {
   int16_t _totalSize;
   int16_t _codeSize;

   public:

   static TR_J2IThunk *allocate(int16_t codeSize, char *signature, TR::CodeGenerator *cg, TR_J2IThunkTable *thunkTable);

   static TR_J2IThunk *get(uint8_t *entryPoint)
      { return ((TR_J2IThunk*)entryPoint)-1; }

   int16_t totalSize() { return _totalSize; }
   int16_t codeSize() { return _codeSize; }
   uint8_t *entryPoint() { return (uint8_t*)(this+1); }
   char *terseSignature() { return (char*)(entryPoint() + codeSize()); }
   };

class TR_J2IThunkTable
   {
   enum TypeChars
      {
      TC_VOID,
      TC_INT,
      TC_LONG,
      TC_FLOAT,
      TC_DOUBLE,
      TC_REFERENCE,

      NUM_TYPE_CHARS
      };

   static int32_t typeCharIndex(char typeChar)
      {
      switch (typeChar)
         {
         case 'V': return TC_VOID;
         case 'I': return TC_INT;
         case 'J': return TC_LONG;
         case 'F': return TC_FLOAT;
         case 'D': return TC_DOUBLE;
         case 'L': return TC_REFERENCE;
         default:
            TR_ASSERT(0, "Unknown type char '%c'", typeChar);
            return -1;
         }
      }

   struct Node
      {
      typedef int32_t ChildIndex; // sad -- how many nodes are there really going to be in this array?  uint8_t would suffice for some workloads!

      TR_J2IThunk *_thunk;
      ChildIndex _children[NUM_TYPE_CHARS];

      Node *get(char *terseSignature, TR_PersistentArray<Node> &nodeArray, bool createIfMissing);

      void dumpTo(TR_FrontEnd *fe, TR::FILE *file, TR_PersistentArray<Node> &nodeArray, int indent);
      };

   private: // Fields

   char *_name;
   TR::Monitor *_monitor;
   TR_PersistentArray<Node> _nodes;

   public: // API

   // FUN FACT: signature strings don't need to be null-terminated.  This class
   // can always tell where the end is from the Java signature string format,
   // and does not rely on null-termination.

   TR_J2IThunk *findThunk(char *signature, TR_FrontEnd *frontend, bool isForCurrentRun=false); // may return NULL
   TR_J2IThunk *getThunk (char *signature, TR_FrontEnd *frontend, bool isForCurrentRun=false); // same as findThunk but asserts !NULL

   // Note: the 'isForCurrentRun' flag is meant for AOT-aware code.
   // If you don't know what this should be, you probably want to use its default value.
   //
   void addThunk(TR_J2IThunk *thunk, TR_FrontEnd *fe, bool isForCurrentRun=false);

   char terseTypeChar(char *type);
   int16_t terseSignatureLength(char *signature);
   void getTerseSignature(char *buf, int16_t bufLength, char *signature);

   TR_J2IThunkTable(TR_PersistentMemory *m, char *name);
   TR_PERSISTENT_ALLOC(TR_Memory::JSR292)

   void dumpTo(TR_FrontEnd *fe, TR::FILE *file);

   private:

   Node *root() { return &_nodes[0]; }
   TR_J2IThunk *findThunkFromTerseSignature(char *terseSignature, TR_FrontEnd *frontend, bool isForCurrentRun);
   };

#endif
