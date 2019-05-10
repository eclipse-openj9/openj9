
/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#include "rpc/gen/compile.pb.h"

#ifndef PROTOBUF_WRAPPERS_H
#define PROTOBUF_WRAPPERS_H

#include "j9.h"
#include "env/jittypes.h"
#include "codegen/RecognizedMethods.hpp"


using namespace google::protobuf;
class TR_ResolvedJ9Method;

namespace JITaaS
{
struct
TR_ResolvedMethodInfoWrapper
   {
   TR_ResolvedMethodInfoWrapper()
      {
      }

   TR_ResolvedMethodInfoWrapper(const ResolvedMethodInfo &serializedInfo)
      {
      deserialize(serializedInfo);
      }

   void deserialize(const ResolvedMethodInfo &serializedInfo)
      {
      remoteMirror = (TR_ResolvedJ9Method *) serializedInfo.remotemirror();
      literals = (J9RAMConstantPoolItem *) serializedInfo.literals();
      ramClass = (J9Class *) serializedInfo.ramclass();
      methodIndex = serializedInfo.methodindex();
      jniProperties = (uintptrj_t) serializedInfo.jniproperties();
      jniTargetAddress = (void *) serializedInfo.jnitargetaddress();
      isInterpreted = serializedInfo.isinterpreted();
      isJNINative = serializedInfo.isjninative();
      isMethodInValidLibrary = serializedInfo.ismethodinvalidlibrary();
      mandatoryRm = (TR::RecognizedMethod) serializedInfo.mandatoryrm();
      rm = (TR::RecognizedMethod) serializedInfo.rm();
      startAddressForJittedMethod = (void *) serializedInfo.startaddressforjittedmethod();
      virtualMethodIsOverridden = serializedInfo.virtualmethodisoverridden();
      addressContainingIsOverriddenBit = (void *) serializedInfo.addresscontainingisoverriddenbit();
      classLoader = (J9ClassLoader *) serializedInfo.classloader();
      bodyInfoStr = serializedInfo.bodyinfostr();
      methodInfoStr = serializedInfo.methodinfostr();
      entryStr = serializedInfo.entrystr();
      }

   void serialize(ResolvedMethodInfo *serializedInfo) const
      {
      serializedInfo->set_remotemirror((uint64) remoteMirror);
      serializedInfo->set_literals((uint64) literals);
      serializedInfo->set_ramclass((uint64) ramClass);
      serializedInfo->set_methodindex(methodIndex);
      serializedInfo->set_jniproperties((uint64) jniProperties);
      serializedInfo->set_jnitargetaddress((uint64) jniTargetAddress);
      serializedInfo->set_isinterpreted(isInterpreted);
      serializedInfo->set_isjninative(isJNINative);
      serializedInfo->set_ismethodinvalidlibrary(isMethodInValidLibrary);
      serializedInfo->set_mandatoryrm((int32) mandatoryRm);
      serializedInfo->set_rm((int32) rm);
      serializedInfo->set_startaddressforjittedmethod((uint64) startAddressForJittedMethod);
      serializedInfo->set_virtualmethodisoverridden(virtualMethodIsOverridden);
      serializedInfo->set_addresscontainingisoverriddenbit((uint64) addressContainingIsOverriddenBit);
      serializedInfo->set_classloader((uint64) classLoader);
      serializedInfo->set_bodyinfostr(bodyInfoStr);
      serializedInfo->set_methodinfostr(methodInfoStr);
      serializedInfo->set_entrystr(entryStr);
      }

   TR_ResolvedJ9Method *remoteMirror;
   J9RAMConstantPoolItem *literals;
   J9Class *ramClass;
   uint64_t methodIndex;
   uintptrj_t jniProperties;
   void *jniTargetAddress;
   bool isInterpreted;
   bool isJNINative;
   bool isMethodInValidLibrary;
   TR::RecognizedMethod mandatoryRm;
   TR::RecognizedMethod rm;
   void *startAddressForJittedMethod;
   bool virtualMethodIsOverridden;
   void *addressContainingIsOverriddenBit;
   J9ClassLoader *classLoader;
   std::string bodyInfoStr;
   std::string methodInfoStr;
   std::string entryStr;
   };
}
#endif // PROTOBUF_WRAPPERS_H
