/*******************************************************************************
* Copyright IBM Corp. and others 2017
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

#ifndef J9RECOGNIZEDCALLTRANSFORMER_INCL
#define J9RECOGNIZEDCALLTRANSFORMER_INCL

#include "optimizer/OMRRecognizedCallTransformer.hpp"
#include "compile/SymbolReferenceTable.hpp"

namespace J9
{

class RecognizedCallTransformer : public OMR::RecognizedCallTransformer
   {
   public:
   RecognizedCallTransformer(TR::OptimizationManager* manager)
      : OMR::RecognizedCallTransformer(manager)
      {
      _processedINLCalls = new (trStackMemory()) TR_BitVector(0, trMemory(), stackAlloc, growable);
      }

   protected:
   virtual bool isInlineable(TR::TreeTop* treetop);
   virtual void transform(TR::TreeTop* treetop);

   private:
   void processIntrinsicFunction(TR::TreeTop* treetop, TR::Node* node, TR::ILOpCodes opcode);
   void processConvertingUnaryIntrinsicFunction(TR::TreeTop* treetop, TR::Node* node, TR::ILOpCodes argConvertOpcode, TR::ILOpCodes opcode, TR::ILOpCodes resultConvertOpcode);
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
   /** \brief
    *     Helper function that constructs the alternate faster path for bypassing VM INL calls, used by linkToStatic, linkToSpecial,
    *     and invokeBasic call tranformers
    *
    *  \param treetop
    *     the TreeTop anchoring the call node
    *
    *  \param node
    *     the call node representing the original INL call
    *
    *  \param vmTargetNode
    *     the node representing the J9Method corresponding to the target method
    *
    *  \param argsList
    *     the TR::list<TR::SymbolReference *> consisting of temp symrefs to load args for the computed call node
    *
    *  \param inlCallNode
    *     the node representing the reconstructed INL call that can be safely placed in a different basic block
    *
    */
   void processVMInternalNativeFunction(TR::TreeTop* treetop, TR::Node* node, TR::Node* vmTargetNode, TR::list<TR::SymbolReference *>* argsList, TR::Node* inlCallNode);
