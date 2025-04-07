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

#ifndef J9_CONSTPROVENANCEGRAPH_INCL
#define J9_CONSTPROVENANCEGRAPH_INCL

#include "env/KnownObjectTable.hpp"
#include "infra/Assert.hpp"
#include "infra/map.hpp"
#include "infra/set.hpp"

namespace TR { class Compilation; }
class TR_ResolvedMethod;

namespace J9 {

/**
 * \brief ConstProvenanceGraph is a graph that tracks where known objects were
 * found at compile time.
 *
 * The graph essentially consists of directed edges between \em places, where a
 * \em place is one of the following:
 * - a known object (which is what we're ultimately interested in),
 * - an anonymous class,
 * - a non-permanent class loader, or
 * - "permanent root," which stands in for all permanent class loaders.
 *
 * When adding edges, the caller does not need to make the origin and referent
 * fit into this categorization of place. Instead, the caller just passes
 * methods or classes that will be mapped down to places according to simple
 * lifetime relationships:
 * - a method has the same lifetime as its defining class,
 * - a (non-anonymous) class has the same lifetime as its defining loader, and
 * - a permanent loader has the same lifetime as "permanent root."
 *
 * The loaders considered permanent for these purposes are those that are listed
 * by TR::Compilation::permanentLoaders() for the current compilation.
 *
 * As an example of justifiable edges, suppose that some method C.foo() uses a
 * static final field D.SOME_REF. This would justify an edge from C to D (since
 * C refers to D in its constant pool) and an edge from D to the object that
 * SOME_REF refers to.
 *
 * \b Motivation
 *
 * This information will be used to determine which classes should own which
 * constant references in a given JIT body. The idea is that if some object, say
 * obj3, was found to be reachable from a class C at compile time via a path of
 * one or more "sufficiently constant" hops, i.e. references that aren't
 * \em supposed to change, then even if there is technically still a possibility
 * for references on the path to be mutated later, it's justifiable to give C a
 * direct reference to obj3 for the lifetime of the JIT body.
 *
 * Using this kind of reachability as justification for a class to own a
 * constant reference avoids a class of memory leak, where e.g.:
 * 1. Foo.foo() calls Bar.bar()
 * 2. It inlines a single implementer or profiled target BarImpl.bar().
 * 3. There is no lifetime relationship between Foo and BarImpl.
 * 4. BarImpl.bar() uses a constant reference, e.g. BarImpl.SOME_CONST_REF.
 *
 * In such a case, if Foo were to own all of the constant references for the JIT
 * body of Foo.foo(), then it would have a direct reference to the object from
 * (4). But that object wasn't necessarily reachable from Foo to begin with. For
 * example, it could be an instance of BarImpl, and in that case, the constant
 * reference would prevent unloading of BarImpl.
 *
 * Using the constant provenance graph in this example, the object from (4) will
 * not be reachable from Foo in the graph, and so Foo will not own the constant
 * reference. Instead, BarImpl will own it, preserving the possibility of
 * unloading BarImpl before Foo.
 */
class ConstProvenanceGraph
   {
   public:
   ConstProvenanceGraph(TR::Compilation *comp);

   enum PlaceKind
      {
      PlaceKind_PermanentRoot,
      PlaceKind_ClassLoader,
      PlaceKind_AnonymousClass,
      PlaceKind_KnownObject,
      };

   private: // TODO: will need to be made accessible for JITServer

   /**
    * \brief The internal representation of Place, for JITServer.
    */
   struct RawPlace
      {
      PlaceKind _kind;
      union
         {
         J9ClassLoader *_classLoader;
         J9Class *_anonymousClass;
         TR::KnownObjectTable::Index _knownObjectIndex;
         };
      };

   public:

   /**
    * \brief Place represents a \em place as described in ConstProvenanceGraph.
    */
   class Place
      {
      private:

      Place() {}

      public:

      static Place makePermanentRoot()
         {
         Place result;
         result._raw._kind = PlaceKind_PermanentRoot;
         return result;
         }

      // The loader should not be permanent, but that isn't checked here since
      // Place is just a thin wrapper around the pointer.
      static Place makeClassLoader(J9ClassLoader *classLoader)
         {
         Place result;
         result._raw._kind = PlaceKind_ClassLoader;
         result._raw._classLoader = classLoader;
         return result;
         }

