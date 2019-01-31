/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifndef S390REGISTER_INCL
#define S390REGISTER_INCL

#include <stddef.h>
#include <stdint.h>
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"

class TR_OpaquePseudoRegister;
class TR_PseudoRegister;
namespace TR { class AutomaticSymbol; }
namespace TR { class Compilation; }
namespace TR { class Symbol; }
namespace TR { class SymbolReference; }
template <typename ListKind> class List;

class TR_StorageReference
   {
   public:

   TR_ALLOC(TR_Memory::StorageReference)

   TR_StorageReference(TR::SymbolReference *t, TR::Compilation *c);
   TR_StorageReference(TR::Node *n, int32_t refCount, TR::Compilation *c);

   static TR_StorageReference *createTemporaryBasedStorageReference(TR::SymbolReference *t, TR::Compilation *c);
   static TR_StorageReference *createTemporaryBasedStorageReference(int32_t size, TR::Compilation *c);
   static TR_StorageReference *createNodeBasedStorageReference(TR::Node *n, int32_t refCount, TR::Compilation *c);
   static TR_StorageReference *createNodeBasedHintStorageReference(TR::Node *n, TR::Compilation *c);

   TR::AutomaticSymbol*  getTemporarySymbol();

   TR::SymbolReference *getTemporarySymbolReference() { return _temporary; }

   int32_t getReferenceNumber();
   TR::Symbol *getSymbol();
   TR::SymbolReference *getSymbolReference();

   TR::Node *getNode() { return _node; }

   rcount_t getNodeReferenceCount();
   rcount_t incrementNodeReferenceCount(rcount_t increment=1);
   rcount_t decrementNodeReferenceCount(rcount_t decrement=1);
   rcount_t setNodeReferenceCount(rcount_t count);

   int32_t  getOwningRegisterCount()            { return _owningRegisterCount; }
   void     setOwningRegisterCount(int32_t c)   { _owningRegisterCount = c; }
   int32_t  incOwningRegisterCount()            { return ++_owningRegisterCount; }
   int32_t  decOwningRegisterCount()            { TR_ASSERT(_owningRegisterCount > 0, "_owningRegisterCount %d should be > 0"); return --_owningRegisterCount; }

   void decrementTemporaryReferenceCount(rcount_t decrement=1);
   void incrementTemporaryReferenceCount(rcount_t increment=1);
   void setTemporaryReferenceCount(rcount_t count);
   rcount_t getTemporaryReferenceCount();

   int32_t getSymbolSize();

   void increaseTemporarySymbolSize(int32_t sizeIncrement, TR_OpaquePseudoRegister *reg);

   bool isSingleUseTemporary();
   void setIsSingleUseTemporary();

   bool isTemporaryBased();
   bool isNodeBased();
   bool isConstantNodeBased();
   bool isNonConstantNodeBased() { return isNodeBased() && !isConstantNodeBased(); }

   List<TR::Node> *getSharedNodes() {return _sharedNodes;}
   void addSharedNode(TR::Node *node);
   void removeSharedNode(TR::Node *node);
   int32_t getMaxSharedNodeSize();

   List<TR::Node> *getNodesToUpdateOnClobber() {return _nodesToUpdateOnClobber;}
   void emptyNodesToUpdateOnClobber();
   void addNodeToUpdateOnClobber(TR::Node *node);
   void removeNodeToUpdateOnClobber(TR::Node *node);

   bool mayOverlapWith(TR_StorageReference *ref2);

   TR::Compilation *comp() { return _comp; }

   enum // _flags
      {
      IsNodeBasedHint      = 0x01,
      HintHasBeenUsed      = 0x02, // only allow a storageRefHint to be used once to avoid possible overlapping code to be generated when a parent/child share a hint
      IsReadOnlyTemporary  = 0x04, // for passThroughs and operations that do a conservative clobber (like some pdcleans) to skip/delay the clobberEvaluate
      };

   bool isNodeBasedHint()                    { return _flags.testAny(IsNodeBasedHint); }
   void setIsNodeBasedHint(bool b = true)    {_flags.set(IsNodeBasedHint, b); }

   bool hintHasBeenUsed()                 { return _flags.testAny(HintHasBeenUsed); }
   void setHintHasBeenUsed(bool b = true) { _flags.set(HintHasBeenUsed, b); }

   bool isReadOnlyTemporary()
      {
      if (isTemporaryBased())
         return _flags.testAny(IsReadOnlyTemporary);
      else
         return false;
      }

   void setIsReadOnlyTemporary(bool b, TR::Node *nodeToUpdateOnClobber);

   flags8_t setFlags(flags8_t flags)      { return (_flags = flags); }
   flags8_t getFlags()                    { return _flags; }

   private:

   TR::Compilation       *_comp;
   TR::SymbolReference   *_temporary;
   TR::Node              *_node;
   rcount_t              _nodeReferenceCount;
   int32_t              _owningRegisterCount; // how many unique registers point to this storageRef -- needed during ssrClobberEvaluate
   flags8_t             _flags;
   List<TR::Node>        *_sharedNodes;
   List<TR::Node>        *_nodesToUpdateOnClobber;
   };


