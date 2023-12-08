/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "env/J2IThunk.hpp"

#include "env/CompilerEnv.hpp"
#include "codegen/CodeGenerator.hpp"
#include "infra/Array.hpp"
#include "infra/Monitor.hpp"
#include "env/IO.hpp"
#include "infra/CriticalSection.hpp"
#include "env/j9method.h"
#include "env/VMJ9.h"
#include "env/VerboseLog.hpp"

static int32_t
computeSignatureLength(char *signature)
   {
   char *currentArgument;
   for (currentArgument = signature+1; currentArgument[0] != ')'; currentArgument = nextSignatureArgument(currentArgument))
      {}
   currentArgument = nextSignatureArgument(currentArgument+1);
   return currentArgument - signature;
   }


TR_MHJ2IThunk *
TR_MHJ2IThunk::allocate(
      int16_t codeSize,
      char *signature,
      TR::CodeGenerator *cg,
      TR_MHJ2IThunkTable *thunkTable)
   {
   int16_t terseSignatureBufLength = thunkTable->terseSignatureLength(signature)+1;
   int16_t totalSize = (int16_t)sizeof(TR_MHJ2IThunk) + codeSize + terseSignatureBufLength;
   TR_MHJ2IThunk *result;
#if defined(J9VM_OPT_JITSERVER)
   if (cg->comp()->isOutOfProcessCompilation())
      {
      // Don't need to use code cache because the entire thunk will be copied and sent to the client
      result = (TR_MHJ2IThunk*)cg->comp()->trMemory()->allocateMemory(totalSize, heapAlloc);
      }
   else
#endif /* defined(J9VM_OPT_JITSERVER) */
      {
      result = (TR_MHJ2IThunk*)cg->allocateCodeMemory(totalSize, true, false);
      }
   omrthread_jit_write_protect_disable();
   result->_codeSize  = codeSize;
   result->_totalSize = totalSize;
   thunkTable->getTerseSignature(result->terseSignature(), terseSignatureBufLength, signature);
   omrthread_jit_write_protect_enable();
   return result;
   }


TR_MHJ2IThunkTable::TR_MHJ2IThunkTable(TR_PersistentMemory *m, const char *name):
   _name(name),
   _monitor(TR::Monitor::create(name)),
   _nodes(m)
   {
   _nodes.setSize(1); // Initially just the root node
   }


int16_t
TR_MHJ2IThunkTable::terseSignatureLength(char *signature)
   {
   int16_t numArgs = 0;
   for (char *currentArgument = signature+1; currentArgument[0] != ')'; currentArgument = nextSignatureArgument(currentArgument))
      numArgs++;
   return numArgs+1; // +1 for return type char
   }


void TR_MHJ2IThunkTable::getTerseSignature(char *buf, int16_t bufLength, char *signature)
   {
   int16_t i=0;
   char *currentArgument;
   for (currentArgument = signature+1; currentArgument[0] != ')'; currentArgument = nextSignatureArgument(currentArgument))
      buf[i++] = terseTypeChar(currentArgument);
   TR_ASSERT(i <= bufLength-2, "Must have room for return type and null terminator");
   buf[i++] = terseTypeChar(currentArgument+1); // return type
   buf[i++] = 0; // null terminator
   }


char TR_MHJ2IThunkTable::terseTypeChar(char *type)
   {
   switch (type[0])
      {
      case '[':
      case 'L':
      case 'Q':
         return TR::Compiler->target.is64Bit()? 'L' : 'I';
      case 'Z':
      case 'B':
      case 'S':
      case 'C':
         return 'I';
      default:
         return type[0];
      }
   }


TR_MHJ2IThunkTable::Node *
TR_MHJ2IThunkTable::Node::get(
      char *terseSignature,
      TR_PersistentArray<Node> &nodeArray,
      bool createIfMissing)
   {
   Node *result = NULL;
   if (terseSignature[0] == 0)
      {
      // We've reached the matching node
      result = this;
      }
   else
      {
      int32_t typeIndex = typeCharIndex(terseSignature[0]);
      ChildIndex childIndex = _children[typeIndex];
      if (!childIndex && createIfMissing)
         {
         _children[typeIndex] = childIndex = nodeArray.size();
         Node emptyNode = {0};

         // CAREFUL! The following call can realloc the node array, including the
         // receiver of this very method we're in.  Don't access "this" anymore
         // in this function after this point!
         //
         nodeArray.add(emptyNode);
         }
      if (childIndex)
         result = nodeArray[childIndex].get(terseSignature+1, nodeArray, createIfMissing);
      else
         result = NULL;
      }
   return result;
   }


