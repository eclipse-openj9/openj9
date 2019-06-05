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

#define J9_EXTERNAL_TO_VM
#include "vmaccess.h"
#include "env/annotations/AnnotationBase.hpp"
#include "il/Symbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "il/symbol/ParameterSymbol.hpp"
#include "il/SymbolReference.hpp"
#include "env/CHTable.hpp"
#include "runtime/RuntimeAssumptions.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/CompilationThread.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/VMJ9.h"
#include "env/J9JitMemory.hpp"


TR_AnnotationBase::TR_AnnotationBase(TR::Compilation *comp ):
        _isValid(false),_comp(comp),
   _annotationSignature(kUnknownAnnotationSignature){}


/**
 * The annotation classes (which hold default values) must be loaded before they can
 * be queried.  However, the act of loading can trigger the jit, and as the jit
 * is not reentrant, we must perform any such loading outside.  Since in theory
 * the first class jitted could be annotated we must preemptively load all
 * annotation classes before the first compile.  We cannot do this when loading
 * this jit because the VM is not in a state to honour the request.  Hence, before
 * each compile we check to see if the desired annotation classes have been
 * loaded.  If not, then trigger a load.  Note that this will probably cause
 * the jit to compile any class used in the loading, and if any of these
 * methods are annotated, then their default values will be unavailable.
 * Called from compileMethod.
 */
void TR_AnnotationBase::loadExpectedAnnotationClasses(J9VMThread * vmThread)
  {
  int32_t i;

  static char * doit = feGetEnv("TR_DISABLEANNOTATIONS");
  if(NULL != doit) return;
  /*
  */
  /*
    if(((TR_JitPrivateConfig*)(jitConfig)->privateConfig)->annotationClassesAlreadyLoaded) return;
  ((TR_JitPrivateConfig*)(jitConfig)->privateConfig)->annotationClassesAlreadyLoaded=true;
  */


  J9JavaVM* javaVM = vmThread->javaVM;
  J9InternalVMFunctions * intFunc = javaVM->internalVMFunctions;
  acquireVMAccess(vmThread); // this is not the comp thread so suspension is possible
  for(i=0;i < kLastAnnotationSignature;++i)
     {
     // Strip off 'L' prefix and ';' suffix
     const char *name = recognizedAnnotations[i].name;
     int32_t nameLen = recognizedAnnotations[i].nameLen;
     TR_ASSERT(strlen(name) == nameLen,"Table entry for %s is %d but should be %d\n",name,nameLen,strlen(name));
     J9Class * theClass=intFunc->internalFindClassUTF8(vmThread,(U_8 *)(name+1), nameLen-2, javaVM->systemClassLoader, 0);

     // Note: this class is cached.  Because it's being loaded from the system classloader, it is
     // impossible for it to be unloaded, hence safe to cache.
     recognizedAnnotations[i].clazz = theClass;
     if(false)printf("**** >loading>%s< len:%d %s\n",name+1,nameLen-2,theClass==NULL?"UNLOADED":"loaded");
     }
  releaseVMAccess(vmThread);

}

/*
 * Scan the given class for recognized annotation classes and record this information in the persistentcallinfo
 * If the class has already been scanned, then check the result
 */