#endif
   /** \brief
    *     Transforms java/lang/Class.IsAssignableFrom(Ljava/lang/Class;)Z into a JIT helper call TR_checkAssignable with equivalent
    *     semantics.
    *
    *  \param treetop
    *     The treetop which anchors the call node.
    *
    *  \param node
    *     The call node representing a call to java/lang/Class.IsAssignableFrom(Ljava/lang/Class;)Z which has the following shape:
    *
    *     \code
    *     icall <java/lang/Class.IsAssignableFrom(Ljava/lang/Class;)Z>
    *       <cast class object>
    *       <class object to be checked>
    *     \endcode
    */
   void process_java_lang_Class_IsAssignableFrom(TR::TreeTop* treetop, TR::Node* node);

   /** \brief
    *     Transforms java/lang/Class.cast(Ljava/lang/Object;)Ljava/lang/Object;
    *     into a checkcast node.
    *
    *  \param treetop
    *     The treetop which anchors the call node.
    *
    *  \param node
    *     The call node for java/lang/Class.cast(Ljava/lang/Object;)Ljava/lang/Object;
    *     which has the following shape:
    *
    *     \code
    *     acall java/lang/Class.cast(Ljava/lang/Object;)Ljava/lang/Object;
    *       <cast class object>
    *       <instance to be checked>
    *     \endcode
    */
   void process_java_lang_Class_cast(TR::TreeTop* treetop, TR::Node* node);

   /** \brief
    *     Transforms java/lang/StringCoding.encodeASCII(B[B)[B into a compiler intrinsic for codegen acceleration (i.e encodeASCII symbol).
    *
    *  \param treetop
    *     The treetop which anchors the call node.
    *
    *  \param node
    *     The call node representing a call to java/lang/StringCoding.encodeASCII(B[B)[B which has the following shape:
    *
    *     \code
    *     acall <java/lang/StringCoding.encodeASCII(B[B)[B>
    *       <coder>
    *       <val>
    *     \endcode
    */
   void process_java_lang_StringCoding_encodeASCII(TR::TreeTop* treetop, TR::Node* node);
   /** \brief
    *     Transforms java/lang/StringUTF16.toBytes([CII)[B into a fast allocate and arraycopy sequence with equivalent
    *     semantics.
    *
    *  \param treetop
    *     The treetop which anchors the call node.
    *
    *  \param node
    *     The call node representing a call to java/lang/StringUTF16.toBytes([CII)[B which has the following shape:
    *
    *     \code
    *     acall <java/lang/StringUTF16.toBytes([CII)[B>
    *       <value>
    *       <off>
    *       <len>
    *     \endcode
    */
   void process_java_lang_StringUTF16_toBytes(TR::TreeTop* treetop, TR::Node* node);
   /** \brief
    *     Transforms jdk/internal/util/ArraysSupport.vectorizedMismatch(Ljava/lang/Object;JLjava/lang/Object;JII)I
    *     into an arraycmplen, bit manipulation and iselect sequence with equivalent semantics.
    *
    *  \param treetop
    *     The treetop which anchors the call node.
    *
    *  \param node
    *     The call node representing a call to jdk/internal/util/ArraysSupport.vectorizedMismatch(Ljava/lang/Object;JLjava/lang/Object;JII)I
    *     which has the following shape:
    *
    *     \code
    *     icall  <jdk/internal/util/ArraysSupport.vectorizedMismatch(Ljava/lang/Object;JLjava/lang/Object;JII)I>
    *       <a>
    *       <aOffset>
    *       <b>
    *       <bOffset>
    *       <length>
    *       <log2ArrayIndexScale>
    *     \endcode
    *
    *  \details
    *     The call node is transformed to the following shape:
    *
    *     \code
    *     iselect ()
    *       lcmpeq
    *         arraycmplen
    *           aladd
    *             <a>
    *             <aOffset>
    *           aladd
    *             <b>
    *             <bOffset>
    *           land
    *             lshl
    *               i2l
    *                 <length>
    *               <log2ArrayIndexScale>
    *             lxor
    *               lor
    *                 lshl
    *                   ==>i2l
    *                   iconst 1
    *                 lconst 3
    *               lconst -1
    *         ==>land
    *       ixor
    *         l2i
    *           lshr
    *             land
    *               ==>lshl
    *               ==>lor
    *             <log2ArrayIndexScale>
    *         iconst -1
    *       l2i
    *         lshr
    *           ==>arraycmplen
    *           <log2ArrayIndexScale>
    *     \endcode
    *
    *     This transformation is valid because vectorizedMismatch is functionally equivalent to the following pseudocode
    *
    *     \code
    *     vectorizedMismatch(a, aOffset, b, bOffset, length, log2ArrayIndexScale) {
    *         lengthInBytes = length << log2ArrayIndexScale
    *
    *         // the following mask calculation is equivalent to 'mask = (log2ArrayIndexScale<2) ? 3 : 7', assuming log2ArrayIndexScale <= 3
    *         // the original java implementation checks multiple of 8B at a time, but for elements smaller than 4B it also checks another 4B at the end
    *         // this mask serves to round down to nearest multiple of 8B (or 4B if the element is smaller than 4B) and get remainder
    *         mask = (log2ArrayIndexScale<<1) | 3
    *
    *         lengthToCompare = lengthInBytes & ~(mask)                          // round down to nearest multiple of 8 (or 4)
    *         mismatchIndex = arrayCmpLen(a+aOffset, b+bOffset, lengthToCompare)
    *         if (mismatchIndex == lengthToCompare)                              // no mismatch found
    *             return ~((lengthInBytes & mask) >> log2ArrayIndexScale)        // inverted remainder, converted from byte-wise index to element-wise index
    *         else                                                               // mismatch found
    *             return mismatchIndex >> log2ArrayIndexScale                    // convert byte-wise index to element-wise index
    *     }
    *     \endcode
    */
   void process_jdk_internal_util_ArraysSupport_vectorizedMismatch(TR::TreeTop* treetop, TR::Node* node);
   /** \brief
    *     Transforms java/lang/StrictMath.sqrt(D)D and java/lang/Math.sqrt(D)D into a CodeGen inlined function with equivalent semantics.
    *
    *  \param treetop
    *     The treetop which anchors the call node.
    *
    *  \param node
    *     The call node representing a call to java/lang/StrictMath.sqrt(D)D which has the following shape:
    *
    *     \code
    *     dcall  java/lang/StrictMath.sqrt(D)D or java/lang/Math.sqrt(D)D
    *       <jclass>
    *       <value>
    *     \endcode
    */
   void process_java_lang_StrictMath_and_Math_sqrt(TR::TreeTop* treetop, TR::Node* node);
   /** \brief
    *     Transforms certain Unsafe atomic helpers into a CodeGen inlined helper with equivalent semantics.
    *
    *  \param treetop
    *     The treetop which anchors the call node.
    *
    *  \param helper
    *     The CodeGen inlined helper being transformed into
    *
    *  \param needsNullCheck
    *     Flag indicating if null check is needed on the first argument of the unsafe call
    */
   void processUnsafeAtomicCall(TR::TreeTop* treetop, TR::SymbolReferenceTable::CommonNonhelperSymbol helper, bool needsNullCheck = false);
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
   /** \brief
    *     Transforms java/lang/MethodHandle.invokeBasic calls when receiver MethodHandle (first arg) is not a known object.
    *     invokeBasic is a VM INL call that would construct the call frame for the method handle invocation. This
    *     would be the case even if the method to be invoked is compiled, resulting in j2i and i2j transitions. This
    *     transformation creates an alternate conditional path for such invocations to directly call the compiled
    *     method if the method is compiled.
    *
    *  \param treetop
    *     the TreeTop anchoring the call node
    *
    *  \param node
    *     the call node representing the invokeBasic call
    */
   void process_java_lang_invoke_MethodHandle_invokeBasic(TR::TreeTop * treetop, TR::Node* node);
   /** \brief
    *     Transforms java/lang/MethodHandle.linkToStatic and java/lang/MethodHandle.linkToSpecial calls when the memberName
    *     (last arg) is not a known object. linkToStatic and linkToSpecial are VM INL calls that would construct the call
    *     frame for the target method invocation. This would be the case even if the method to be invoked is compiled,
    *     resulting in j2i and i2j transitions. This transformation creates an alternate conditional path for such invocations
    *     to directly call the compiled method if the method is compiled. The transformation is skipped if the linkToStatic call
    *     was created as a result of unresolved invokedynamic and invokehandle, as the VM needs to check if the appendix object
    *     pushed as the second last argument is NULL, which cannot be determined at compile time.
    *
    *  \param treetop
    *     the TreeTop anchoring the call node
    *
    *  \param node
    *     the call node representing the linkToStatic call
    */
   void process_java_lang_invoke_MethodHandle_linkToStaticSpecial(TR::TreeTop * treetop, TR::Node* node);

   /** \brief
    *     Transforms java/lang/MethodHandle.linkToVirtual when the MemberName object (last arg) is not a known object.
    *     linkToVirtual is a VM INL call that would construct the call frame for the target virtual method invocation.
    *     This would be the case even if the method to be invoked is compiled, resulting in j2i and i2j transitions.
    *     The JITHelper method dispatchVirtual will be used to dispatch to the vtable entry for the method directly
    *     and thereby avoid the transitions.
    *
    *  \param treetop
    *     the TreeTop anchoring the call node
    *
    *  \param node
    *     the call node representing the linkToVirtual call
    */
   void process_java_lang_invoke_MethodHandle_linkToVirtual(TR::TreeTop * treetop, TR::Node * node);

   /** \brief
    *     Transforms java/lang/MethodHandle.linkToInterface when the MemberName object (last arg) is not a known object.
    *
    *     linkToInterface is a VM INL call that would construct the call frame for the target method invocation.
    *     This would be the case even if the method to be invoked is compiled, resulting in j2i and i2j transitions.
    *     This transformation creates an interface dispatch to find the vtable offset and then uses it to do a computed
    *     virtual call (represented by JITHelpers.dispatchVirtual).
    *
    *     linkToInterface only needs to handle regular interface dispatch. Directly-dispatched methods (e.g. private
    *     interface instance methods, final methods of Object) will be handled by linkToSpecial(). Non-interface
    *     virtual methods (e.g. non-final methods of Object) will be handled by linkToVirtual().
    *
    *  \param treetop
    *     the TreeTop anchoring the call node
    *
    *  \param node
    *     the call node representing the linkToInterface call
    */
   void process_java_lang_invoke_MethodHandle_linkToInterface(TR::TreeTop * treetop, TR::Node * node);

   /** \brief
    *     Transforms \p node into a call to \c JITHelpers.dispatchVirtual(), calling the method whose (interpreter)
    *     vTable offset is the result \p vftOffset.
    *
    *  Additional parameters beyond \p node and \p vftOffset are not strictly necessary, but callers have already
    *  either identified or created nodes to compute them anyway.
    *
    *  \param node
    *     the linkToVirtual() or linkToInterface() call node to modify
    *
    *  \param vftOffset
    *     the (interpreter) VFT offset to use for the call
    *
    *  \param vftNode
    *     the vTable of the receiver
    *
    *  \param memberNameNode
    *     the MemberName (last argument)
    */
   void makeIntoDispatchVirtualCall(TR::Node *node, TR::Node *vftOffset, TR::Node *vftNode, TR::Node *memberNameNode);
#endif

   private:
   /**
    * \brief
    *    BitVector for keeping track of processed INL call nodes. This is required because
    *    VM INL call transformations do not eliminate or modify the the original call into
    *    a different recognized method, but move them to a successor block and a computed
    *    static call is inserted in its original place.
    */
   TR_BitVector *_processedINLCalls;
   };

}
#endif
