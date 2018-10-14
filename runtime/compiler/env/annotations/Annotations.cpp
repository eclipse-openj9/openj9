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

#include "j9.h"
#include "env/j9method.h"
#include "vmaccess.h"
#include "env/VMJ9.h"
#include "runtime/MethodMetaData.h"

#include "compile/Compilation.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/Instruction.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "env/IO.hpp"

void
TR_Debug::printAnnotationInfoEntry(J9AnnotationInfo * annotationInfo,
                                   J9AnnotationInfoEntry *annotationInfoEntryPtr,int32_t indentationLevel)
   {
   J9UTF8 *annotationName;
   const int bufLen=1024;
   char stringBufferA[bufLen];
   J9JavaVM * javaVM = ((TR_J9VMBase *)_comp->fej9())->_jitConfig->javaVM;
   J9InternalVMFunctions * intFunc = javaVM->internalVMFunctions;
   annotationName = SRP_GET(annotationInfoEntryPtr->annotationType,J9UTF8*);
   const char *annotationTypeName;
   bool filterOnName=false; // don't filter out annotation based on name
   const char * methodName = _comp->getCurrentMethod()->signature(comp()->trMemory(), heapAlloc);

   int32_t flag = annotationInfoEntryPtr->flags;
   #define ANNO_NAMEBUF_LEN 30
   char annNameBuffer[ANNO_NAMEBUF_LEN];
   char * signatureName="";
   switch(flag)
      {
      case ANNOTATION_TYPE_CLASS:
	 annotationTypeName="class";
	 break;

      case ANNOTATION_TYPE_FIELD:{
         int32_t len;
         J9UTF8 *name      = SRP_GET(annotationInfoEntryPtr->memberName,J9UTF8*);
	 annotationTypeName="field:";
	 signatureName=utf8Data(name,len);
         strncpy(stringBufferA,signatureName,len);
         stringBufferA[len] = ' ';
         stringBufferA[len+1] = '\0';
         // append the type signature
         int32_t len2;
         char * typeName = utf8Data(SRP_GET(annotationInfoEntryPtr->memberSignature,J9UTF8*),len2);
         strncat(stringBufferA,typeName,len2);
         stringBufferA[len+len2+1] = '\0';
         TR_ASSERT(len+len2+1 < bufLen, "Buffer length of %d exceeded",bufLen);

         signatureName = stringBufferA;

	 //filterOnName=true;
	 break;
      }
      case ANNOTATION_TYPE_METHOD:
	 annotationTypeName="method";
	 filterOnName=true;
	 break;

      case ANNOTATION_TYPE_ANNOTATION:{
         int32_t len;
         J9UTF8 *name      = SRP_GET(annotationInfoEntryPtr->annotationType,J9UTF8*);
	 annotationTypeName="annotation:";
	 signatureName=utf8Data(name,len);
         strncpy(stringBufferA,signatureName,len);
         stringBufferA[len] = '\0';
         signatureName = stringBufferA;
	 break;
	 }
      default:
	 if ((flag & ~ANNOTATION_PARM_MASK) == ANNOTATION_TYPE_PARAMETER)
	    {
	    sprintf(annNameBuffer,"parm(%d)",(flag & ANNOTATION_PARM_MASK)>> ANNOTATION_PARM_SHIFT);
	    TR_ASSERT( strlen(annNameBuffer) < ANNO_NAMEBUF_LEN, "buffer length somehow exceeded\n");
	    annotationTypeName=annNameBuffer;
	    filterOnName=true;
	    break;
	    }
         else
	    annotationTypeName = "unknown";
      }

   if (filterOnName){
      J9UTF8 *name      = SRP_GET(annotationInfoEntryPtr->memberName,J9UTF8*);
      J9UTF8 *signature = SRP_GET(annotationInfoEntryPtr->memberSignature,J9UTF8*);

     //trfprintf(_file,"name=%s sig=%s\n",utf8Data(name),utf8Data(signature));

      //int32_t classNameLength;
      //const char *annotatedClass = utf8Data(SRP_GET(annotationInfo->className,J9UTF8*),classNameLength);

      //if (strncmp(methodName,annotatedClass,classNameLength) ||
      //   strncmp(methodName+classNameLength+1,utf8Data(name),name->length) ||
      //   strncmp(methodName+name->length+classNameLength+1,utf8Data(signature),signature->length))
      //   {
      //   return;
      //   }
      }

   J9AnnotationState state;
   void *data;
   J9UTF8 * namePtr = intFunc->annotationElementIteratorStart(&state,annotationInfoEntryPtr,&data);
   while (namePtr)
      {
      int32_t * ptr = (int32_t *)data;
      int32_t tag = *ptr & ANNOTATION_TAG_MASK;


      for (int32_t j=0;j< indentationLevel;++j) trfprintf(_file,"\t");
      int32_t len;
      char *dataName = utf8Data(namePtr,len);

      trfprintf(_file,"\ttype=%s%s %.*s=",annotationTypeName,signatureName,len,dataName);

      ++ptr;// point at data

      switch(tag)
         {
         case 'B': trfprintf(_file,"%d\n",*(int32_t *)ptr);break;
         case 'C': trfprintf(_file,"%d\n",*(int32_t*)ptr);break;
         case 'D': trfprintf(_file,"%e\n",*(double*)ptr);break;
         case 'F': trfprintf(_file,"%f\n",*(float*)ptr);break;
         case 'I':trfprintf(_file,"%d\n",*(int32_t*)ptr);break;
         case 'J':trfprintf(_file,"%lld\n",*(int64_t*)ptr);break;
         case 'S':trfprintf(_file,"%d\n",*(int32_t*)ptr);break;
         case 'Z':trfprintf(_file,"%d\n",*(int32_t*)ptr);break;
         case 'e':
            {
            J9SRP *typeNamePtr = (J9SRP* )ptr++;
            J9SRP *valueNamePtr = (J9SRP* )ptr;
            J9UTF8 *typeName =  (J9UTF8 *)SRP_PTR_GET(typeNamePtr,J9UTF8*);
            J9UTF8 *valueName = (J9UTF8 *)SRP_PTR_GET(valueNamePtr,J9UTF8*);
            int32_t dataLen,typeLen;
            char *vName = utf8Data(valueName,dataLen);
            char * tName = utf8Data(typeName,typeLen);
            trfprintf(_file,"%.*s enum_type=\"%.*s\"\n",dataLen,vName,typeLen,tName);

            break;
            }
         case 'c':
         case 's':
            {
            J9SRP *stringNamePtr = (J9SRP* )ptr;
            int32_t len;
            char *dataName = utf8Data(SRP_PTR_GET(stringNamePtr,J9UTF8*),len);
            trfprintf(_file,"\"%.*s\"\n",len,dataName);
            break;
            }

         case '@':
            {
            J9AnnotationInfoEntry *infoPtr = SRP_PTR_GET(ptr,J9AnnotationInfoEntry *);
            int32_t j,mLen,sLen;
            for (j=0;j< indentationLevel;++j) trfprintf(_file,"\t");
            trfprintf(_file,"(nested annotation)\n\n");
            char * mName=  utf8Data(SRP_GET(annotationInfoEntryPtr->memberName,J9UTF8*),mLen);

            char *sName = utf8Data(SRP_GET(annotationInfoEntryPtr->memberSignature,J9UTF8*),sLen);
            trfprintf(_file, "\t<annotations name=\"%.*s %.*s\">\n",mLen,mName,sLen,sName);

            printAnnotationInfoEntry( annotationInfo,infoPtr,++indentationLevel);

            for (j=0;j< indentationLevel;++j) trfprintf(_file,"\t");
               trfprintf(_file, "</annotations>\n\n");
            break;
            }

         case '[':
            {
            uint32_t arraySize = *ptr++;
            uint32_t numElements = *ptr++;
            char *charPtr = (char *)ptr;
            bool truncateOutput = 40< arraySize/sizeof(uint32_t);
            int32_t upperLimit =  arraySize/sizeof(uint32_t);
            if (truncateOutput) upperLimit = 40;

            for (int32_t i =0; i < upperLimit;++i)
               {
               if (((i+1) % 12) == 0) trfprintf(_file,"\n\t\t");

               trfprintf(_file,"%x ",*ptr++);
               }
            if (truncateOutput) trfprintf(_file," (truncated)...");
            trfprintf(_file,"\n");
            break;
            }

         default:
            trfprintf(_file,"Unknown tag:%x %c\n",tag,tag);
         }


      namePtr = intFunc->annotationElementIteratorNext(&state,&data);
      }

   J9VMThread* vmContext = intFunc->currentVMThread(javaVM);
   J9Class * clazz = (J9Class *)_comp->getCurrentMethod()->containingClass();
                                                 //NOTE: acquireVMAccess() has already been called
   // default query is broken right now
   J9AnnotationInfoEntry *defaultEntry;
   defaultEntry =  intFunc->getAnnotationDefaultsForAnnotation(vmContext,clazz,annotationInfoEntryPtr,
                                                               J9_FINDCLASS_FLAG_EXISTING_ONLY);
   if (defaultEntry)
      {
      trfprintf(_file,"\n");
      int32_t j;
      for (j=0;j< indentationLevel;++j) trfprintf(_file,"\t");
      trfprintf(_file,"Default values:\n");

      printAnnotationInfoEntry(annotationInfo,defaultEntry,indentationLevel);
      }
   }



