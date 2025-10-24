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

#include "env/J9ConstProvenanceGraph.hpp"
#include "compile/Compilation.hpp"
#include <algorithm>

J9::ConstProvenanceGraph::ConstProvenanceGraph(TR::Compilation *comp)
   : _comp(comp)
   , _edges(comp->region())
   , _emptyReferents(comp->region())
   , _seenArgPairs(comp->region())
   {
   // empty
   }

const TR::set<J9::ConstProvenanceGraph::Place> &
J9::ConstProvenanceGraph::referents(Place origin)
   {
   auto it = _edges.find(origin);
   return it == _edges.end() ? _emptyReferents : it->second;
   }

bool
J9::ConstProvenanceGraph::isConstProvenanceEnabled()
   {
   // Disable during AOT compilation. There is no known object table anyway.
   // If/when the known object table is eventually supported in AOT, it will
   // require SVM, and the relocations will specify how to find all of the
   // known objects, so it will be possible to populate the graph from scratch
   // as validations are carried out at load time.
   //
   // Also disable if class unloading is disabled (-Xnoclassgc). In that case
   // all classes are permanent, and all const refs will be equally permanent,
   // so it won't matter which classes own them.
   //
   return !_comp->compileRelocatableCode()
      && !_comp->getOption(TR_NoClassGC)
      && !_comp->getOption(TR_DisableConstProvenance);
   }

void
J9::ConstProvenanceGraph::addEdgeImpl(Place origin, Place referent)
   {
   bool ignore = referent.kind() == PlaceKind_PermanentRoot || origin == referent;
   if (trace())
      {
      const char *what = ignore ? "ignore redundant" : "accept";
      traceMsg(_comp, "    %s edge: ", what);
      trace(origin);
      trace(" -> ");
      trace(referent);
      trace("\n");
      }

   assertValidPlace(origin);
   assertValidPlace(referent);
   if (ignore)
      {
      return;
      }

   TR::set<Place> emptyEdgeSet(_comp->region());
   auto mapInsertResult = _edges.insert(std::make_pair(origin, emptyEdgeSet));
   TR::set<Place> &edgeSet = mapInsertResult.first->second;
   edgeSet.insert(referent);
   }

J9::ConstProvenanceGraph::Place
J9::ConstProvenanceGraph::place(J9ClassLoader *loader)
   {
   if (!isPermanentLoader(loader))
      {
      return Place::makeClassLoader(loader);
      }

   // Identify permanent loaders with the permanent root place. This eliminates
   // edges that would otherwise uselessly target the permanent loaders.
   if (trace())
      {
      trace("    ");
      trace(loader);
      trace(" is permanent\n");
      }

   return Place::makePermanentRoot();
   }

J9::ConstProvenanceGraph::Place
J9::ConstProvenanceGraph::place(TR_OpaqueClassBlock *clazz)
   {
   if (_comp->fej9()->isClassArray(clazz))
      {
      TR_OpaqueClassBlock *component =
         _comp->fej9()->getLeafComponentClassFromArrayClass(clazz);

      if (trace())
         {
         trace("    ");
         trace(clazz);
         trace(" is an array class with leaf component ");
         trace(component);
         trace("\n");
         }

      clazz = component;
      }

   if (_comp->fej9()->isAnonymousClass(clazz))
      {
      return Place::makeAnonymousClass((J9Class*)clazz);
      }

   // Identify the class with its loader, which has the same lifetime because
   // the class is not anonymous.
   auto loader = (J9ClassLoader*)_comp->fej9()->getClassLoader(clazz);

   if (trace())
      {
      trace("    ");
      trace(clazz);
      trace(" is defined by ");
      trace(loader);
      trace("\n");
      }

   return place(loader);
   }

J9::ConstProvenanceGraph::Place
J9::ConstProvenanceGraph::place(J9Class *clazz)
   {
   return place((TR_OpaqueClassBlock*)clazz);
   }

J9::ConstProvenanceGraph::Place
J9::ConstProvenanceGraph::place(J9ConstantPool *cp)
   {
   // Identify the constant pool with the class it belongs to, which has the
   // same lifetime.
   TR_OpaqueClassBlock *clazz = _comp->fej9()->getClassFromCP(cp);

   if (trace())
      {
      trace("    ");
      trace(cp);
      trace(" belongs to ");
      trace(clazz);
      trace("\n");
      }

   return place(clazz);
   }

