/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#include "optimizer/CallInfo.hpp"

class TR_ResolvedMethod;


class TR_ProfileableCallSite : public  TR_IndirectCallSite
   {
   public:
      virtual bool findProfiledCallTargets (TR_CallStack *callStack, TR_InlinerBase* inliner);

   protected :
      TR_CALLSITE_INHERIT_CONSTRUCTOR_AND_TR_ALLOC(TR_ProfileableCallSite, TR_IndirectCallSite)
      //capabilities
      void findSingleProfiledReceiver(ListIterator<TR_ExtraAddressInfo>&, TR_AddressInfo * valueInfo, TR_InlinerBase* inliner);
      virtual void findSingleProfiledMethod(ListIterator<TR_ExtraAddressInfo>&, TR_AddressInfo * valueInfo, TR_InlinerBase* inliner);
   };


class TR_J9MethodHandleCallSite : public  TR_FunctionPointerCallSite
   {
   public:
      TR_CALLSITE_INHERIT_CONSTRUCTOR_AND_TR_ALLOC(TR_J9MethodHandleCallSite, TR_FunctionPointerCallSite)
      virtual bool findCallSiteTarget (TR_CallStack *callStack, TR_InlinerBase* inliner);
		virtual const char*  name () { return "TR_J9MethodHandleCallSite"; }
   };


class TR_J9MutableCallSite : public  TR_FunctionPointerCallSite
   {
   public:
      TR_CALLSITE_INHERIT_CONSTRUCTOR_AND_TR_ALLOC(TR_J9MutableCallSite, TR_FunctionPointerCallSite)
      virtual bool findCallSiteTarget (TR_CallStack *callStack, TR_InlinerBase* inliner);
		virtual const char*  name () { return "TR_J9MutableCallSite"; }
   };

class TR_J9VirtualCallSite : public TR_ProfileableCallSite
   {
   public:
      TR_CALLSITE_INHERIT_CONSTRUCTOR_AND_TR_ALLOC(TR_J9VirtualCallSite, TR_ProfileableCallSite)
      virtual bool findCallSiteTarget (TR_CallStack *callStack, TR_InlinerBase* inliner);
      virtual TR_ResolvedMethod* findSingleJittedImplementer (TR_InlinerBase *inliner);
      virtual const char*  name () { return "TR_J9VirtualCallSite"; }

   protected:
		//capabilities
		bool findCallSiteForAbstractClass(TR_InlinerBase* inliner);
		//queries
      virtual TR_OpaqueClassBlock* getClassFromMethod ();

   };

class TR_J9InterfaceCallSite : public TR_ProfileableCallSite
   {

   public:
      TR_CALLSITE_INHERIT_CONSTRUCTOR_AND_TR_ALLOC(TR_J9InterfaceCallSite, TR_ProfileableCallSite)
      virtual bool findCallSiteTarget (TR_CallStack *callStack, TR_InlinerBase* inliner);
      virtual TR_OpaqueClassBlock* getClassFromMethod ();
		virtual const char*  name () { return "TR_J9InterfaceCallSite"; }
   protected:
      virtual TR_ResolvedMethod* getResolvedMethod (TR_OpaqueClassBlock* klass);
      virtual void findSingleProfiledMethod(ListIterator<TR_ExtraAddressInfo>&, TR_AddressInfo * valueInfo, TR_InlinerBase* inliner);

   };

