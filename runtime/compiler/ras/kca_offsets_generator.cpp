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

/*
  * This file is used to write a header file that can be used with KCA so
  * that it knows the offset information required to correctly work with this
  * JVM.
  *
  * How to modify:
  *
  *   If a field has been removed - Set the "#define" to be 0, KCA will have to
  *   be modified so that it does not use that offset for the particular release
  *   but the #define has to be in the generated file!
  *
  *   In general, the output header file from this code must generate the same
  *   number of "#define" lines using the same names in all JIT releases.
  */

#include "j9.h"
#include "j9cfg.h"
#include "control/Options.hpp"
#include "control/OptionsUtil.hpp"
#include "control/Options_inlines.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/PersistentInfo.hpp"
#include <stdio.h>


char *
J9::Options::kcaOffsets(char *option, void *, TR::OptionTable *entry)
   {
   char szFileName[40];
   char szCMPRSS[8];
   char szRT[4];

   #if defined(OMR_GC_COMPRESSED_POINTERS)
   strcpy( szCMPRSS, "_CMPRSS" );
   #else
   szCMPRSS[0]='\0';
   #endif

   szRT[0]='\0';

   // The generated file looks like this: kca_offsets_gen_R#_#[_CMPRSS][_RT].h
   sprintf( szFileName, "kca_offsets_gen_R%d_%d%s%s.h", EsVersionMajor, EsVersionMinor, szCMPRSS, szRT );
   TR_ASSERT( strlen(szFileName)<40, "szFileName array needs to be increased" );

   auto file = fopen( szFileName, "wt" );
   if( file )
      {
      fprintf( file, "/*Automatically Generated Header*/\n\n" );
      fprintf( file, "/*File name: %s*/\n\n", szFileName );

      fprintf( file, "#define J9METHOD_BYTECODES         (%d)\n", offsetof(J9Method,bytecodes) );
      fprintf( file, "#define J9METHOD_CONSTANTPOOL      (%d)\n", offsetof(J9Method,constantPool) );
      fprintf( file, "#define J9METHOD_EXTRA             (%d)\n", offsetof(J9Method,extra) );

      fprintf( file, "#define J9CLASSLOADER_OBJ          (%d)\n", offsetof(J9ClassLoader,classLoaderObject) );

      // There should be a better way to do this???
      #if EsVersionMajor == 2 && EsVersionMinor <= 30
      fprintf( file, "#define CONSTANTPOOL_ALIGNMENT     (8)\n" );
      #else
      fprintf( file, "#define CONSTANTPOOL_ALIGNMENT     (16)\n" );
      #endif


      fprintf( file, "#define J9OBJECT_J9CLASS           (%d)\n", offsetof(J9IndexableObjectContiguous,clazz) );
      #if !defined(J9VM_INTERP_FLAGS_IN_CLASS_SLOT)
      fprintf( file, "#define J9OBJECT_FLAGS             (%d)\n", offsetof(J9IndexableObjectContiguous,flags) );
      #else
	  fprintf( file, "#define J9OBJECT_FLAGS             (0) // Does not exist!\n" );
      #endif
      #if !defined(J9VM_THR_LOCK_NURSERY)
      fprintf( file, "#define J9OBJECT_MONITOR           (%d)\n", offsetof(J9IndexableObjectContiguous,monitor) );
      #else
      fprintf( file, "#define J9OBJECT_MONITOR           (0) // Does not exist!\n" );
      #endif
      fprintf( file, "#define J9OBJECT_ARRAY_SIZE        (%d)\n", offsetof(J9IndexableObjectContiguous,size) );

      fprintf( file, "#define METADATA_CLASSNAME         (%d)\n", offsetof(J9JITExceptionTable,className) );
      fprintf( file, "#define METADATA_METHODNAME        (%d)\n", offsetof(J9JITExceptionTable,methodName) );
      fprintf( file, "#define METADATA_SIGNATURE         (%d)\n", offsetof(J9JITExceptionTable,methodSignature) );
      fprintf( file, "#define METADATA_CONSTANTPOOL      (%d)\n", offsetof(J9JITExceptionTable,constantPool) );
      fprintf( file, "#define METADATA_J9METHOD          (%d)\n", offsetof(J9JITExceptionTable,ramMethod) );
      fprintf( file, "#define METADATA_STARTPC           (%d)\n", offsetof(J9JITExceptionTable,startPC) );
      fprintf( file, "#define METADATA_ENDWARMPC         (%d)\n", offsetof(J9JITExceptionTable,endWarmPC) );
      fprintf( file, "#define METADATA_COLDSTART         (%d)\n", offsetof(J9JITExceptionTable,startColdPC) );
      fprintf( file, "#define METADATA_COLDEND           (%d)\n", offsetof(J9JITExceptionTable,endPC) );
      fprintf( file, "#define METADATA_FRAMESIZE         (%d)\n", offsetof(J9JITExceptionTable,totalFrameSize) );
      fprintf( file, "#define METADATA_NUM_EXC_RANGES    (%d)\n", offsetof(J9JITExceptionTable,numExcptionRanges) );
      fprintf( file, "#define METADATA_INLINEDCALLS      (%d)\n", offsetof(J9JITExceptionTable,inlinedCalls) );
      fprintf( file, "#define METADATA_BODYINFO          (%d)\n", offsetof(J9JITExceptionTable,bodyInfo) );
      fprintf( file, "#define METADATA_SIZE              (%d)\n", sizeof(J9JITExceptionTable) );

      fprintf( file, "#define J9CLASS_J9ROMCLASS         (%d)\n", offsetof(J9Class,romClass) );
      fprintf( file, "#define J9CLASS_SUPERCLASSES       (%d)\n", offsetof(J9Class,superclasses) );
      fprintf( file, "#define J9CLASS_CLASSDEPTHANDFLAGS (%d)\n", offsetof(J9Class,classDepthAndFlags) );
      fprintf( file, "#define J9CLASS_CLASSLOADER        (%d)\n", offsetof(J9Class,classLoader) );
      #if EsVersionMajor == 2 && EsVersionMinor <= 30
      fprintf( file, "#define J9CLASS_CLASSOBJECT        (0)\n" ); // Not used by KCA for 22/23
      #else
      fprintf( file, "#define J9CLASS_CLASSOBJECT        (%d)\n", offsetof(J9Class,classObject) );
      #endif
      fprintf( file, "#define J9CLASS_J9METHODS          (%d)\n", offsetof(J9Class,ramMethods) );
      fprintf( file, "#define J9CLASS_INSTANCESIZE       (%d)\n", offsetof(J9Class,totalInstanceSize) );
      fprintf( file, "#define J9CLASS_SUBCLASSLINK       (%d)\n", offsetof(J9Class,subclassTraversalLink) );
      fprintf( file, "#define J9CLASS_ITABLE             (%d)\n", offsetof(J9Class,iTable) );
      fprintf( file, "#define J9CLASS_VFT                (%d)\n", sizeof(J9Class) );
      fprintf( file, "#define J9CLASS_LOCKOFFSET         (%d)\n", offsetof(J9Class,lockOffset) );

      fprintf( file, "#define J9ARRAYCLASS_ARRAYTYPE     (%d)\n", offsetof(J9ArrayClass,arrayClass) );
      fprintf( file, "#define J9ARRAYCLASS_COMPTYPE      (%d)\n", offsetof(J9ArrayClass,componentType) );

      fprintf( file, "#define J9ROMCLASS_CLASSNAME       (%d)\n", offsetof(J9ROMClass,className) );
      fprintf( file, "#define J9ROMCLASS_SUPERCLASSNAME  (%d)\n", offsetof(J9ROMClass,superclassName) );
      fprintf( file, "#define J9ROMCLASS_MODIFIERS       (%d)\n", offsetof(J9ROMClass,modifiers) );
      fprintf( file, "#define J9ROMCLASS_ROMMETHODCOUNT  (%d)\n", offsetof(J9ROMClass,romMethodCount) );
      fprintf( file, "#define J9ROMCLASS_ROMMETHODS      (%d)\n", offsetof(J9ROMClass,romMethods) );
      fprintf( file, "#define J9ROMCLASS_ROMFIELDCOUNT   (%d)\n", offsetof(J9ROMClass,romFieldCount) );
      fprintf( file, "#define J9ROMCLASS_ROMFIELDS       (%d)\n", offsetof(J9ROMClass,romFields) );
      fprintf( file, "#define J9ROMCLASS_RAMCPCOUNT      (%d)\n", offsetof(J9ROMClass,ramConstantPoolCount) );
      fprintf( file, "#define J9ROMCLASS_ROMCPCOUNT      (%d)\n", offsetof(J9ROMClass,romConstantPoolCount) );

      fprintf( file, "#define J9ROMMETHOD_NAME           (%d)\n", offsetof(J9ROMMethod,nameAndSignature) + offsetof(J9ROMNameAndSignature,name) );
      fprintf( file, "#define J9ROMMETHOD_SIGNATURE      (%d)\n", offsetof(J9ROMMethod,nameAndSignature) + offsetof(J9ROMNameAndSignature,signature) );
      fprintf( file, "#define J9ROMMETHOD_MODIFIERS      (%d)\n", offsetof(J9ROMMethod,modifiers) );
      #if EsVersionMajor == 2 && EsVersionMinor <= 30
      fprintf( file, "#define J9ROMMETHOD_BC_SIZELOW     (%d)\n", offsetof(J9ROMMethod,bytecodeSlots) );
      fprintf( file, "#define J9ROMMETHOD_BC_SIZEHIGH    (%d)\n", offsetof(J9ROMMethod,bytecodeSlotsHigh) );
      #else
      fprintf( file, "#define J9ROMMETHOD_BC_SIZELOW     (%d)\n", offsetof(J9ROMMethod,bytecodeSizeLow) );
      fprintf( file, "#define J9ROMMETHOD_BC_SIZEHIGH    (%d)\n", offsetof(J9ROMMethod,bytecodeSizeHigh) );
      #endif
      fprintf( file, "#define J9ROMMETHOD_MAXSTACK       (%d)\n", offsetof(J9ROMMethod,maxStack) );
      fprintf( file, "#define J9ROMMETHOD_ARGCOUNT       (%d)\n", offsetof(J9ROMMethod,argCount) );

      fprintf( file, "#define J9ROMFIELDSHAPE_NAME       (%d)\n", offsetof(J9ROMFieldShape,nameAndSignature) + offsetof(J9ROMNameAndSignature,name) );
      fprintf( file, "#define J9ROMFIELDSHAPE_SIGNATURE  (%d)\n", offsetof(J9ROMFieldShape,nameAndSignature) + offsetof(J9ROMNameAndSignature,signature) );
      fprintf( file, "#define J9ROMFIELDSHAPE_MODIFIERS  (%d)\n", offsetof(J9ROMFieldShape,modifiers) );
      fprintf( file, "#define J9ROMFIELDSHAPE_VALUE      (%d)\n", sizeof(J9ROMFieldShape) );

      fprintf( file, "#define J9METHOD_SIZE              (%d)\n", sizeof(J9Method) );
      fprintf( file, "#define BYTECODES_J9ROMMETHOD      (%d)\n", -(int)sizeof(J9ROMMethod));

      fprintf( file, "#define J9VMRAS_VM                 (%d)\n", offsetof(J9RAS,vm) );
      fprintf( file, "#define J9VMRAS_CRASHINFO          (%d)\n", offsetof(J9RAS,crashInfo) );

      fprintf( file, "#define CRASHINFO_FAILINGTHREAD    (%d)\n", offsetof(J9RASCrashInfo,failingThread) );
      fprintf( file, "#define CRASHINFO_GPINFO           (%d)\n", offsetof(J9RASCrashInfo,gpInfo) );

      fprintf( file, "#define VM_MAIN_THREAD             (%d)\n", offsetof(J9JavaVM,mainThread) );
      fprintf( file, "#define VM_JITCONFIG               (%d)\n", offsetof(J9JavaVM,jitConfig) );
      fprintf( file, "#define VM_BOOLARRAYCLASS          (%d)\n", offsetof(J9JavaVM,booleanArrayClass) );
      fprintf( file, "#define VM_CMPRSS_DISPLACEMENT     (%d)\n", 0 );
      #if defined(OMR_GC_COMPRESSED_POINTERS)
      fprintf( file, "#define VM_CMPRSS_SHIFT            (%d)\n", offsetof(J9JavaVM,compressedPointersShift) );
      #else
      fprintf( file, "#define VM_CMPRSS_SHIFT            (%d)\n", 0 );
      #endif

      fprintf( file, "#define JITCONFIG_JITARTIFACTS     (%d)\n", offsetof(J9JITConfig,translationArtifacts) );
      fprintf( file, "#define JITCONFIG_COMPILING        (%d)\n", 0 );
      fprintf( file, "#define JITCONFIG_PSEUDOTOC        (%d)\n", offsetof(J9JITConfig,pseudoTOC) );

      fprintf( file, "#define J9AVLTREE_ROOTNODE         (%d)\n", offsetof(J9AVLTree,rootNode) );

      fprintf( file, "#define J9VMTHREAD_COMPILING       (%d)\n", offsetof(J9VMThread,jitMethodToBeCompiled) );
      fprintf( file, "#define J9VMTHREAD_VM              (%d)\n", offsetof(J9VMThread,javaVM) );
      fprintf( file, "#define J9VMTHREAD_ARG0EA          (%d)\n", offsetof(J9VMThread,arg0EA) );
      fprintf( file, "#define J9VMTHREAD_BYTECODES       (%d)\n", offsetof(J9VMThread,bytecodes) );
      fprintf( file, "#define J9VMTHREAD_SP              (%d)\n", offsetof(J9VMThread,sp) );
      fprintf( file, "#define J9VMTHREAD_PC              (%d)\n", offsetof(J9VMThread,pc) );
      fprintf( file, "#define J9VMTHREAD_LITERALS        (%d)\n", offsetof(J9VMThread,literals) );
      fprintf( file, "#define J9VMTHREAD_SOF_MARK        (%d)\n", offsetof(J9VMThread,stackOverflowMark) );
      #if defined(J9VM_GC_THREAD_LOCAL_HEAP)
      fprintf( file, "#define J9VMTHREAD_HEAP_ALLOC      (%d)\n", offsetof(J9VMThread,heapAlloc) );
      #else
      fprintf( file, "#define J9VMTHREAD_HEAP_ALLOC      (%d)\n", 0 ); // The RealTime JVM does not have this field
      #endif
      fprintf( file, "#define J9VMTHREAD_THREADOBJ       (%d)\n", offsetof(J9VMThread,threadObject) );
      fprintf( file, "#define J9VMTHREAD_STACKOBJ        (%d)\n", offsetof(J9VMThread,stackObject) );
      fprintf( file, "#define J9VMTHREAD_OSTHREAD        (%d)\n", offsetof(J9VMThread,osThread) );
      fprintf( file, "#define J9VMTHREAD_CUR_EXCEPTION   (%d)\n", offsetof(J9VMThread,currentException) );
      fprintf( file, "#define J9VMTHREAD_NEXT_THREAD     (%d)\n", offsetof(J9VMThread,linkNext) );
      fprintf( file, "#define J9VMTHREAD_ELS             (%d)\n", offsetof(J9VMThread,entryLocalStorage) );

      fprintf( file, "#define OSTHREAD_TID               (%d)\n", offsetof(J9AbstractThread,tid) );

      fprintf( file, "#define J9JITSTACKATLAS_MAPBYTES   (%d)\n", offsetof(J9JITStackAtlas,numberOfMapBytes) );
      fprintf( file, "#define BODYINFO_HOTNESS           (%d)\n", offsetof(TR_PersistentJittedBodyInfo,_hotness) );
      fprintf( file, "#define PERSISTENTINFO_CHTABLE     (%d)\n", offsetof(TR::PersistentInfo,_persistentCHTable) );
      fprintf( file, "#define PERSISTENTCLASS_VISITED    (%d)\n", offsetof(TR_PersistentClassInfo,_visitedStatus) );

      fprintf( file, "#define ELS_OLDELS                 (%d)\n", offsetof(J9VMEntryLocalStorage,oldEntryLocalStorage) );
      fprintf( file, "#define ELS_I2JSTATE               (%d)\n", offsetof(J9VMEntryLocalStorage,i2jState) );

      fprintf( file, "#define I2JSTATE_RETURNSP          (%d)\n", offsetof(J9I2JState,returnSP) );
      fprintf( file, "#define I2JSTATE_A0                (%d)\n", offsetof(J9I2JState,a0) );
      fprintf( file, "#define I2JSTATE_J9METHOD          (%d)\n", offsetof(J9I2JState,literals) );
      fprintf( file, "#define I2JSTATE_RETURNPC          (%d)\n", offsetof(J9I2JState,pc) );

      fprintf( file, "#define J9SFJ2IFRAME_RETURNADDR    (%d)\n", offsetof(J9SFJ2IFrame,returnAddress) );
      fprintf( file, "#define J9SFJ2IFRAME_RETURNSP      (%d)\n", offsetof(J9SFJ2IFrame,taggedReturnSP) );

      fclose(file);
      }
   return option;
   }