// A TR_PseudoRegister is a pseudo virtual register.
// 'Pseudo' in that it will not be assigned a real register during instruction selection.
// Instead, it is used to hold a TR_StorageReference that encapsulates and abstracts the result memory location as well as other evaluated node (result) specific state.
//
// TR_PseudoRegisters come in two flavours: a base TR_OpaquePseudoRegister that is basically just a simple container for a TR_StorageReference and a TR_BCDPseudoRegister
// that contains the TR_StorageReference as well as BCD specific state
//

class TR_OpaquePseudoRegister : public TR::Register
   {
   public:

   TR_OpaquePseudoRegister(TR::DataType dt, TR::Compilation *c) : TR::Register(TR_SSR),
                                                          _comp(c),
                                                          _storageReference(NULL),
                                                          _size(0),
                                                          _dataType(dt),
                                                          _rightAlignedDeadBytes(0),
                                                          _rightAlignedIgnoredBytes(0),
                                                          _flags(0)  {}

   TR_OpaquePseudoRegister(TR_OpaquePseudoRegister *reg, TR::Compilation *c) : TR::Register(TR_SSR),
                                                         _comp(c) ,
                                                         _storageReference(NULL),
                                                         _size(reg->getSize()),
                                                         _dataType(reg->getDataType()),
                                                         _rightAlignedDeadBytes(0),
                                                         _rightAlignedIgnoredBytes(0),
                                                         _flags(reg->getFlags()) {}

   enum PseudoRegisterFlags // _flags
      {
      IsInitialized                 = 0x01,
      SignStateAssumed              = 0x02,  // Assumed - if the preferred,clean or signCode state setting is from a node flag
      SignStateKnown                = 0x04,  // Known   - if the preferred,clean or signCode state is from generated code that guarantees a particular state
      HasPreferredSign              = 0x08,  //
      HasCleanSign                  = 0x10,  //
      HasSignCode                   = 0x20,  //

      HasKnownBadSignCode           = 0x40,  // Bad - code has been generated that guarantees the sign is bad (0-9).
                                             //       These bad signs are shortlived and are dominated by a good sign setting operation
      HasKnownValidSign             = 0x80,  // Valid - sign between 0xa-0xf
      HasKnownValidData             = 0x100, // Data - digits between 0-9
      SignStateInitialized          = 0x200, // Set to true when preferred,clean or _signCode is set but also manually for operations
                                             // that have no particular sign state (loads for example)
                                             // SignStateInitialized=true does not imply that any valid or known sign state has to be set so HasKnownBadSignCode=true
                                             // *does* imply SignStateInitialized=true
      HasTemporaryKnownSignCode     = 0x400, // to improve code generation for zoned leading separate sign to x conversions a zone value (0xf) can be temporarily set on the register

      // NOTE: _flags is defined as flags16_t so this type must be widened if new flags are to be added
      };

   TR::Compilation *comp() {return _comp;}

   virtual void increaseTemporarySymbolSize(int32_t sizeIncrement);

   virtual void setStorageReference(TR_StorageReference *ref, TR::Node *node);
   virtual TR_StorageReference *getStorageReference() { return _storageReference; }

   virtual int32_t getSymbolSize();

   virtual void clearLeftAndRightAlignedState() { }
   virtual void clearLeftAlignedState() { }

   virtual int32_t getSize()           { return _size; }
   virtual int32_t setSize(int32_t s)  { return _size = s; }

   flags16_t getFlags()                { return _flags; }
   flags16_t setFlags(flags16_t flags) { return (_flags = flags); }

   virtual TR::DataType getDataType()                  { return _dataType; }
   virtual TR::DataType setDataType(TR::DataType dt) { return (_dataType = dt); }

   virtual bool isInitialized()                 { return _flags.testAny(IsInitialized); }
   virtual void setIsInitialized(bool b = true) { _flags.set(IsInitialized, b); }

   virtual bool trackZeroDigits() { return false; }

   virtual int32_t getLeftAlignedZeroDigits()   { return 0; }
   virtual int32_t setLeftAlignedZeroDigits(int32_t z) { return 0; }

   virtual void addRangeOfZeroBytes(int32_t startByte, int32_t endByte)       { return; }
   virtual void addRangeOfZeroDigits(int32_t startDigit, int32_t endDigit)    { return; }
   virtual void removeRangeOfZeroDigits(int32_t startDigit, int32_t endDigit) { return; }
   virtual void removeRangeOfZeroBytes(int32_t startByte, int32_t endByte)    { return; }

   virtual int32_t getDigitsToClear(int32_t startDigit, int32_t endDigit)     { return 0; }
   virtual int32_t getBytesToClear(int32_t startByte, int32_t endByte)        { return 0; }

   virtual int32_t getRightAlignedDeadBytes()                  { return _rightAlignedDeadBytes; }
   virtual int32_t addToRightAlignedDeadBytes(int32_t bytes)   { return setRightAlignedDeadBytes(_rightAlignedDeadBytes + bytes); }
   virtual int32_t setRightAlignedDeadBytes(int32_t bytes)
      {
      TR_ASSERT(bytes >= 0,"setRightAlignedDeadBytes value must be >=0 and not %d\n",bytes);
      return (_rightAlignedDeadBytes = bytes);
      }

   virtual int32_t getRightAlignedIgnoredBytes()                  { return _rightAlignedIgnoredBytes; }
   virtual int32_t addToRightAlignedIgnoredBytes(int32_t bytes)   { return setRightAlignedIgnoredBytes(_rightAlignedIgnoredBytes + bytes); }
   virtual int32_t setRightAlignedIgnoredBytes(int32_t bytes)
      {
      TR_ASSERT(bytes >= 0,"setRightAlignedIgnoredBytes value must be >=0 and not %d\n",bytes);
      return (_rightAlignedIgnoredBytes = bytes);
      }

   virtual int32_t getRightAlignedDeadAndIgnoredBytes()
      {
      return getRightAlignedDeadBytes() + getRightAlignedIgnoredBytes();
      }

   virtual void setRightAlignedDeadAndIgnoredBytes(int32_t bytes)
      {
      setRightAlignedDeadBytes(bytes);
      setRightAlignedIgnoredBytes(bytes);
      }

   virtual void clearRightAlignedState()
      {
      setRightAlignedDeadAndIgnoredBytes(0);
      }


   virtual int32_t getLiveSymbolSize();
   virtual int32_t getValidSymbolSize();

   virtual bool canBeConservativelyClobberedBy(TR::Node *clobberingNode) { return false; }

   virtual TR::Register              *getRegister();
   virtual TR_PseudoRegister        *getPseudoRegister();
   virtual TR_OpaquePseudoRegister  *getOpaquePseudoRegister();

   protected:
   flags16_t            _flags;
   int32_t              _size;

   private:

   TR::Compilation      *_comp;
   TR_StorageReference *_storageReference;
   TR::DataType       _dataType;

   int32_t              _rightAlignedDeadBytes;    // these bytes are have been invalidated/clobbered and must not be referenced
   int32_t              _rightAlignedIgnoredBytes; // these bytes are still valid (not clobbered/invalidated) but not used by this register

   };