bool TR_AnnotationBase::scanForKnownAnnotationsAndRecord(TR::CompilationInfo *compInfo,J9Method *method,J9JavaVM *javaVM,TR_FrontEnd *fe)
   {
   //   J9JavaVM * javaVM = (compInfo->_jitConfig->javaVM);
   //  return false;
   TR_ASSERT(compInfo,"compinfo isnull");
   TR_ASSERT(method,"method is null");
   TR_ASSERT(javaVM,"javavm isnull");
   TR_ASSERT(fe,"fe is null");

   J9InternalVMFunctions * intFunc = javaVM->internalVMFunctions;
   J9Class * clazz = J9_CLASS_FROM_METHOD(method);
   J9AnnotationInfo *annotationInfo =  intFunc->getAnnotationInfoFromClass(javaVM,clazz);
   if (NULL == annotationInfo)
      {
      return false;
      }

   TR_PersistentClassInfo *classInfo=NULL;
   if(TR::Options::getCmdLineOptions()->allowRecompilation() &&
      !TR::Options::getCmdLineOptions()->getOption(TR_DisableCHOpts) &&
      compInfo->getPersistentInfo()->getPersistentCHTable())
      {
      TR_OpaqueClassBlock *cl=J9JitMemory::convertClassPtrToClassOffset(clazz);

      classInfo = compInfo->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(cl, fe);

      if(classInfo)
         {
         if(classInfo->hasRecognizedAnnotations())
            {
            return true;
            }
         else if(classInfo->alreadyCheckedForAnnotations())
            {
            return false;
            }

         classInfo->setAlreadyCheckedForAnnotations(true);
         }
      }


   J9AnnotationInfoEntry *annotationInfoEntryPtr;
   int32_t numAnnotations,i;

   numAnnotations= intFunc->getAllAnnotationsFromAnnotationInfo(annotationInfo,&annotationInfoEntryPtr);

   TR_ASSERT(numAnnotations,"Should have at least one annotation\n");

   // return false;
   for (i = 0;i < numAnnotations;++i)
      {
      for(int32_t j=0; j < kLastAnnotationSignature;++j)
         {


         J9UTF8 *nameData = (J9UTF8 *)SRP_GET(annotationInfoEntryPtr->annotationType,J9UTF8*);

         int32_t nameLength=0;
         char * annotationName = utf8Data(nameData,nameLength);
         if(false)printf("Comparing %*s to %s\n",nameLength,annotationName,recognizedAnnotations[j].name);
         if(nameLength == recognizedAnnotations[j].nameLen &&
            (0==strncmp(recognizedAnnotations[j].name,annotationName,nameLength)))
            {
            if(false) printf("* * *  found match * * *\n");
            if(classInfo) classInfo->setHasRecognizedAnnotations(true);

            return true;
            }
         }

      annotationInfoEntryPtr++;
      }
   TR_ASSERT(!classInfo || !classInfo->hasRecognizedAnnotations(),"current class marked as having annotations\n");
   return false;
   }

bool TR_AnnotationBase::loadAnnotation(J9Class *clazz,AnnotationSignature annotationSig)
   {
   if (_comp->compileRelocatableCode())
      return false;

   J9JavaVM * javaVM = ((TR_J9VMBase *)_comp->fej9())->_jitConfig->javaVM;
   J9InternalVMFunctions * intFunc = javaVM->internalVMFunctions;
   _annotationInfo = intFunc->getAnnotationInfoFromClass(javaVM,clazz);
   _annotationSignature=annotationSig;

   if(NULL == _annotationInfo)
      {
      return false;
      }

   return true;

   }


J9AnnotationInfoEntry *
TR_AnnotationBase::getAnnotationInfo(J9AnnotationInfo * annotationInfo, int32_t annotationType,
                                     const char *memberName,const  char * memberSignature,
                                     const char * annotationName,bool isTag)
   {

   J9AnnotationInfoEntry *annotationInfoEntryPtr=NULL;
   int32_t numAnnotations,i;
   TR_ASSERT(annotationInfo,"ann info is null");
   J9JavaVM * javaVM = ((TR_J9VMBase *)_comp->fej9())->_jitConfig->javaVM;
   J9InternalVMFunctions * intFunc = javaVM->internalVMFunctions;
   int32_t count;

   int32_t nameLen=0,signatureLen=0;
   if(memberName) nameLen = strlen(memberName);
   if(memberSignature) signatureLen=strlen(memberSignature);

   if(isTag)
      {
      annotationInfoEntryPtr = intFunc->getAnnotationFromAnnotationInfo(annotationInfo,annotationType,
                                                                        (char *)memberName,nameLen,
                                                                        (char *)memberSignature,signatureLen,
                                                                        (char *)annotationName,strlen(annotationName));
      if(false && annotationInfoEntryPtr) printf("found tag annotation %s for %s(%s)\n",annotationName,memberName,memberSignature);
      return annotationInfoEntryPtr;
      }
   else
      {
      count = intFunc->getAnnotationsFromAnnotationInfo(annotationInfo,annotationType,(char *)memberName,nameLen,
                                                        (char *)memberSignature,signatureLen,&annotationInfoEntryPtr);


      if(false)printf("found %d entries for %p\n",count,annotationInfoEntryPtr);

      if(count) return annotationInfoEntryPtr;
      }

   return NULL;
   }

