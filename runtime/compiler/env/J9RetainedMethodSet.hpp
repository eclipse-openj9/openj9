/*******************************************************************************
 * Copyright IBM Corp. and others 2024
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
#include "infra/set.hpp"

#if defined(J9VM_OPT_JITSERVER)
#include <vector>
#endif

struct J9Class;
struct J9ClassLoader;
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

   RetainedMethodSet(
      TR::Compilation *comp,
      TR_ResolvedMethod *method,
      J9::RetainedMethodSet *parent);

   public:

#if defined(J9VM_OPT_JITSERVER)
   // This is used on the client for collecting any new entries added by scan()
   // so that they can be sent to the server.
   struct ScanLog
      {
      // NOTE: A single scan will add at most one anonymous class.
      J9Class *_addedAnonClass; // NULL if none
      std::vector<J9ClassLoader*> _addedLoaders;

      ScanLog() : _addedAnonClass(NULL) {}
      };
#endif

   static J9::RetainedMethodSet *create(
      TR::Compilation *comp, TR_ResolvedMethod *method
#if defined(J9VM_OPT_JITSERVER)
      , ScanLog *scanLog = NULL
#endif
      );

#if defined(J9VM_OPT_JITSERVER)
   virtual J9::RetainedMethodSet *createChild(TR_ResolvedMethod *method)
      {
      return createChild(method, NULL);
      }

   J9::RetainedMethodSet *createChild(TR_ResolvedMethod *method, ScanLog *scanLog);
#else
   virtual J9::RetainedMethodSet *createChild(TR_ResolvedMethod *method);
#endif

   virtual J9::RetainedMethodSet *withKeepalivesAttested();

   virtual bool willRemainLoaded(TR_ResolvedMethod *method);
   bool willRemainLoaded(J9Class *clazz);
   bool willRemainLoaded(J9ClassLoader *loader);
   virtual void attestLinkedCalleeWillRemainLoaded(TR_ByteCodeInfo bci);
   virtual void keepalive();
   virtual void bond();

#if defined(J9VM_OPT_JITSERVER)
   void scanForClient(J9Class *clazz, ScanLog *scanLog);
   void *remoteMirror() { return _remoteMirror; }
#endif

   protected:

   J9::RetainedMethodSet *parent()
      {
      return static_cast<J9::RetainedMethodSet*>(OMR::RetainedMethodSet::parent());
      }

   virtual void mergeIntoParent();
   virtual void *unloadingKey(TR_ResolvedMethod *method);

   private:

   void init();
   void addPermanentLoader(J9ClassLoader *loader, const char *name);
   void attestWillRemainLoaded(TR_ResolvedMethod *method);
   void scan(J9Class *clazz);
   void scan(J9ClassLoader *loader);
   bool willAnonymousClassRemainLoaded(J9Class *clazz);
   J9ClassLoader *getLoader(J9Class *clazz);
   bool isAnonymousClass(J9Class *clazz);

#if defined(J9VM_OPT_JITSERVER)
   void initWithScanLog(ScanLog *scanLog);
   void logAddedLoader(J9ClassLoader *loader);
#endif

   TR::set<J9ClassLoader*> _loaders;
   TR::set<J9Class*> _anonClasses;

#if defined(J9VM_OPT_JITSERVER)
   union
      {
      void *_remoteMirror; // used on the server
      ScanLog *_scanLog; // used on the client
      };
#endif
   };

} // namespace TR

#endif // J9_RETAINEDMETHODSET_INCL