J9::ConstProvenanceGraph::Place
J9::ConstProvenanceGraph::place(TR_ResolvedMethod *method)
   {
   // Identify the method with its class, which has the same lifetime.
   TR_OpaqueClassBlock *clazz = method->classOfMethod();

   if (trace())
      {
      trace("    ");
      trace(method);
      trace(" is defined by ");
      trace(clazz);
      trace("\n");
      }

   return place(clazz);
   }

J9::ConstProvenanceGraph::Place
J9::ConstProvenanceGraph::place(TR_OpaqueMethodBlock *method)
   {
   // Identify the method with its class, which has the same lifetime.
   TR_OpaqueClassBlock *clazz = _comp->fej9()->getClassOfMethod(method);

   if (trace())
      {
      trace("    ");
      trace(method);
      trace(" is defined by ");
      trace(clazz);
      trace("\n");
      }

   return place(clazz);
   }

J9::ConstProvenanceGraph::Place
J9::ConstProvenanceGraph::place(J9Method *method)
   {
   return place((TR_OpaqueMethodBlock*)method);
   }

J9::ConstProvenanceGraph::Place
J9::ConstProvenanceGraph::place(KnownObject koi)
   {
   return Place::makeKnownObject(koi._i);
   }

bool
J9::ConstProvenanceGraph::trace()
   {
   return _comp->getOption(TR_TraceConstProvenance);
   }

void
J9::ConstProvenanceGraph::trace(Place p)
   {
   switch (p.kind())
      {
      case PlaceKind_PermanentRoot:
         traceMsg(_comp, "permanent root");
         break;

      case PlaceKind_ClassLoader:
         traceMsg(_comp, "class loader %p", p.getClassLoader());
         break;

      case PlaceKind_AnonymousClass:
         traceMsg(_comp, "anonymous class %p", p.getAnonymousClass());
         break;

      case PlaceKind_KnownObject:
         traceMsg(_comp, "obj%d", p.getKnownObject());
         break;
      }
   }

void
J9::ConstProvenanceGraph::trace(const char *s)
   {
   traceMsg(_comp, "%s", s);
   }

void
J9::ConstProvenanceGraph::trace(J9ClassLoader *loader)
   {
   traceMsg(_comp, "class loader %p", loader);
   }

void
J9::ConstProvenanceGraph::trace(TR_OpaqueClassBlock *clazz)
   {
   int32_t len = 0;
   const char *name = TR::Compiler->cls.classNameChars(_comp, clazz, len);
   traceMsg(_comp, "class %p %.*s", clazz, len, name);
   }

void
J9::ConstProvenanceGraph::trace(J9Class *clazz)
   {
   trace((TR_OpaqueClassBlock*)clazz);
   }

void
J9::ConstProvenanceGraph::trace(J9ConstantPool *cp)
   {
   traceMsg(_comp, "constant pool %p", cp);
   }

void
J9::ConstProvenanceGraph::trace(TR_ResolvedMethod *method)
   {
   traceMsg(
      _comp,
      "method %p %.*s.%.*s%.*s",
      method->getNonPersistentIdentifier(),
      method->classNameLength(),
      method->classNameChars(),
      method->nameLength(),
      method->nameChars(),
      method->signatureLength(),
      method->signatureChars());
   }

void
J9::ConstProvenanceGraph::trace(TR_OpaqueMethodBlock *method)
   {
   char buf[1024];
   const char *sig =
      _comp->fej9()->sampleSignature(method, buf, sizeof(buf), _comp->trMemory());

   traceMsg(_comp, "method %p %s", method, sig);
   }

void
J9::ConstProvenanceGraph::trace(J9Method *method)
   {
   trace((TR_OpaqueMethodBlock*)method);
   }

void
J9::ConstProvenanceGraph::trace(KnownObject koi)
   {
   traceMsg(_comp, "obj%d", koi._i);
   }