J9AnnotationInfoEntry *
TR_AnnotationBase::getDefaultAnnotationInfo(const char *annotationName)
   {
   J9JavaVM * javaVM = ((TR_J9VMBase *)_comp->fej9())->_jitConfig->javaVM;
   J9InternalVMFunctions * intFunc = javaVM->internalVMFunctions;
   J9VMThread *vmThread = intFunc->currentVMThread(javaVM);
   if(NULL == _comp->getClassClassPointer()) return NULL;
   int32_t i;
   J9Class * clazz=NULL;
   for(i=0;i < kLastAnnotationSignature;++i)
      {
      if(0==strncmp(annotationName,recognizedAnnotations[i].name,recognizedAnnotations[i].nameLen))
         {
         clazz = recognizedAnnotations[i].clazz;
         break;
         }
      }
   if(NULL == clazz) return NULL;
   const char * className = annotationName+1; // strip off leading 'L';
   int32_t classNameLength = strlen (className) -1; // strip off trailing ';'
   J9AnnotationInfoEntry *defaultEntry = intFunc->getAnnotationDefaultsForNamedAnnotation(vmThread, clazz, (char *)className, classNameLength,
                                                                                          J9_FINDCLASS_FLAG_EXISTING_ONLY);
   return defaultEntry;

   }

bool TR_AnnotationBase::isValid(){
  return _isValid;
  }

TR_ScratchList<const char*> TR_AnnotationBase::getAnnotationNames(){
  TR_ScratchList<const char*> result(_comp->trMemory());
  return result;
  }

  TR_AnnotationBase::AnnotationType TR_AnnotationBase::getAnnotationType(const char* annotationName){
    return kUnknown;
  }



  /* for now must do a search though all the annotation entries--later when Charlie has fixed it use
   * more efficient query */
J9AnnotationInfoEntry *
TR_AnnotationBase::getAnnotationInfoEntry(TR::SymbolReference *symRef,AnnotationSignature annotationEnum)
   {
   return getAnnotationInfoEntry(symRef,recognizedAnnotations[annotationEnum].name);
   }

/* there are no annotation fields associated with this annotation, so we need to perform an explicit check */
J9AnnotationInfoEntry *
TR_AnnotationBase::getTaggedAnnotationInfoEntry(TR::SymbolReference *symRef,AnnotationSignature annotationEnum)
   {
   return getAnnotationInfoEntry(symRef,recognizedAnnotations[annotationEnum].name,true);
   }
