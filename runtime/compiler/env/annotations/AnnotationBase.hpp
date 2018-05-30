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

#ifndef ANNOTATIONBASE_INCL
#define ANNOTATIONBASE_INCL

#include "j9.h"
#include "codegen/FrontEnd.hpp"
#include "infra/List.hpp"
#include "env/VMJ9.h"

namespace TR { class CompilationInfo; }

class TR_AnnotationBase{

public:
/*
 * ATTENTION: make sure this is always in sync with getTypeSig[]
 */
typedef enum {
  kUnknown = 0,
  kByte,
  kChar,
  kDouble,
  kFloat,
  kInt,
  kLong,
  kShort,
  kBool,
  kClass,
  kEnum,
  kString,
  kNested,
  kArray,
  kLastType
} AnnotationType;


  typedef struct{
    char           * name;
    AnnotationType   type;
  } AnnotationEntry;

typedef enum {
#undef ANNOT_ENTRY
#define ANNOT_ENTRY(A,B) A,
#include "env/annotations/AnnotationTable.inc"
  kLastAnnotationSignature,
  kUnknownAnnotationSignature = kLastAnnotationSignature,
  } AnnotationSignature;


typedef struct {
   char        * name;
   int32_t       nameLen;
   J9Class     * clazz; // annotation class
   } AnnotationTable;
  static AnnotationTable recognizedAnnotations[];

public:
  static bool scanForKnownAnnotationsAndRecord(TR::CompilationInfo *compInfo,J9Method *,J9JavaVM*, TR_FrontEnd *);
  static void loadExpectedAnnotationClasses(J9VMThread * vmThread);
  bool extractValue(J9AnnotationInfoEntry *, const char *, AnnotationType ,void *);

  TR_AnnotationBase(TR::Compilation *);

	// Will return false if class used in constructor did not have an annotation, 
	// or if annotation layout was not as expected (e.g. if class annotation layout has changed)
  bool isValid() ;// return true if this object has been annotated as expected.  

	
// Returns null if isValid() returns false
  TR_ScratchList<const char*> getAnnotationNames(); //get list of names of all the annotations associated with this object.

  AnnotationType getAnnotationType(const char* annotationName);// get the annotation type of this annotation

// returns true if successful. Returns false and leaves ptr unset if annotation of the expected type cannot be found.
// ptr must be matched with the AnnotationType.  The datatypes are as follows:
  bool getValue(TR::SymbolReference* symRef,const char *annotationName,AnnotationType type,void *ptr);

  bool getEnumeration(TR::SymbolReference * symRef,const char *annotationName,
				       char * * enumerationName,int32_t *nameLen,
				       char * * enumerationValue,int32_t *valueLen);

  protected:
     J9AnnotationInfoEntry *getDefaultAnnotationInfo(const char *annotationName);
     J9AnnotationInfoEntry * getAnnotationInfo(J9AnnotationInfo * , int32_t, const char *,const char *, const char *,
                                               bool tag=false);

     J9AnnotationInfoEntry *getAnnotationInfoEntry(TR::SymbolReference *,const char *,bool tag=false);  
     J9AnnotationInfoEntry *getTaggedAnnotationInfoEntry(TR::SymbolReference *,AnnotationSignature);
     J9AnnotationInfoEntry *getAnnotationInfoEntry(TR::SymbolReference *,AnnotationSignature);
     bool loadAnnotation(J9Class *clazz,AnnotationSignature annotationSig);
    
     bool                    _isValid;
     J9AnnotationInfo      * _annotationInfo;
     TR::Compilation         *_comp;
     AnnotationSignature     _annotationSignature; 
};


#endif