class TR_PseudoRegister : public TR_OpaquePseudoRegister
   {
   public:

   TR_PseudoRegister(TR::DataType dt, TR::Compilation *c) : TR_OpaquePseudoRegister(dt, c),
                                                          _leftAlignedZeroDigits(0),
                                                          _decimalPrecision(0),
                                                          _signCode(TR::DataType::getInvalidSignCode()),
                                                          _temporaryKnownSignCode(TR::DataType::getInvalidSignCode()) {}

   TR_PseudoRegister(TR_PseudoRegister *reg, TR::Compilation *c) : TR_OpaquePseudoRegister(reg, c),
                                                                 _leftAlignedZeroDigits(0),     // do not transfer storageReference dependent values
                                                                 _decimalPrecision(reg->getDecimalPrecision()),
                                                                 _signCode(reg->hasKnownOrAssumedSignCode() ? reg->getKnownOrAssumedSignCode() : TR::DataType::getInvalidSignCode()),
                                                                 _temporaryKnownSignCode(TR::DataType::getInvalidSignCode()) {}

   virtual int32_t getSize();
   virtual int32_t setSize(int32_t size);


   virtual void clearLeftAndRightAlignedState()
      {
      clearLeftAlignedState();
      clearRightAlignedState();
      }

   virtual void clearLeftAlignedState()
      {
      setLeftAlignedZeroDigits(0);
      }

   virtual bool trackZeroDigits()
      {
      return getDataType() == TR::PackedDecimal
          || getDataType() == TR::ZonedDecimal
          ;
      }

   virtual int32_t getLeftAlignedZeroDigits()
      {
      // allow the conservative value of zero to be retrieved for all datatypes
      TR_ASSERT((_leftAlignedZeroDigits == 0) || trackZeroDigits(),
         "getLeftAlignedZeroDigits() only supported for TR::PackedDecimal and TR::ZonedDecimal (dt = %s)\n",getDataType().toString());
      return _leftAlignedZeroDigits;
      }

   virtual int32_t setLeftAlignedZeroDigits(int32_t z)
      {
      // allow the conservative value of zero to be set for all datatypes
      if (trackZeroDigits())
         return (_leftAlignedZeroDigits=z);
      else
         return _leftAlignedZeroDigits=0;
      }

   virtual void addRangeOfZeroBytes(int32_t startByte, int32_t endByte);
   virtual void addRangeOfZeroDigits(int32_t startDigit, int32_t endDigit);
   virtual void removeRangeOfZeroDigits(int32_t startDigit, int32_t endDigit);
   virtual void removeRangeOfZeroBytes(int32_t startByte, int32_t endByte);
   virtual int32_t getDigitsToClear(int32_t startDigit, int32_t endDigit);
   virtual int32_t getBytesToClear(int32_t startByte, int32_t endByte);
   virtual int32_t getByteOffsetFromLeftForClear(int32_t startDigit, int32_t endDigit, int32_t &digitsToClear, int32_t resultSize);

   // Sometimes a register value has some of its right most bytes invalidated (pdshr/pddiv for example). This value must be tracked
   // so a codegen TR_*MemoryReference created from this register can have its displacement adjust accordingly.

   virtual bool canBeConservativelyClobberedBy(TR::Node *clobberingNode);

   int32_t getRangeEnd(int32_t rangeStart, int32_t startDigit, int32_t endDigit);
   int32_t getRangeStart(int32_t startDigit, int32_t endDigit);
   int32_t getSymbolDigits();

   void removeByteRangeAfterLeftShift(int32_t operandByteSize, int32_t shiftDigitAmount);
   bool isLeftMostNibbleClear();
   void setLeftMostNibbleClear();

   int32_t getDecimalPrecision();
   int32_t setDecimalPrecision(uint16_t precision);

   bool isEvenPrecision() { return ((getDecimalPrecision() & 0x1) == 0); }
   bool isOddPrecision()  { return ((getDecimalPrecision() & 0x1) != 0); }

   bool signStateKnown()                     { return _flags.testAny(SignStateKnown); }
   void setSignStateKnown()
      {
      if (signStateAssumed())
         {
         //TR_ASSERT(false,"should not promote sign state from assumed to known\n");  // a new TR_PseudoRegister and a transferSignState should be used instead
         _flags.set(SignStateAssumed, false);
         resetSignState();
         }
      _flags.set(SignStateKnown, true);
      }

   bool signStateAssumed()                   { return _flags.testAny(SignStateAssumed); }
   void setSignStateAssumed()
      {
      if (signStateKnown())
         {
         TR_ASSERT(false,"should not regress sign state from known to assumed\n");
         _flags.set(SignStateKnown, false);
         resetSignState();
         }
      _flags.set(SignStateAssumed, true);
      }

   void resetSignStateAssumed()
      {
      if (signStateAssumed())
         {
         _flags.set(SignStateKnown, false);
         resetSignState();
         }
      }

   int32_t setSignCode(int32_t sign)
      {
      resetTemporaryKnownSignCode();

      if (sign == TR::DataType::getInvalidSignCode() || sign == TR::DataType::getIgnoredSignCode())
         {
         _flags.set(HasSignCode, false);
         }
      else
         {
         _flags.set(HasSignCode, true);
         setSignStateInitialized();
         if (signStateKnown())
            setHasKnownValidSign();
         }

      uint32_t preferredPlusSign = TR::DataType::getPreferredPlusSignCode(getDataType());
      uint32_t preferredMinusSign = TR::DataType::getPreferredMinusSignCode(getDataType());

      if (preferredPlusSign == TR::DataType::getInvalidSignCode() || preferredMinusSign == TR::DataType::getInvalidSignCode())
         {
         _flags.set(HasPreferredSign, false);
         _flags.set(HasCleanSign, false);
         return sign;
         }

      if (sign == preferredPlusSign || sign == preferredMinusSign)
         _flags.set(HasPreferredSign, true);

      if (sign == preferredPlusSign)   // a preferred plus sign is always clean (no negative zero is possible)
         {
         _flags.set(HasCleanSign, true);
         }
      else
         {
         _flags.set(HasCleanSign, false);
         if (sign != preferredMinusSign)
            _flags.set(HasPreferredSign, false);
         }
      return sign;
      }

   int32_t setKnownSignCode(int32_t sign)
      {
      setSignStateKnown();
      return _signCode = setSignCode(sign);
      }

   int32_t setAssumedSignCode(int32_t sign)
      {
      setSignStateAssumed();
      return _signCode = setSignCode(sign);
      }

   // There is no guaranteed invalid signCode value so an explicit HasAssumedSignCode/HasKnownSignCode is required to indicate the _signCode value has been set.
   // The default, unset, value of signCode is 0 (TR::DataType::getInvalidSignCode()) and even though this will likely not match any valid sign code
   // the assume is here so retrieving the knownSignCode without first checking hasKnownSignCode() is discouraged.
   int32_t getKnownSignCode()
      {
      TR_ASSERT(hasKnownSignCode(),"getKnownSignCode() should not be called unless hasKnownSignCode() is true\n");
      return _signCode;
      }

   int32_t getAssumedSignCode()
      {
      TR_ASSERT(hasAssumedSignCode(),"getAssumedSignCode() should not be called unless hasAssumedSignCode() is true\n");
      return _signCode;
      }

   int32_t getKnownOrAssumedSignCode()
      {
      TR_ASSERT(hasKnownOrAssumedSignCode(),"getKnownOrAssumedSignCode() should not be called unless hasKnownOrAssumedSignCode() is true\n");
      return _signCode;
      }

   bool knownSignCodeIs(int32_t sign)
      {
      if (hasKnownSignCode() && getKnownSignCode() == sign)
         return true;
      else
         return false;
      }

   bool assumedSignCodeIs(int32_t sign)
      {
      if (hasAssumedSignCode() && getAssumedSignCode() == sign)
         return true;
      else
         return false;
      }

   bool knownOrAssumedSignCodeIs(int32_t sign)  { return knownSignCodeIs(sign) || assumedSignCodeIs(sign); }

   bool hasKnownOrAssumedPositiveSignCode();
   bool hasKnownOrAssumedNegativeSignCode();

   bool knownSignIsZone()     { return knownSignCodeIs(TR::DataType::getZonedValue()); }
   bool assumedSignIsZone()   { return assumedSignCodeIs(TR::DataType::getZonedValue()); }
   bool knownOrAssumedSignIsZone()  { return knownOrAssumedSignCodeIs(TR::DataType::getZonedValue()); }

   bool knownSignIsEmbeddedPreferredOrUnsigned()
      {
      return knownSignCodeIs(TR::DataType::getPreferredPlusCode()) ||
             knownSignCodeIs(TR::DataType::getPreferredMinusCode()) ||
             knownSignCodeIs(TR::DataType::getUnsignedCode());
      }

   // Clean Sign -- Start
   bool hasKnownCleanSign()                     { return signStateKnown() && _flags.testAny(HasCleanSign); }
   void setHasKnownCleanSign()
      {
      setSignStateKnown();
      setHasKnownValidSign();
      setHasCleanSign();
      }

   bool hasAssumedCleanSign()                   { return signStateAssumed() && _flags.testAny(HasCleanSign); }
   void setHasAssumedCleanSign(bool b = true)
      {
      setSignStateAssumed();
      setHasCleanSign();
      }

   bool hasKnownOrAssumedCleanSign()   { return _flags.testAny(HasCleanSign); }
   void resetCleanSign()
      {
      _flags.set(HasCleanSign, false);
      }

   void transferCleanSign(TR_PseudoRegister *srcReg)
      {
      if (srcReg == NULL) return;

      if (srcReg->hasKnownCleanSign())
         setHasKnownCleanSign();
      else if (srcReg->hasAssumedCleanSign())
         setHasAssumedCleanSign();
      }
   // Clean Sign -- End


   // Preferred Sign -- Start
   bool hasKnownPreferredSign()                    { return signStateKnown() && _flags.testAny(HasPreferredSign); }
   void setHasKnownPreferredSign()
      {
      setSignStateKnown();
      setHasKnownValidSign();
      setHasPreferredSign();
      }

   bool hasAssumedPreferredSign()                  { return signStateAssumed() && _flags.testAny(HasPreferredSign); }
   void setHasAssumedPreferredSign(bool b = true)
      {
      setSignStateAssumed();
      setHasPreferredSign();
      }

   bool hasKnownOrAssumedPreferredSign()   { return _flags.testAny(HasPreferredSign); }
   // Preferred Sign -- End


   bool hasKnownSignCode()                      { return signStateKnown() && _flags.testAny(HasSignCode); }
// Only provide a getter and resetter to not encourage setting the flag without also providing a known sign code value.
//   void setHasKnownSignCode(bool b = true)   { _flags.set(HasKnownSignCode, b); }

   bool hasAssumedSignCode()                    { return signStateAssumed() && _flags.testAny(HasSignCode); }
// Only provide a getter and resetter to not encourage setting the flag without also providing a known sign code value.
//   void setHasAssumedSignCode(bool b = true)   { _flags.set(HasAssumedSignCode, b); }

   bool hasKnownOrAssumedSignCode()             { return _flags.testAny(HasSignCode); }

   bool hasKnownBadSignCode()                   { return _flags.testAny(HasKnownBadSignCode); }
   void setHasKnownBadSignCode()
      {
      resetSignState();
      setSignStateKnown();
      setSignStateInitialized();
      _flags.set(HasKnownBadSignCode, true);
      }

   bool hasKnownValidSign()                   { return _flags.testAny(HasKnownValidSign); }
   void setHasKnownValidSign()
      {
      _flags.set(HasKnownBadSignCode, false);
      _flags.set(HasKnownValidSign, true);
      }

   bool hasKnownValidData()                  { return _flags.testAny(HasKnownValidData); }
   void setHasKnownValidData()
      {
      _flags.set(HasKnownValidData, true);
      }

   bool hasKnownValidSignAndData()           { return hasKnownValidData() && hasKnownValidSign(); }
   void setHasKnownValidSignAndData()
      {
      setHasKnownValidSign();
      setHasKnownValidData();
      }

   bool signStateInitialized()               { return _flags.testAny(SignStateInitialized); }
   void setSignStateInitialized()
      {
      _flags.set(SignStateInitialized, true);
      }

   bool hasTemporaryKnownSignCode()                        { return _flags.testAny(HasTemporaryKnownSignCode); }
   int32_t  getTemporaryKnownSignCode()
      {
      TR_ASSERT(hasTemporaryKnownSignCode(),"getTemporaryKnownSignCode() should not be called unless hasTemporaryKnownSignCode() is true\n");
      return _temporaryKnownSignCode;
      }

   void setTemporaryKnownSignCode(int32_t s)
      {
      if (s == TR::DataType::getInvalidSignCode())
         _flags.set(HasTemporaryKnownSignCode, false);
      else
         _flags.set(HasTemporaryKnownSignCode, true);
      _temporaryKnownSignCode = s;
      }

   void resetTemporaryKnownSignCode()
      {
      _flags.set(HasTemporaryKnownSignCode, false);
      _temporaryKnownSignCode = TR::DataType::getInvalidSignCode();
      }

   bool temporaryKnownSignCodeIs(int32_t sign)
      {
      if (hasTemporaryKnownSignCode() && getTemporaryKnownSignCode() == sign)
         return true;
      else
         return false;
      }

   bool isLegalToCleanSign()
      {
      // signStateInitialized=true when the sign has some known state (clean, preferred, a particular value) or
      // when the register is for a load that does not assume any sign state upon the value being loaded
      // The goal here is to prevent a ZAP to be used purely for widening (as opposed to FE directed cleaning)
      // and in the process clobbing a known unsigned sign code or an optimizer (e.g. PRE) generated load that
      // could contain some known unsigned sign code that must not be clobbered.
      //
      return signStateInitialized() &&
             !knownOrAssumedSignCodeIs(TR::DataType::getUnsignedCode());
      }

   // Transfer Sign/Data -- start
   void transferSignState(TR_PseudoRegister *srcReg, bool digitsLost)
      {
      if (srcReg == NULL) return;

      resetTemporaryKnownSignCode();

      if (srcReg->hasKnownBadSignCode())
         _flags.set(HasKnownBadSignCode, true);

      if (srcReg->signStateInitialized())
         _flags.set(SignStateInitialized, true);

      // Before setting any of the optional sign state (clean,preferred,signCode) make sure the srcReg has non-conflicting known/assumed settings
      if (srcReg->signStateKnown())
         {
         if (srcReg->signStateAssumed())
            {
            TR_ASSERT(false,"srcReg sign state should not be set to both known and assume\n");
            resetSignState();
            return;
            }
         _flags.set(SignStateKnown, true);
         }

      if (srcReg->signStateAssumed())
         {
         if (srcReg->signStateKnown())
            {
            TR_ASSERT(false,"srcReg sign state should not be set to both assumed and known\n");
            resetSignState();
            return;
            }
         _flags.set(SignStateAssumed, true);
         }

      if (srcReg->hasKnownOrAssumedSignCode())
         {
         _flags.set(HasSignCode, true);
         _signCode = srcReg->getKnownOrAssumedSignCode();
         if (_signCode != TR::DataType::getInvalidSignCode() && _signCode == TR::DataType::getPreferredPlusSignCode(getDataType()))
            _flags.set(HasCleanSign, true);
         }

      if (srcReg->hasKnownOrAssumedPreferredSign())
         _flags.set(HasPreferredSign, true);

      if (!digitsLost && srcReg->hasKnownOrAssumedCleanSign())
         _flags.set(HasCleanSign, true);

      if (srcReg->hasKnownValidSign())
         _flags.set(HasKnownValidSign, true);
      }

   void transferDataState(TR_PseudoRegister *srcReg)
      {
      if (srcReg == NULL) return;

      if (srcReg->hasKnownValidData())
         _flags.set(HasKnownValidData, true);
      }

   void transferSignAndDataState(TR_PseudoRegister *srcReg, bool digitsLost)
      {
      transferSignState(srcReg, digitsLost);
      transferDataState(srcReg);
      }
   // Transfer Sign/Data -- end

   void resetSignState()
      {
      _flags.set(SignStateKnown, false);
      _flags.set(SignStateAssumed, false);
      _flags.set(HasSignCode, false);
      _signCode = TR::DataType::getInvalidSignCode();
      resetTemporaryKnownSignCode();
      _flags.set(HasPreferredSign, false);
      _flags.set(HasCleanSign, false);
      _flags.set(HasKnownValidSign, false);
      _flags.set(HasKnownBadSignCode, false);
      _flags.set(SignStateInitialized, false);
      }


   virtual TR::Register              *getRegister();
   virtual TR_OpaquePseudoRegister  *getOpaquePseudoRegister();
   virtual TR_PseudoRegister        *getPseudoRegister();

   private:

   // The setHasCleanSign and setHasPreferredSign routines are private so the user is forced, based on the calling context,
   // to decide if either a known or assumed clean/preferred sign should be set.
   void setHasCleanSign()
      {
      resetTemporaryKnownSignCode();
      setSignStateInitialized();
      _flags.set(HasPreferredSign, true);  // clean implies preferred
      if (hasKnownOrAssumedSignCode() &&
          getKnownOrAssumedSignCode() != TR::DataType::getPreferredPlusSignCode(getDataType()) &&
          getKnownOrAssumedSignCode() != TR::DataType::getPreferredMinusSignCode(getDataType()))
         {
         TR_ASSERT(false,"should not set a clean sign on top of a non-preferred sign (0x%x)\n",getKnownOrAssumedSignCode());
         resetSignCode();        // a sign of 0xd is possibly inconsistent with a clean sign but it is up
         }                       // to the uset to reset the clean sign flag if a negative zero is possible
                                 // (as with cases there is no known sign)
      _flags.set(HasCleanSign, true);
      }

   void setHasPreferredSign()
      {
      resetTemporaryKnownSignCode();
      setSignStateInitialized();
      if (hasKnownOrAssumedSignCode() &&
          getKnownOrAssumedSignCode() != TR::DataType::getPreferredPlusSignCode(getDataType()) &&
          getKnownOrAssumedSignCode() != TR::DataType::getPreferredMinusSignCode(getDataType()))
         {
         TR_ASSERT(false,"should not set a preferred sign on top of a non-preferred sign (0x%x)\n",getKnownOrAssumedSignCode());
         resetSignCode();        // a sign of 0xd is possibly inconsistent with a clean/preferred sign but it is up
         }                       // to the uset to reset the clean sign flag if a negative zero is possible
                                 // (as with cases there is no known sign)
      _flags.set(HasPreferredSign, true);
      }

   void resetSignCode()
      {
      _flags.set(HasSignCode, false);
      _signCode = TR::DataType::getInvalidSignCode();
      }

   int32_t              _assumedSignCode;
   int32_t              _signCode;
   int32_t              _temporaryKnownSignCode;
   int32_t              _leftAlignedZeroDigits;
   int32_t              _decimalPrecision;      // _decimalPrecision is the valid number of digits in a binary coded decimal memory slot result.
                                                // The default value (set by node->setRegister()) is the corresponding node->getDecimalPrecision()
                                                // but can be set to another value.
   };

   
#endif