J9AnnotationInfoEntry *
TR_AnnotationBase::getAnnotationInfoEntry(TR::SymbolReference *symRef,const char *annotationName,bool isTag)

   {

   char *nameBuf=NULL;
   const char * memberName,*memberSignature;
   int32_t annotationj9Type;
   bool trace = false;
   J9JavaVM * javaVM = ((TR_J9VMBase *)_comp->fej9())->_jitConfig->javaVM;
   PORT_ACCESS_FROM_JAVAVM(javaVM);
   memberName = memberSignature = NULL;


   if(trace) printf("getAnnotationInfoEntry: searching for >%s<\n",annotationName);

   TR::Symbol *sym = symRef->getSymbol();

   // For methods, fields and parameters, must specify signature to pick the
   // right one
   if(sym->isMethod())
      {
      TR::MethodSymbol *methodSym = sym->getMethodSymbol();

      if(!sym->isResolvedMethod()) return NULL;

      annotationj9Type = ANNOTATION_TYPE_METHOD;

      J9Class *clazz = (J9Class *)sym->castToResolvedMethodSymbol()->getResolvedMethod()->containingClass();
      TR_Method * vmSym = sym->castToMethodSymbol()->getMethod();
      memberName = vmSym->nameChars();
      memberSignature = vmSym->signatureChars();
      int32_t nameLen = vmSym->nameLength();
      int32_t sigLen = vmSym->signatureLength();
      nameBuf = (char *)j9mem_allocate_memory(nameLen+sigLen+2 * sizeof(char), J9MEM_CATEGORY_JIT);
      if (!nameBuf)
         return NULL;

      strncpy(nameBuf,memberName,nameLen);
      nameBuf[nameLen] = '\0';
      strncpy(nameBuf+nameLen+1,memberSignature,sigLen);
      nameBuf[nameLen+sigLen+1] = '\0';
      memberName = nameBuf;
      memberSignature = &(nameBuf[nameLen+1]);
      if(trace) printf("Method:%.*s sig:%.*s\n",nameLen,memberName,sigLen,memberSignature);
      }
   else if(sym->isShadow()) // field
      {
      if (symRef->getCPIndex() >= 0)
         {

         int32_t length = -1;
         // will return a string of the form "prefix.fieldname signature"
         // need to remove
         // replace space with a null and we'll have two strings
         char *name = symRef->getOwningMethod(_comp)->fieldName(symRef->getCPIndex(), length, _comp->trMemory());
         nameBuf = (char *)j9mem_allocate_memory(length+2 * sizeof(char), J9MEM_CATEGORY_JIT);
         if (!nameBuf)
            return NULL;

         strncpy(nameBuf,name,length);
	 memberName = nameBuf;

         int i;
         for( i=0; i < length;++i)
            if(nameBuf[i] == ' ') break;

         nameBuf[i] = '\0';
         memberSignature = &(nameBuf[i+1]);
         // now search backwards for '.'

         for( ; i >=0;i--)
            {
            if(nameBuf[i] == '.')
               {
               memberName = &(nameBuf[i+1]);
               break;
               }
            }

         annotationj9Type = ANNOTATION_TYPE_FIELD;
         if(trace) printf("Field:%s sig:%s\n",memberName,memberSignature);

      }
      else
	 return NULL;

      }
   else if(sym->isParm())
      {
      annotationj9Type = ANNOTATION_TYPE_PARAMETER;
      int32_t len,slotId = symRef->getCPIndex();
      int32_t parmNo=0;

      const char *s = symRef->getSymbol()->castToParmSymbol()->getTypeSignature(len);

      TR::Symbol *sym =  _comp->getOwningMethodSymbol(symRef->getOwningMethodIndex());
      TR_Method * vmSym =sym->castToMethodSymbol()->getMethod();
      if(NULL == vmSym) return NULL;

      TR::ResolvedMethodSymbol *resolvedSym = sym->castToResolvedMethodSymbol();
      if(resolvedSym == NULL) return NULL;

      ListIterator<TR::ParameterSymbol> parms(&resolvedSym->getParameterList());
      for (TR::ParameterSymbol * p = parms.getFirst(); p; p = parms.getNext())
         {
         if(p->getSlot() == slotId) break;
         parmNo++;
         }
      if(!resolvedSym->isStatic()) --parmNo; // accommodate 'this'

      annotationj9Type |= parmNo << ANNOTATION_PARM_SHIFT;
      memberName = vmSym->nameChars();
      int32_t nameLen = vmSym->nameLength();
      int32_t sigLen = vmSym->signatureLength();
      memberSignature = vmSym->signatureChars();
      nameBuf = (char *)j9mem_allocate_memory(nameLen+sigLen+2 * sizeof(char), J9MEM_CATEGORY_JIT);
      if (!nameBuf)
         return NULL;

      strncpy(nameBuf,memberName,nameLen);
      nameBuf[nameLen] = '\0';
      strncpy(nameBuf+nameLen+1,memberSignature,sigLen);
      nameBuf[nameLen+sigLen+1] = '\0';
      memberName = nameBuf;
      memberSignature = &(nameBuf[nameLen+1]);
      if(trace) printf("Parameter:(%.*s) slot is %d in method %s%s\n",len,s,parmNo,memberName,memberSignature);

      }
   else if(sym->isAuto())
      {
      // should be a field
      return NULL;
      }
   else if(sym->isClassObject() || sym->isAddressOfClassObject())
      {
      if(symRef->isUnresolved()) return NULL;


      memberSignature=NULL;
      memberName=NULL;

      annotationj9Type = ANNOTATION_TYPE_CLASS;
      }
   J9AnnotationInfoEntry *annotationInfoEntryPtr;
   annotationInfoEntryPtr = getAnnotationInfo(_annotationInfo,annotationj9Type,memberName,
                                              memberSignature,annotationName,
                                              isTag);


   if(NULL != nameBuf)
      j9mem_free_memory(nameBuf);
   return annotationInfoEntryPtr;


   }

  /* for now must do a search though all the annotation entries--later when Charlie has fixed it use
   * more efficient query */