      // The class should be anonymous, but that isn't checked here since Place
      // is just a thin wrapper around the pointer.
      static Place makeAnonymousClass(J9Class *clazz)
         {
         Place result;
         result._raw._kind = PlaceKind_AnonymousClass;
         result._raw._anonymousClass = clazz;
         return result;
         }

      // The known object index should be in-bounds and not the null index, i.e.
      // it should already have been assigned to some known object, but that
      // isn't checked here since Place is just a thin wrapper around the index.
      static Place makeKnownObject(TR::KnownObjectTable::Index koi)
         {
         Place result;
         result._raw._kind = PlaceKind_KnownObject;
         result._raw._knownObjectIndex = koi;
         return result;
         }

      PlaceKind kind() const { return _raw._kind; }

      J9ClassLoader *getClassLoader() const
         {
         assertKind(PlaceKind_ClassLoader);
         return _raw._classLoader;
         }

      J9Class *getAnonymousClass() const
         {
         assertKind(PlaceKind_AnonymousClass);
         return _raw._anonymousClass;
         }

      TR::KnownObjectTable::Index getKnownObject() const
         {
         assertKind(PlaceKind_KnownObject);
         return _raw._knownObjectIndex;
         }

      bool operator==(const Place &other) const
         {
         PlaceKind k = kind();
         if (k != other.kind())
            {
            return false;
            }

         switch (k)
            {
            case PlaceKind_PermanentRoot:
               return true;
            case PlaceKind_ClassLoader:
               return getClassLoader() == other.getClassLoader();
            case PlaceKind_AnonymousClass:
               return getAnonymousClass() == other.getAnonymousClass();
            case PlaceKind_KnownObject:
               return getKnownObject() == other.getKnownObject();
            }

         TR_ASSERT_FATAL(false, "unreachable");
         return false;
         }

      bool operator<(const Place &other) const
         {
         PlaceKind k1 = kind();
         PlaceKind k2 = other.kind();
         if (k1 != k2)
            {
            return k1 < k2;
            }

         std::less<void*> ptrLt;
         switch (k1)
            {
            case PlaceKind_PermanentRoot:
               return false;
            case PlaceKind_ClassLoader:
               return ptrLt(getClassLoader(), other.getClassLoader());
            case PlaceKind_AnonymousClass:
               return ptrLt(getAnonymousClass(), other.getAnonymousClass());
            case PlaceKind_KnownObject:
               return getKnownObject() < other.getKnownObject();
            }

         TR_ASSERT_FATAL(false, "unreachable");
         return false;
         }

      // TODO: to/from raw for JITServer

      private:

      void assertKind(PlaceKind kind) const
         {
         TR_ASSERT_FATAL(
            _raw._kind == kind, "expected kind %d but was %d", kind, _raw._kind);
         }

      RawPlace _raw;
      };

   /**
    * \brief Get the set of targets of edges outgoing from \p origin.
    * \param origin the starting place
    * \return the set of referents
    */
   const TR::set<Place> &referents(Place origin);

   /**
    * \brief A strongly-typed wrapper for a known object index.
    *
    * This is suitable to pass to addEdge(), which uses the arguments' types to
    * determine how to treat them.
    */
   struct KnownObject
      {
      explicit KnownObject(TR::KnownObjectTable::Index i) : _i(i) {}
      TR::KnownObjectTable::Index _i;

      bool operator==(const KnownObject &other) const { return _i == other._i; }
      };

   /**
    * \brief Convenience method to construct KnownObject.
    *
    * Callers can avoid writing out J9::ConstProvenanceGraph::KnownObject(i).
    *
    * \param i the known object index
    * \return a KnownObject with index \p i
    */
   static KnownObject knownObject(TR::KnownObjectTable::Index i)
      {
      return KnownObject(i);
      }