TR_MHJ2IThunk *
TR_MHJ2IThunkTable::findThunk(
      char *signature,
      TR_FrontEnd *fe,
      bool isForCurrentRun)
   {
   char terseSignature[260]; // 256 args + 1 return type + null terminator
   getTerseSignature(terseSignature, sizeof(terseSignature), signature);
   return findThunkFromTerseSignature(terseSignature, fe, isForCurrentRun);
   }


TR_MHJ2IThunk *
TR_MHJ2IThunkTable::getThunk(char *signature, TR_FrontEnd *fe, bool isForCurrentRun)
   {
   TR_MHJ2IThunk *result = findThunk(signature, fe, isForCurrentRun);
   if (!result)
      {
      char terseSignature[260]; // 256 args + 1 return type + null terminator
      dumpTo(fe, TR::IO::Stderr);
      getTerseSignature(terseSignature, sizeof(terseSignature), signature);
      trfprintf(TR::IO::Stderr, "\nERROR: Failed to find J2I thunk for %s signature %.*s\n", terseSignature, computeSignatureLength(signature), signature);
      TR_ASSERT(result != NULL, "Expected a J2I thunk for %s signature %.*s", terseSignature, computeSignatureLength(signature), signature);
      }
   return result;
   }


TR_MHJ2IThunk *
TR_MHJ2IThunkTable::findThunkFromTerseSignature(
      char *terseSignature,
      TR_FrontEnd *fe,
      bool isForCurrentRun)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe);
   TR_MHJ2IThunk *returnThunk = NULL;

   if (fej9->isAOT_DEPRECATED_DO_NOT_USE() && !isForCurrentRun)
      {
      // Must use persistent thunks for compiles that will persist
      return (TR_MHJ2IThunk *)fej9->findPersistentMHJ2IThunk(terseSignature);
      }
   else
      {
      OMR::CriticalSection critialSection(_monitor);

      Node *match = root()->get(terseSignature, _nodes, false);
      returnThunk = match ? match->_thunk : NULL;
      }

   return returnThunk;
   }


void
TR_MHJ2IThunkTable::addThunk(
      TR_MHJ2IThunk *thunk,
      TR_FrontEnd *fe, bool
      isForCurrentRun)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe);

   if (fej9->isAOT_DEPRECATED_DO_NOT_USE() && !isForCurrentRun)
      {
      // Must use persistent thunks for compiles that will persist
      fej9->persistMHJ2IThunk(thunk);
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseJ2IThunks))
         TR_VerboseLog::writeLineLocked(TR_Vlog_J2I,"persist %s @%p", thunk->terseSignature(), thunk);
      }
   else
      {
      OMR::CriticalSection criticalSection(_monitor);

      Node *match = root()->get(thunk->terseSignature(), _nodes, true);
      match->_thunk = thunk;

      // This assume must be in the monitor or else another thread could break it
      TR_ASSERT(findThunkFromTerseSignature(thunk->terseSignature(), fe, isForCurrentRun) == thunk, "TR_MHJ2IThunkTable: setThunk must cause findThunk(%s) to return %p", thunk->terseSignature(), thunk);
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseJ2IThunks))
         TR_VerboseLog::writeLineLocked(TR_Vlog_J2I,"add %s @%p", thunk->terseSignature(), thunk);
      }
   }


void
TR_MHJ2IThunkTable::Node::dumpTo(
      TR_FrontEnd *fe,
      TR::FILE *file,
      TR_PersistentArray<Node> &nodeArray,
      int indent)
   {
   static const char typeChars[] = "VIJFDL";
   if (_thunk)
      trfprintf(file, " %s @%p\n", _thunk->terseSignature(), _thunk);
   else
      trfprintf(file, "\n");
   for (int32_t typeIndex = 0; typeIndex < NUM_TYPE_CHARS; typeIndex++)
      {
      if (_children[typeIndex])
         {
         trfprintf(file, "%*s%c @%d:", indent*3, "", typeChars[typeIndex], _children[typeIndex]);
         nodeArray[_children[typeIndex]].dumpTo(fe, file, nodeArray, indent+1);
         }
      }
   }


void
TR_MHJ2IThunkTable::dumpTo(TR_FrontEnd *fe, TR::FILE *file)
   {
   OMR::CriticalSection criticalSection(_monitor);
   trfprintf(file, "J2IThunkTable \"%s\":", _name);
   root()->dumpTo(fe, file, _nodes, 1);
   }