bool
J9::ConstProvenanceGraph::isPermanentLoader(J9ClassLoader *loader)
   {
   // NOTE: Test for presence in _comp->permanentLoaders() instead of checking
   // for J9CLASSLOADER_OUTLIVING_LOADERS_PERMANENT to ensure consistency with
   // J9::RetainedMethodSet and consistency over time.
   //
   // -- RetainedMethodSet (RMS) --
   //
   // Because RMS uses _comp->permanentLoaders(), which is determined once for
   // the compilation and cached, we'll get exactly the same loaders here.
   //
   // The actual requirement w.r.t. RMS is that every loader that it considers
   // to be permanent in this compilation must also be considered permanent
   // here. Otherwise, to see what kind of problem could arise, suppose loader
   // L is known to be permanent by RMS but not ConstProvenanceGraph (CPG).
   // It's possible to inline a profiled target or single implementer defined
   // by L without a bond. If a known object is encountered starting from (a
   // method defined by a class defined by) L, it won't necessarily be
   // reachable in the graph from the outermost method, but it also won't
   // necessarily be reachable from a bond method, since no bond was generated
   // to allow the inlining. So the known object could be unreachable.
   //
   // Checking J9CLASSLOADER_OUTLIVING_LOADERS_PERMANENT wouldn't necessarily
   // allow this requirement to be violated, depending on the detailed order in
   // which operations are performed in the compilation, but by using the exact
   // same loaders, it's obvious that the requirement is met.
   //
   // -- Consistency over time --
   //
   // It's important to use the same permanent loaders for CPG throughout the
   // compilation, and looking for J9CLASSLOADER_OUTLIVING_LOADERS_PERMANENT
   // could allow us to treat a loader first as unloadable but then later as
   // permanent. To see the problem, suppose we're compiling a method defined
   // by (a class defined by) some permanent loader L that, at the start of the
   // compilation, is not yet known to the JIT to be permanent. Then suppose
   // that we encounter a known object at some path starting from L. The first
   // edge in the path will originate from L. If later during the compilation L
   // is discovered to be permanent, and if the determination here is sensitive
   // to that discovery, then when it eventually comes time to search the graph
   // to attribute the known object to an owning class, the known object might
   // not be reachable from the permanent root place, and in that case, it also
   // won't be reachable from the outermost method, since at the time of the
   // search the method would be identified as permanent and reduced to the
   // permanent root place as well. So the known object could be unreachable.
   //
   auto permanentLoaders = _comp->permanentLoaders();
   auto end = permanentLoaders.end();
   return std::find(permanentLoaders.begin(), end, loader) != end;
   }

void
J9::ConstProvenanceGraph::assertValidPlace(Place p)
   {
   switch (p.kind())
      {
      case PlaceKind_ClassLoader:
         {
         J9ClassLoader *loader = p.getClassLoader();
         TR_ASSERT_FATAL(loader != NULL, "class loader place with null pointer");

         // If the loader is permanent, then we should use the permanent root
         // place instead.
         TR_ASSERT_FATAL(
            !isPermanentLoader(loader),
            "class loader place for loader %p, which is permanent",
            loader);

         break;
         }

      case PlaceKind_AnonymousClass:
         {
         J9Class *clazz = p.getAnonymousClass();
         TR_ASSERT_FATAL(clazz != NULL, "anonymous class place with null pointer");
         TR_ASSERT_FATAL(
            _comp->fej9()->isAnonymousClass((TR_OpaqueClassBlock*)clazz),
            "anonymous class place for non-anonymous class %p",
            p.getAnonymousClass());

         break;
         }

      case PlaceKind_KnownObject:
         {
         TR::KnownObjectTable *knot = _comp->getKnownObjectTable();
         TR_ASSERT_FATAL(
            knot != NULL, "known object place without known object table");

         TR::KnownObjectTable::Index koi = p.getKnownObject();
         TR_ASSERT_FATAL(
            0 <= koi && koi < knot->getEndIndex(),
            "known object place koi=%d out of bounds (%d)",
            koi,
            knot->getEndIndex());

         TR_ASSERT_FATAL(!knot->isNull(koi), "known object place with null index");
         break;
         }

      default:
         break;
      }
   }