bool TR_AnnotationBase::getValue(TR::SymbolReference *symRef,const char *annotationName,
                                 AnnotationType type,void *ptr)
   {
   const bool trace=false;
   J9AnnotationInfoEntry *annotationInfoEntryPtr = getAnnotationInfoEntry(symRef,annotationName);
   if((NULL == annotationInfoEntryPtr) ||
      !extractValue(annotationInfoEntryPtr,annotationName,type,ptr))
      {
      if(trace) printf("Searching in default annotations...\n");
      annotationInfoEntryPtr = getDefaultAnnotationInfo(annotationName);

      if(NULL == annotationInfoEntryPtr) return false;

      return extractValue(annotationInfoEntryPtr,annotationName,type,ptr);
      }
   return true;

   }
bool TR_AnnotationBase::extractValue(J9AnnotationInfoEntry * annotationInfoEntryPtr,
                                     const char *annotationName,  AnnotationType type,void *ptr)
   {
   if(NULL == annotationInfoEntryPtr) return false;
   J9JavaVM * javaVM = ((TR_J9VMBase *)_comp->fej9())->_jitConfig->javaVM;
   PORT_ACCESS_FROM_JAVAVM(javaVM);
   J9InternalVMFunctions * intFunc = javaVM->internalVMFunctions;

   bool trace=false;
   // now scan for the field name
   void *data;
   J9AnnotationState state;
   J9UTF8 * namePtr = intFunc->annotationElementIteratorStart(&state,annotationInfoEntryPtr,
                                                              &data);
   if(NULL ==namePtr) return false;

   do{
     int32_t * dataPtr = (int32_t *)data;
     int32_t tag = *dataPtr & ANNOTATION_TAG_MASK;
     char *fieldName;int32_t len;
     fieldName = utf8Data(namePtr,len);
     if(trace) printf("Comparing fieldName %.*s(%c) to %s(%d)\n",len,fieldName,tag,annotationName,len);

     if(strncmp(fieldName,annotationName,len)) continue;

     switch(type)
      {
      case kByte:  if('B' != tag) return false; else break;
      case kChar:  if('C' != tag) return false; else break;
      case kDouble:if('D' != tag) return false; else break;
      case kFloat: if('F' != tag) return false; else break;
      case kInt:   if('I' != tag) return false; else break;
      case kLong:  if('J' != tag) return false; else break;
      case kShort: if('S' != tag) return false; else break;
      case kBool:  if('Z' != tag) return false; else break;
      case kClass: if('c' != tag) return false; else break;
      case kEnum:  if('e' != tag) return false; else break;
      case kString:if('s' != tag) return false; else break;
      case kNested:if('@' != tag) return false; else break;
      case kArray: if('[' != tag) return false; else break;
      default:
        TR_ASSERT(false,"Unhandled type %p\n",type);
      }

     if(trace) printf("\tfound field %s\n",utf8Data(namePtr));

     ++dataPtr;// point at data
     *(int32_t* *)ptr = dataPtr;
     return true;
   } while(namePtr = intFunc->annotationElementIteratorNext(&state,&data));
    if(trace) printf("Search failed\n");
    return false;
  }


bool TR_AnnotationBase::getEnumeration(TR::SymbolReference * symRef,const char *annotationName,
				       char * * enumerationName,
				       int32_t *nameLength,
				       char * * enumerationValue,
				       int32_t *valueLength)
  {
  J9SRP *j9ptr;
  bool trace=false;
  if(trace) printf("getting value for %s...\n",annotationName);

  if(!getValue(symRef,annotationName,kEnum,(void *)&j9ptr))
     {
     return false;
     }

  J9SRP *typeNamePtr = (J9SRP* )j9ptr++;
  J9SRP *valueNamePtr = (J9SRP* )j9ptr;
  J9UTF8 *typeName =  (J9UTF8 *)SRP_PTR_GET(typeNamePtr,J9UTF8*);
  J9UTF8 *valueName = (J9UTF8 *)SRP_PTR_GET(valueNamePtr,J9UTF8*);
  *enumerationName = utf8Data(typeName,*nameLength);
  *enumerationValue = utf8Data(valueName,*valueLength);

  if(trace) printf("Enumeration returns %.*s %.*s\n",*nameLength,*enumerationName,
                   *valueLength,*enumerationValue);
  return true;
  }

TR_AnnotationBase::AnnotationTable TR_AnnotationBase::recognizedAnnotations[] = {
#undef ANNOT_ENTRY
#define ANNOT_ENTRY(A,B) {B,sizeof(B)-1,NULL},
#include "env/annotations/AnnotationTable.inc"
  {"kUnknown",0}
};



