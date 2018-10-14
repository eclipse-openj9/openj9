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

#include "env/annotations/TROptAnnotation.hpp"
#include "il/Symbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "il/SymbolReference.hpp"
#include "compile/ResolvedMethod.hpp"


TR_OptAnnotation::TR_OptAnnotation(TR::Compilation *comp,
                                    TR_ResolvedMethod *resolvedMethod):
   TR_AnnotationBase(comp),_count(-2),_optLevel(unknownHotness)
{

  TR_ASSERT(comp,"comp NULL\n");
  const bool trace=false;

  _isValid=false;

  J9Class * clazz = (J9Class *)resolvedMethod->containingClass();

  if(!loadAnnotation(clazz,kTRJITOpt)) return;

  J9JavaVM * javaVM = ((TR_J9VMBase *)_comp->fej9())->_jitConfig->javaVM;
  PORT_ACCESS_FROM_JAVAVM(javaVM);


  char   *methodName = resolvedMethod->nameChars();
  char   *methodSig  = resolvedMethod->signatureChars();
  int32_t methodNameLen = resolvedMethod->nameLength();
  int32_t methodSigLen  = resolvedMethod->signatureLength();

  char *nameBuf = (char *)j9mem_allocate_memory(methodNameLen+methodSigLen+2 * sizeof(char), J9MEM_CATEGORY_JIT);
  if (!nameBuf)
     return;

  strncpy(nameBuf,methodName,methodNameLen);
  nameBuf[methodNameLen] = '\0';
  strncpy(nameBuf+methodNameLen+1,methodSig,methodSigLen);
  nameBuf[methodNameLen+methodSigLen+1] = '\0';
  methodName = nameBuf;
  methodSig = &(nameBuf[methodNameLen+1]);

  if(trace) printf("Method:%.*s sig:%.*s\n",methodNameLen,methodName,methodSigLen,methodSig);

  const char *annotationName = recognizedAnnotations[kTRJITOpt].name;
  J9AnnotationInfoEntry *annotationInfoEntryPtr;
  annotationInfoEntryPtr = getAnnotationInfo(_annotationInfo,ANNOTATION_TYPE_METHOD,
                                             methodName,
                                             methodSig,
                                             annotationName,
                                             clazz);

   if(NULL != nameBuf)
      j9mem_free_memory(nameBuf);

   J9SRP *j9ptr;
   if(extractValue(annotationInfoEntryPtr,"optLevel",kEnum ,&j9ptr))
      {

      J9SRP      *typeNamePtr = (J9SRP* )j9ptr++;
      J9SRP      *valueNamePtr = (J9SRP* )j9ptr;
      J9UTF8     *typeName =  (J9UTF8 *)SRP_PTR_GET(typeNamePtr,J9UTF8*);
      J9UTF8     *valueName = (J9UTF8 *)SRP_PTR_GET(valueNamePtr,J9UTF8*);
      int32_t     nameLength,valueLength;
      const char *enumerationName = utf8Data(typeName,nameLength);
      const char *enumerationValue = utf8Data(valueName,valueLength);

      if(strncmp(enumerationName,"Lx10/annotations/OptLevel;",nameLength)) return;

      if(!strncmp(enumerationValue,"WARM",valueLength))           _optLevel = warm;
      else if(!strncmp(enumerationValue,"SCORCHING",valueLength)) _optLevel = scorching;
      else if(!strncmp(enumerationValue,"NOOPT",valueLength))     _optLevel = noOpt;
      else if(!strncmp(enumerationValue,"VERYHOT",valueLength))   _optLevel = veryHot;
      else if(!strncmp(enumerationValue,"HOT",valueLength))       _optLevel = hot;
      else if(!strncmp(enumerationValue,"COLD",valueLength))      _optLevel = cold;
      if(_optLevel != unknownHotness)
         _isValid=true;
      }

  int32_t *countptr;
  if(!extractValue(annotationInfoEntryPtr,"count",kInt ,&countptr)) return;
  _isValid=true;

  _count = *countptr;
}


