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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "optimizer/CallInfo.hpp"
#include "il/J9DataTypes.hpp"

class TR_ResolvedMethod;


class TR_ProfileableCallSite : public  TR_IndirectCallSite
   {
   public:
      virtual bool findProfiledCallTargets (TR_CallStack *callStack, TR_InlinerBase* inliner);

   protected :
      TR_CALLSITE_TR_ALLOC_AND_INHERIT_EMPTY_CONSTRUCTOR(TR_ProfileableCallSite, TR_IndirectCallSite)
      //capabilities
      void findSingleProfiledReceiver(ListIterator<TR_ExtraAddressInfo>&, TR_AddressInfo * valueInfo, TR_InlinerBase* inliner);
      virtual void findSingleProfiledMethod(ListIterator<TR_ExtraAddressInfo>&, TR_AddressInfo * valueInfo, TR_InlinerBase* inliner);
      virtual TR_YesNoMaybe isCallingObjectMethod() { return TR_maybe; };
   };


class TR_J9MethodHandleCallSite : public  TR_FunctionPointerCallSite
   {
   public:
      TR_CALLSITE_TR_ALLOC_AND_INHERIT_EMPTY_CONSTRUCTOR(TR_J9MethodHandleCallSite, TR_FunctionPointerCallSite)
      virtual bool findCallSiteTarget (TR_CallStack *callStack, TR_InlinerBase* inliner);
		virtual const char*  name () { return "TR_J9MethodHandleCallSite"; }
   };


class TR_J9MutableCallSite : public  TR_FunctionPointerCallSite
   {
   public:
      TR_CALLSITE_TR_ALLOC_AND_INHERIT_CONSTRUCTOR(TR_J9MutableCallSite, TR_FunctionPointerCallSite) { _mcsReferenceLocation = NULL; };
      virtual bool findCallSiteTarget (TR_CallStack *callStack, TR_InlinerBase* inliner);
		virtual const char*  name () { return "TR_J9MutableCallSite"; }
      virtual void setMCSReferenceLocation(uintptr_t *mcsReferenceLocation) { _mcsReferenceLocation = mcsReferenceLocation; }
   private:
      uintptr_t * _mcsReferenceLocation;
   };

class TR_J9VirtualCallSite : public TR_ProfileableCallSite
   {
   public:
      TR_CALLSITE_TR_ALLOC_AND_INHERIT_CONSTRUCTOR(TR_J9VirtualCallSite, TR_ProfileableCallSite) { _isCallingObjectMethod = TR_maybe; }
      virtual bool findCallSiteTarget (TR_CallStack *callStack, TR_InlinerBase* inliner);
      virtual TR_ResolvedMethod* findSingleJittedImplementer (TR_InlinerBase *inliner);
      virtual const char*  name () { return "TR_J9VirtualCallSite"; }

   protected:
		//capabilities
		bool findCallSiteForAbstractClass(TR_InlinerBase* inliner);
		//queries
		bool isBasicInvokeVirtual();
      virtual TR_OpaqueClassBlock* getClassFromMethod ();
      // Is the call site calling a method of java/lang/Object
      virtual TR_YesNoMaybe isCallingObjectMethod() { return _isCallingObjectMethod; };
   private:
      TR_YesNoMaybe _isCallingObjectMethod;

   };

class TR_J9InterfaceCallSite : public TR_ProfileableCallSite
   {

   public:
      TR_CALLSITE_TR_ALLOC_AND_INHERIT_EMPTY_CONSTRUCTOR(TR_J9InterfaceCallSite, TR_ProfileableCallSite)
      virtual bool findCallSiteTarget (TR_CallStack *callStack, TR_InlinerBase* inliner);
      bool findCallSiteTargetImpl (TR_CallStack *callStack, TR_InlinerBase *inliner, TR_OpaqueClassBlock *iface);
      virtual TR_OpaqueClassBlock* getClassFromMethod ();
		virtual const char*  name () { return "TR_J9InterfaceCallSite"; }
   protected:
      virtual TR_ResolvedMethod* getResolvedMethod (TR_OpaqueClassBlock* klass);
      virtual void findSingleProfiledMethod(ListIterator<TR_ExtraAddressInfo>&, TR_AddressInfo * valueInfo, TR_InlinerBase* inliner);

   };