void
TR_Debug::printByteCodeAnnotations()
   {

   //JVM support for JXE is broken right now.  Disable temporarily

   if (_comp->compileRelocatableCode())
      {
      trfprintf(_file,"AOT support of annotations temporarily disabled\n");
      return;
      }

   J9JavaVM * javaVM = ((TR_J9VMBase *)_comp->fej9())->_jitConfig->javaVM;
   J9InternalVMFunctions * intFunc = javaVM->internalVMFunctions;
   J9Class * clazz = (J9Class *)_comp->getCurrentMethod()->containingClass();
   J9AnnotationInfo *annotationInfo =  intFunc->getAnnotationInfoFromClass(javaVM,clazz);
   if (NULL == annotationInfo) return;

   J9AnnotationInfoEntry *annotationInfoEntryPtr;
   int32_t numAnnotations,i;

   numAnnotations= intFunc->getAllAnnotationsFromAnnotationInfo(annotationInfo,&annotationInfoEntryPtr);
   TR_ASSERT( numAnnotations, "Should have at least one annotation\n");

   trfprintf(_file, "\n<annotations name=\"%s\">\n", _comp->getCurrentMethod()->signature(comp()->trMemory(), heapAlloc));
   for (i = 0;i < numAnnotations;++i)
      {

      printAnnotationInfoEntry( annotationInfo,annotationInfoEntryPtr,0);
      annotationInfoEntryPtr++;
      }

   trfprintf(_file, "</annotations>\n");
   }

