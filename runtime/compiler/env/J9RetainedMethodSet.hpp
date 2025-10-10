/*******************************************************************************
 * Copyright IBM Corp. and others 2025
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

#ifndef J9_RETAINEDMETHODSET_INCL
#define J9_RETAINEDMETHODSET_INCL

#include "env/OMRRetainedMethodSet.hpp"
#include "env/ResolvedInlinedCallSite.hpp"
#include "infra/set.hpp"
#include "infra/vector.hpp"

struct J9Class;
struct J9ClassLoader;
struct J9JITExceptionTable;
namespace TR { class Compilation; }
class TR_ResolvedMethod;

namespace J9 {

/**
 * \brief An OMR::RetainedMethodSet that is represented as a set of
 * J9ClassLoader pointers and a set of anonymous J9Class pointers.
 *
 * See OMR::RetainedMethodSet. Virtual methods in this class are just overrides.
 */
class RetainedMethodSet : public OMR::RetainedMethodSet
   {
   private:

   class InliningTable;
   class CompInliningTable;
   class VectorInliningTable;

   RetainedMethodSet(
      TR::Compilation *comp,
      TR_ResolvedMethod *method,
      J9::RetainedMethodSet *parent,
      InliningTable *inliningTable);

   public:

   static J9::RetainedMethodSet *create(
      TR::Compilation *comp, TR_ResolvedMethod *method);

   static J9::RetainedMethodSet *create(
      TR::Compilation *comp,
      TR_ResolvedMethod *method,
      const TR::vector<J9::ResolvedInlinedCallSite, TR::Region&> &inliningTable);

   static const TR::vector<J9::ResolvedInlinedCallSite, TR::Region&> &
      copyInliningTable(TR::Compilation *comp, J9JITExceptionTable *metadata);

   virtual J9::RetainedMethodSet *createChild(TR_ResolvedMethod *method);

   virtual bool willRemainLoaded(TR_ResolvedMethod *method);
   bool willRemainLoaded(J9Class *clazz);
   bool willRemainLoaded(J9ClassLoader *loader);
   virtual J9::RetainedMethodSet *withLinkedCalleeAttested(TR_ByteCodeInfo bci);
   virtual void keepalive();
   virtual void bond();
   virtual J9::RetainedMethodSet *withKeepalivesAttested();
   virtual J9::RetainedMethodSet *withBondsAttested();

   // This is protected in OMR::RetainedMethodSet, but it's useful for
   // J9::RepeatRetainedMethodsAnalysis::analyzeOnClient(), and it doesn't hurt
   // for it to be public.
   virtual void *unloadingKey(TR_ResolvedMethod *method);

   protected:

   struct KeepalivesAndBonds : public OMR::RetainedMethodSet::KeepalivesAndBonds
      {
      // Set contents implied by the keepalives, for withKeepalivesAttested().
      TR::set<J9ClassLoader*> _keepaliveLoaders;
      TR::set<J9Class*> _keepaliveAnonClasses;

      // Set contents implied by the bonds, for withBondsAttested().
      TR::set<J9ClassLoader*> _bondLoaders;
      TR::set<J9Class*> _bondAnonClasses;

      KeepalivesAndBonds(TR::Region &heapRegion);
      };

   J9::RetainedMethodSet *parent()
      {
      return static_cast<J9::RetainedMethodSet*>(OMR::RetainedMethodSet::parent());
      }

   J9::RetainedMethodSet::KeepalivesAndBonds *keepalivesAndBonds()
      {
      return static_cast<J9::RetainedMethodSet::KeepalivesAndBonds*>(
         OMR::RetainedMethodSet::keepalivesAndBonds());
      }

   virtual J9::RetainedMethodSet::KeepalivesAndBonds *createKeepalivesAndBonds();

   private:

   void init();
   void attestWillRemainLoaded(TR_ResolvedMethod *method);
   void scan(J9Class *clazz);
   void scan(J9ClassLoader *loader);
   bool willAnonymousClassRemainLoaded(J9Class *clazz);
   J9ClassLoader *getLoader(J9Class *clazz);
   bool isAnonymousClass(J9Class *clazz);

   TR::set<J9ClassLoader*> _loaders;
   TR::set<J9Class*> _anonClasses;
   InliningTable * const _inliningTable;
   };

} // namespace J9

#endif // J9_RETAINEDMETHODSET_INCL
