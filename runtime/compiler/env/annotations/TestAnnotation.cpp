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

#include "TestAnnotation.hpp"
#include "il/Symbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "il/SymbolReference.hpp"
#include "compile/ResolvedMethod.hpp"

// only purpose of this class is to test that annotations are working.  Will
// expect specific values
TR_TestAnnotation::TR_TestAnnotation(TR::Compilation *comp,TR::SymbolReference *symRef):
  TR_AnnotationBase(comp)
{
  TR_ASSERT(symRef,"symref NULL\n");
  TR_ASSERT(comp,"comp NULL\n");

  _isValid=false;
  TR::Symbol *sym = symRef->getSymbol();
 

  TR_ResolvedMethod *resolved = symRef->getOwningMethod(_comp);
  J9Class * clazz = (J9Class *)resolved->containingClass();

  if(!loadAnnotation(clazz,kTestAnnotation)) return;
  
  int32_t *intptr;
  float *fltptr;
  double *dblptr;
  int64_t *longptr;
 
  J9SRP *strPtr;
  if(getValue(symRef,"intField",kInt,&intptr))
     {
     printf("Found int value %d\n",*intptr);
     }
   if(getValue(symRef,"floatField",kFloat,&fltptr))
     {
     printf("Found float value %f\n",*fltptr);
     }
   if(getValue(symRef,"booleanField",kBool,&intptr))
     {
     printf("Found boolean value %d\n",*intptr);
     }
   if(getValue(symRef,"doubleField",kDouble,&dblptr))
     {
     printf("Found dbl value %e\n",*dblptr);
     }
   if(getValue(symRef,"charField",kChar,&intptr))
     {
     printf("Found char value %d\n",*intptr);
     }
   if(getValue(symRef,"shortField",kShort,&intptr))
     {
     printf("Found short value %d\n",*intptr);
     }
   if(getValue(symRef,"byteField",kByte,&intptr))
     {
     printf("Found byte value %d\n",*intptr);
     }
   if(getValue(symRef,"longField",kLong,&longptr))
     {
     printf("Found byte value %lld\n",*longptr);
     }
   char *enumerationName=NULL, *enumerationValue=NULL;
   int32_t nameLen,valueLen;
   if(getEnumeration(symRef,"enumField",&enumerationName,&nameLen,&enumerationValue,&valueLen))
      {
      char buf1[200],buf2[200];
      strncpy(buf1,enumerationName,nameLen);
      strncpy(buf2,enumerationValue,valueLen);
      buf1[nameLen] = '\0';
      buf2[valueLen] = '\0';
      printf("Found enumerations %s %s\n",buf1,buf2);
   }
  if(getValue(symRef,"stringField",kString,&strPtr))
     {
     J9UTF8 *description = (J9UTF8 *)SRP_PTR_GET(strPtr,J9UTF8*);
     char buf[100];
     int32_t len;
     char *nm = utf8Data(description,len);
     strncpy(buf,nm,len);
     buf[len] = '\0';
     printf("Found string %s\n",buf);
     }
    
  
  _isValid=true;
}