   /**
    * \brief Add an edge from \p origin to \p referent.
    *
    * This indicates there is a path of one or more "sufficiently constant"
    * references from \p origin to \p referent in the runtime environment, i.e.
    * class statics, constant pool, Java heap.
    *
    * \param origin where the edge/path originates from
    * \param referent the referent
    */
   template <typename T, typename U>
   void addEdge(T origin, U referent)
      {
      // The equals() test isn't really necessary, since if origin and referent
      // are equal, place() will map them to equal places, and addEdgeImpl()
      // will ignore the edge. So the main point of equals() here is just to
      // cut down on noise in the log, but it doesn't hurt to skip calling
      // place() on both arguments when possible.
      if (!isConstProvenanceEnabled()
          || isNull(referent)
          || equals(origin, referent))
         {
         return;
         }

      // Check whether these exact arguments have been seen before. This is
      // also not strictly necessary, but cuts down on noise in the log. It's
      // done regardless of tracing so that enabling tracing won't have any
      // effect on the logic.
      auto pair = std::make_pair(ArgKey(origin), ArgKey(referent));
      if (_seenArgPairs.count(pair) != 0)
         {
         return; // We've already had these exact arguments.
         }

      _seenArgPairs.insert(pair);

      if (trace())
         {
         trace("Const provenance: add edge: ");
         trace(origin);
         trace(" -> ");
         trace(referent);
         trace("\n");
         }

      // This forces all tracing from place(origin) to appear in the log before
      // any tracing from place(referent).
      Place originPlace = place(origin);
      Place referentPlace = place(referent);
      addEdgeImpl(originPlace, referentPlace);
      }

   /**
    * \brief Determine whether constant provenance is enabled.
    *
    * If it's disabled, then ConstProvenanceGraph will still exist, but no edges
    * will be added.
    *
    * \return true if constant provenance is enabled, false otherwise
    */
   bool isConstProvenanceEnabled();

   bool trace(); // true if tracing is enabled for constant provenance

   // Convert various types to Place. These are mainly used in addEdge(), but
   // they can also be useful for callers to get a starting point from which to
   // call referents().
   Place place(J9ClassLoader *loader);
   Place place(TR_OpaqueClassBlock *clazz);
   Place place(J9Class *clazz);
   Place place(J9ConstantPool *cp);
   Place place(TR_ResolvedMethod *method);
   Place place(TR_OpaqueMethodBlock *method);
   Place place(J9Method *method);
   Place place(KnownObject koi);

   // Trace various kinds of values (whether trace() is true or not).
   void trace(const char *s); // verbatim message
   void trace(J9ClassLoader *loader);
   void trace(TR_OpaqueClassBlock *clazz);
   void trace(J9Class *clazz);
   void trace(J9ConstantPool *cp);
   void trace(TR_ResolvedMethod *method);
   void trace(TR_OpaqueMethodBlock *method);
   void trace(J9Method *method);
   void trace(KnownObject koi);
   void trace(Place p);

   private:

   void addEdgeImpl(Place origin, Place referent);

   static bool isNull(void *p) { return p == NULL; }
   static bool isNull(KnownObject koi) { return false; }

   static bool equals(void *x, void *y) { return x == y; }
   static bool equals(const KnownObject &x, const KnownObject &y) { return x == y; }
   static bool equals(const KnownObject &x, void *y) { return false; }
   static bool equals(void *x, const KnownObject &y) { return false; }

   bool isPermanentLoader(J9ClassLoader *loader);
   void assertValidPlace(Place p);

   enum ArgKeyKind
      {
      ArgKeyKind_Pointer,
      ArgKeyKind_KnownObject,
      };

   struct ArgKey
      {
      ArgKeyKind _kind;
      union
         {
         void *_pointer;
         TR::KnownObjectTable::Index _knownObjectIndex;
         };

      explicit ArgKey(void *p) : _kind(ArgKeyKind_Pointer), _pointer(p) {}
      explicit ArgKey(KnownObject koi)
         : _kind(ArgKeyKind_KnownObject), _knownObjectIndex(koi._i) {}

      bool operator<(const ArgKey &other) const
         {
         if (_kind != other._kind)
            {
            return _kind < other._kind;
            }
         else if (_kind == ArgKeyKind_Pointer)
            {
            return std::less<void*>()(_pointer, other._pointer);
            }
         else
            {
            return _knownObjectIndex < other._knownObjectIndex;
            }
         }
      };

   TR::Compilation * const _comp;
   TR::map<Place, TR::set<Place>> _edges;
   const TR::set<Place> _emptyReferents;
   TR::set<std::pair<ArgKey, ArgKey>> _seenArgPairs;
   };

} // namespace J9

#endif // J9_CONSTPROVENANCEGRAPH_INCL
