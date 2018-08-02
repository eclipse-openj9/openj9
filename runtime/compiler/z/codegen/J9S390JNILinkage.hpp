/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#ifndef J9S390JNILINKAGE_INCL
#define J9S390JNILINKAGE_INCL

#include "z/codegen/J9S390PrivateLinkage.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class Snippet; }

namespace TR {

/**
 * \brief
 * The JNI dispatch building class for the z system.
 * This linkage class uses the OMR::Z::Linkage to build arguments, TR::S390PrivateLinkage to build dependencies, and
 * system linkage to buld the native call.
*/
class J9S390JNILinkage : public TR::S390PrivateLinkage
   {
public:

   J9S390JNILinkage(TR::CodeGenerator * cg,
                    TR_S390LinkageConventions elc = TR_JavaPrivate,
                    TR_LinkageConventions lc = TR_J9JNILinkage);

   /**
    * \brief
    * Build the whole JNI direct dispatch sequence for the JNI call node.
    *
   */
   virtual TR::Register * buildDirectDispatch(TR::Node * callNode);

private:

   /**
    * \brief
    * Print register assignment information for a given dependency group
    */
   void printDepsDebugInfo(TR::Node* callNode, TR::RegisterDependencyConditions* deps);

   /**
    * \brief
    * A pre and post native helper. This function uses the J9S390CHelperLinkage to build a C-helper
    * call to invoke IFA switching VM helpers. This is used
    * to build calls to turn on and off IFA switching on z/OS.
    *
    * \param deps
    *       the JNI call pre and post dependency group
    * \param isJNICallOutFrame
    *       boolean flag to indicated if a callout frame is needed.
    * \param helperIndex
    *       The IFA call helper index
   */
   void callIFAOffloadCheck(TR::Node* callNode, TR_RuntimeHelper helperIndex);

   /**
    * \brief
    * Post-JNI dependency bulider. It appends a post-dependency group at the very end of all
    * JNI related operations to kill all GPRs and HPRs.
    * This is used to keep RA from spilling and reverse-spilling before the JNI call itself and its helper calls
    * are able to finish.
   */
   void addEndOfAllJNILabel(TR::Node* callNode,
                            TR::RegisterDependencyConditions* deps,
                            TR::Register* javaReturnRegister);

   /**
    * \brief
    * Pre-JNI frame builder.
    * Sets up the 5-slot JNI Callout frame to be used by the VM.
   */
   void setupJNICallOutFrameIfNeeded(TR::Node * callNode,
                                     bool isJNICallOutFrame,
                                     bool isJavaOffLoadCheck,
                                     TR::RealRegister * javaStackPointerRealRegister,
                                     TR::Register * methodMetaDataVirtualRegister,
                                     TR::Register* javaLitOffsetReg,
                                     TR::S390JNICallDataSnippet *jniCallDataSnippet,
                                     TR::Register* tempReg);

   /**
    * \brief
    * A helper function to populate JNI call data and builds up the snippet object.
    */
   TR::S390JNICallDataSnippet*
   buildJNICallDataSnippet(TR::Node* callNode,
                           TR::RegisterDependencyConditions* deps,
                           intptrj_t targetAddress,
                           bool isJNICallOutFrame,
                           bool isReleaseVMAccess,
                           TR::LabelSymbol * returnFromJNICallLabel,
                           int64_t killMask);
   /**
    * \brief
    * Post-JNI VM helper call builder.
    * This builds a call to invoke the VM check exception helper. Used after returning from the native code to check
    * native code exceptions.
   */
   void checkException(TR::Node * callNode, TR::Register* flagReg, TR::Register* methodMetaDataVirtualRegister);

#ifdef J9ZOS390
   /**
    * \brief
    * z/OS only. The CEE handler requires the system stack pointer register and the vmThread register saved on the system stack.
   */
   void saveRegistersForCEEHandler(TR::Node * callNode, TR::Register* vmThreadRegister);
#endif

   /**
    * \brief
    * Post-JNI helper builder. Builds a call to invoke the collapseJNIReference VM helper after the native code returns.
    *
    * \param callNode
    *       the JNI call node
    * \param flagReg
    *       a real register that contains the value that determines if a collapse frame call is needed
   */
   void collapseJNIReferenceFrame(TR::Node * callNode,
                                  TR::Register* javaLitPoolRealRegister,
                                  TR::Register* methodAddressReg,
                                  TR::Register* javaStackPointerRealRegister);


   /**
    * \brief
    *
    *
    *
   */
   void unwrapReturnValue(TR::Node* callNode,
                          TR::Register* javaReturnRegister,
                          TR::RegisterDependencyConditions* deps);

#ifdef J9VM_INTERP_ATOMIC_FREE_JNI
   /**
    * \brief
    * Build the release VM access sequence for JNI dispatch without using the atomic compare-and-swap
    * instruction. This is mutually exclusive with the old compare-and-swap releaseVMAccessMask().
    */
   void releaseVMAccessMaskAtomicFree(TR::Node * callNode,
                                      TR::Register * methodMetaDataVirtualRegister,
                                      TR::Register * tempReg1);

   /**
    * \brief
    * Build the acquire VM access sequence for JNI dispatch without using the atomic compare-and-swap
    * instruction. This is mutually exclusive with the old compare-and-swap acquireVMAccessMask().
    */
   void acquireVMAccessMaskAtomicFree(TR::Node * callNode,
                                      TR::Register * methodMetaDataVirtualRegister,
                                      TR::Register * tempReg1);
#else
   /**
    * \brief
    * Pre-JNI VM helper call builder.
    * This is used to build a call to invoke the VM releaseVMAccessMask helper before transitioning from the
    * JIT'ed code to native code.
    */
   void releaseVMAccessMask(TR::Node * callNode, TR::Register * methodMetaDataVirtualRegister,
                            TR::Register * methodAddressReg, TR::Register * javaLitOffsetReg,
                            TR::S390JNICallDataSnippet * jniCallDataSnippet, TR::RegisterDependencyConditions * deps);
   /**
    * \brief
    * Post-JNI VM helper call builder.
    * This is used to build a call to invoke the VM acquireVMAccessMask helper after transitioning from the
    * native code back to the JIT'ed code.
    */
   void acquireVMAccessMask(TR::Node * callNode, TR::Register * javaLitPoolVirtualRegister,
                            TR::Register * methodMetaDataVirtualRegister);
#endif
   };
}

#endif /* J9S390JNILINKAGE_INCL */
