#ifndef JAAS_CHTABLE_H
#define JAAS_CHTABLE_H

#include "compile/VirtualGuard.hpp"            // for TR_VirtualGuard
#include "il/SymbolReference.hpp"              // for SymbolReference

struct VirtualGuardInfoForCHTable
   {
   TR_VirtualGuardTestType _testType;
   TR_VirtualGuardKind _kind;
   int16_t _calleeIndex;
   int32_t _byteCodeIndex;

   // non-null for guarded-devirtualizations only
   bool _isInlineGuard;
   TR_OpaqueClassBlock *_guardedMethodThisClass;

   // used for Interface/Method, Abstract/Method, Hierarchy/Method
   TR_OpaqueClassBlock *_thisClass;
   bool _mergedWithHCRGuard;
   bool _mergedWithOSRGuard;

   // These reference locations are non-null only for MutableCallSiteGuards
   uintptrj_t *_mutableCallSiteObject;
   TR::KnownObjectTable::Index _mutableCallSiteEpoch;

   // Part of the symref
   int32_t _cpIndex;
   TR_ResolvedMethod *_owningMethod;
   bool _isInterface;
   TR_ResolvedMethod *_guardedMethod;
   int32_t _offset;
   bool _hasResolvedMethodSymbol;

   // these two are hoisted up into a tuple for serialization
   //std::vector<TR_VirtualGuardSite> _nopSites;
   //std::vector<VirtualGuardInfoForCHTable> _innerAssumptions;
   };

using VirtualGuardForCHTable = std::tuple<VirtualGuardInfoForCHTable, std::vector<TR_VirtualGuardSite>, std::vector<VirtualGuardInfoForCHTable>>;

using CHTableCommitData = std::tuple<
      std::vector<TR_OpaqueClassBlock*>, // classes
      std::vector<TR_OpaqueClassBlock*>, // classesThatShouldNotBeNewlyExtended
      std::vector<TR_ResolvedMethod*>, // preXMethods
      std::vector<TR_VirtualGuardSite>, // sideEffectPatchSites
      std::vector<VirtualGuardForCHTable>, // vguards
      uint8_t*>; // startPC


bool jaasCHTableCommit(
      TR::Compilation *comp,
      TR_MethodMetaData *metaData,
      CHTableCommitData &data);

#endif // JAAS_CHTABLE_H
