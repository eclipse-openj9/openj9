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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 * Assisted-by: IBM Bob
 *******************************************************************************/

#ifndef SYMBOL_VALIDATION_MANAGER_INCL
#define SYMBOL_VALIDATION_MANAGER_INCL

#include <algorithm>
#include <map>
#include <set>
#include <vector>
#include <stddef.h>
#include <stdint.h>
#include "env/jittypes.h"
#include "j9.h"
#include "j9nonbuilder.h"
#include "infra/TRlist.hpp"
#include "env/TRMemory.hpp"
#include "env/VMJ9.h"
#include "exceptions/AOTFailure.hpp"
#include "ras/Logger.hpp"
#include "runtime/J9Runtime.hpp"

#if defined(J9VM_OPT_JITSERVER)
class ClientSessionData;
#endif /* defined(J9VM_OPT_JITSERVER) */
class AOTCacheClassChainRecord;
class AOTCacheWellKnownClassesRecord;

/**
 * @def SVM_ASSERT_LOCATION_INNER
 * @brief Helper macro for generating file:line location strings
 */
#define SVM_ASSERT_LOCATION_INNER(line) __FILE__ ":" #line

/**
 * @def SVM_ASSERT_LOCATION
 * @brief Macro to generate the current file and line location
 */
#define SVM_ASSERT_LOCATION(line) SVM_ASSERT_LOCATION_INNER(line)

/**
 * @def SVM_ASSERT_IMPL
 * @brief Internal implementation macro for SVM assertions
 * @param assertName Name of the assertion for error messages
 * @param nonfatal Whether the assertion is non-fatal (true) or fatal (false)
 * @param condition The condition to check
 * @param condStr String representation of the condition
 * @param format Printf-style format string for additional error information
 * @param ... Variable arguments for the format string
 */
#define SVM_ASSERT_IMPL(assertName, nonfatal, condition, condStr, format, ...)                   \
    do {                                                                                         \
        if (!(condition)) {                                                                      \
            if (!(nonfatal) && ::TR::SymbolValidationManager::assertionsAreFatal())              \
                ::TR::fatal_assertion(__FILE__, __LINE__, condStr, "" format "", ##__VA_ARGS__); \
            else {                                                                               \
                /*                                                                               \
                 * It is possible that this assert will fire before even the                     \
                 * default Logger is initialized. Check if it is available before                \
                 * logging.                                                                      \
                 */                                                                              \
                OMR::Logger *log = ::TR::comp()->log();                                          \
                if (log && log->isEnabled_DEPRECATED())                                          \
                    log->printf("" format "\n", ##__VA_ARGS__);                                  \
            }                                                                                    \
                                                                                                 \
            ::TR::comp()->failCompilation< ::J9::AOTSymbolValidationManagerFailure>(             \
                SVM_ASSERT_LOCATION(__LINE__) ": " assertName " failed: " condStr);              \
        }                                                                                        \
    } while (false)

/**
 * @def SVM_ASSERT
 * @brief Assertion for logic errors in Symbol Validation Manager
 *
 * This is a fatal assertion when TR_svmAssertionsAreFatal returns true;
 * otherwise it fails the current AOT compilation or load.
 *
 * @param condition The condition to check
 * @param format Printf-style format string for error message
 * @param ... Variable arguments for the format string
 */
#define SVM_ASSERT(condition, format, ...) \
    SVM_ASSERT_IMPL("SVM_ASSERT", false, condition, #condition, format, ##__VA_ARGS__)

/**
 * @def SVM_ASSERT_NONFATAL
 * @brief Non-fatal assertion for unhandled but non-error situations
 *
 * @param condition The condition to check
 * @param format Printf-style format string for error message
 * @param ... Variable arguments for the format string
 */
#define SVM_ASSERT_NONFATAL(condition, format, ...) \
    SVM_ASSERT_IMPL("SVM_ASSERT_NONFATAL", true, condition, #condition, format, ##__VA_ARGS__)

/**
 * @def SVM_ASSERT_ALREADY_VALIDATED
 * @brief Assert that a value has already been validated by the SVM
 * @param svm Pointer to the SymbolValidationManager instance
 * @param value The value to check for prior validation
 */
#define SVM_ASSERT_ALREADY_VALIDATED(svm, value)                                                            \
    do {                                                                                                    \
        void *_0value = (value);                                                                            \
        SVM_ASSERT_IMPL("SVM_ASSERT_ALREADY_VALIDATED", false, (svm)->isAlreadyValidated(_0value),          \
            "isAlreadyValidated(" #value ")", "%s %p should have already been validated", #value, _0value); \
    } while (false)

namespace TR {

/**
 * @struct SymbolValidationRecord
 * @brief Base class for all symbol validation records
 *
 * This is the abstract base class for all types of validation records used
 * during AOT compilation and loading. Each record represents a validation
 * that must be performed to ensure the compiled code is still valid.
 */
struct SymbolValidationRecord {
    /**
     * @brief Constructor
     * @param kind The type of relocation/validation this record represents
     */
    SymbolValidationRecord(TR_ExternalRelocationTargetKind kind)
        : _kind(kind)
    {}

    /**
     * @brief Check if this record is equal to another
     * @param other The record to compare with
     * @return true if records are equal, false otherwise
     */
    bool isEqual(SymbolValidationRecord *other) { return !isLessThan(other) && !other->isLessThan(this); }

    /**
     * @brief Compare this record with another for ordering
     * @param other The record to compare with
     * @return true if this record is less than other, false otherwise
     */
    bool isLessThan(SymbolValidationRecord *other)
    {
        if (_kind < other->_kind)
            return true;
        else if (_kind > other->_kind)
            return false;
        else
            return isLessThanWithinKind(other);
    }

    /**
     * @brief Print the fields of this record for debugging
     *
     * Pure virtual method that must be implemented by derived classes to print
     * their specific field values for logging and debugging purposes.
     */
    virtual void printFields() = 0;

    /**
     * @brief Check if this is a class validation record
     * @return true if this is a ClassValidationRecord, false otherwise
     */
    virtual bool isClassValidationRecord() { return false; }

    /** The kind of relocation/validation this record represents */
    TR_ExternalRelocationTargetKind _kind;

protected:
    /**
     * @brief Compare records of the same kind for ordering
     *
     * Pure virtual method for comparing two records of the same kind. This is
     * called by isLessThan() after determining that both records have the same
     * _kind value. Derived classes implement this to define ordering within
     * their specific record type.
     *
     * @param other The record to compare with (guaranteed to be same kind)
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other) = 0;

    /**
     * @brief Safely downcast a record to a specific type
     * @tparam T The target type
     * @param that Instance of target type (for type deduction)
     * @param record The record to downcast
     * @return The downcasted record
     */
    template<typename T> static T *downcast(T *that, SymbolValidationRecord *record)
    {
        TR_ASSERT(record->_kind == that->_kind, "unexpected SVM record comparison");
        return static_cast<T *>(record);
    }
};

/**
 * @struct LessSymbolValidationRecord
 * @brief Comparison functor for STL containers
 *
 * Provides a comparison operator for storing SymbolValidationRecord pointers
 * in STL containers like std::set.
 */
struct LessSymbolValidationRecord {
    /**
     * @brief Compare two validation records
     * @param a First record
     * @param b Second record
     * @return true if a < b, false otherwise
     */
    bool operator()(SymbolValidationRecord *a, SymbolValidationRecord *b) const { return a->isLessThan(b); }
};

/**
 * @struct ClassValidationRecord
 * @brief Base class for class-related validation records
 *
 * Specialized validation record for validations that involve classes.
 */
struct ClassValidationRecord : public SymbolValidationRecord {
    /**
     * @brief Constructor
     * @param kind The type of class validation
     */
    ClassValidationRecord(TR_ExternalRelocationTargetKind kind)
        : SymbolValidationRecord(kind)
    {}

    /**
     * @brief Check if this is a class validation record
     * @return true (always, since this is a ClassValidationRecord)
     */
    virtual bool isClassValidationRecord() { return true; }
};

/**
 * @struct ClassValidationRecordWithChain
 * @brief Class validation record that includes a class chain
 *
 * A class validation where the class chain provides data that is used to find
 * the class (e.g. its name). It can be advantageous to use a class chain even
 * when it is not required for the validation at hand, since each class needs a
 * class chain validation at some point. By including one in ClassByNameRecord
 * (for example), it's possible to eliminate the separate ClassChainRecord that
 * would otherwise be required.
 */
struct ClassValidationRecordWithChain : public ClassValidationRecord {
    /**
     * @brief Constructor
     * @param kind The type of class validation
     * @param clazz The class being validated
     */
    ClassValidationRecordWithChain(TR_ExternalRelocationTargetKind kind, TR_OpaqueClassBlock *clazz)
        : ClassValidationRecord(kind)
        , _class(clazz)
        , _classChainOffset(TR_SharedCache::INVALID_CLASS_CHAIN_OFFSET)
    {
#if defined(J9VM_OPT_JITSERVER)
        _aotCacheClassChainRecord = NULL;
#endif /* defined(J9VM_OPT_JITSERVER) */
    }

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

#if defined(J9VM_OPT_JITSERVER)
    /**
     * @brief Get the AOT cache class chain record (JITServer only)
     * @return Pointer to the AOT cache class chain record
     */
    const AOTCacheClassChainRecord *getAOTCacheClassChainRecord() const { return _aotCacheClassChainRecord; }
#else /* defined(J9VM_OPT_JITSERVER) */
    const AOTCacheClassChainRecord *getAOTCacheClassChainRecord() const { return NULL; }
#endif /* defined(J9VM_OPT_JITSERVER) */

    /** The class being validated */
    TR_OpaqueClassBlock *_class;
    /** Offset to the class chain in the shared cache */
    uintptr_t _classChainOffset;
#if defined(J9VM_OPT_JITSERVER)
    /** AOT cache class chain record (JITServer only) */
    const AOTCacheClassChainRecord *_aotCacheClassChainRecord;
#endif /* defined(J9VM_OPT_JITSERVER) */
};

/**
 * @struct ClassByNameRecord
 * @brief Validation record for a class looked up by name
 *
 * Validates that a class with a specific name can be found from a beholder
 * class.
 */
struct ClassByNameRecord : public ClassValidationRecordWithChain {
    /**
     * @brief Constructor
     * @param clazz The class being validated
     * @param beholder The class from whose perspective the lookup is performed
     */
    ClassByNameRecord(TR_OpaqueClassBlock *clazz, TR_OpaqueClassBlock *beholder)
        : ClassValidationRecordWithChain(TR_ValidateClassByName, clazz)
        , _beholder(beholder)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other ClassByNameRecord to compare with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The class from whose perspective the lookup is performed */
    TR_OpaqueClassBlock *_beholder;
};

/**
 * @struct ProfiledClassRecord
 * @brief Validation record for a profiled class
 *
 * Validates a class acquired via profiling.
 */
struct ProfiledClassRecord : public ClassValidationRecord {
    /**
     * @brief Constructor
     * @param clazz The profiled class
     * @param classChainOffset Offset to the class chain in the shared cache
     * @param aotCacheClassChainRecord AOT cache class chain record (JITServer
     * only)
     */
    ProfiledClassRecord(TR_OpaqueClassBlock *clazz, uintptr_t classChainOffset,
        const AOTCacheClassChainRecord *aotCacheClassChainRecord = NULL)
        : ClassValidationRecord(TR_ValidateProfiledClass)
        , _class(clazz)
        , _classChainOffset(classChainOffset)
    {
#if defined(J9VM_OPT_JITSERVER)
        _aotCacheClassChainRecord = aotCacheClassChainRecord;
#endif /* defined(J9VM_OPT_JITSERVER) */
    }

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other ProfiledClassRecord to compare with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

#if defined(J9VM_OPT_JITSERVER)
    const AOTCacheClassChainRecord *getAOTCacheClassChainRecord() const { return _aotCacheClassChainRecord; }
#else /* defined(J9VM_OPT_JITSERVER) */
    const AOTCacheClassChainRecord *getAOTCacheClassChainRecord() const { return NULL; }
#endif /* defined(J9VM_OPT_JITSERVER) */

    /** The profiled class */
    TR_OpaqueClassBlock *_class;
    /** Offset to the class chain in the shared cache */
    uintptr_t _classChainOffset;
#if defined(J9VM_OPT_JITSERVER)
    /** AOT cache class chain record (JITServer only) */
    const AOTCacheClassChainRecord *_aotCacheClassChainRecord;
#endif /* defined(J9VM_OPT_JITSERVER) */
};

/**
 * @struct ClassFromCPRecord
 * @brief Validation record for a class acquired from a constant pool entry
 */
struct ClassFromCPRecord : public ClassValidationRecord {
    /**
     * @brief Constructor
     * @param clazz The class to be validated
     * @param beholder The beholder class
     * @param cpIndex The constant pool index
     */
    ClassFromCPRecord(TR_OpaqueClassBlock *clazz, TR_OpaqueClassBlock *beholder, uint32_t cpIndex)
        : ClassValidationRecord(TR_ValidateClassFromCP)
        , _class(clazz)
        , _beholder(beholder)
        , _cpIndex(cpIndex)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other ClassFromCPRecord to compare with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The class to be validated */
    TR_OpaqueClassBlock *_class;
    /** The beholder class */
    TR_OpaqueClassBlock *_beholder;
    /** The constant pool index */
    uint32_t _cpIndex;
};

/**
 * @struct DefiningClassFromCPRecord
 * @brief Validation record for the defining class of a field or method from CP
 */
struct DefiningClassFromCPRecord : public ClassValidationRecord {
    /**
     * @brief Constructor
     * @param clazz The defining class
     * @param beholder The beholder class
     * @param cpIndex The constant pool index
     * @param isStatic Whether this is for a static field/method
     */
    DefiningClassFromCPRecord(TR_OpaqueClassBlock *clazz, TR_OpaqueClassBlock *beholder, uint32_t cpIndex,
        bool isStatic)
        : ClassValidationRecord(TR_ValidateDefiningClassFromCP)
        , _class(clazz)
        , _beholder(beholder)
        , _cpIndex(cpIndex)
        , _isStatic(isStatic)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other DefiningClassFromCPRecord to compare with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The defining class */
    TR_OpaqueClassBlock *_class;
    /** The beholder class */
    TR_OpaqueClassBlock *_beholder;
    /** The constant pool index */
    uint32_t _cpIndex;
    /** Whether this is for a static field/method */
    bool _isStatic;
};

/**
 * @struct StaticClassFromCPRecord
 * @brief Validation record for a static class resolved from a constant pool
 */
struct StaticClassFromCPRecord : public ClassValidationRecord {
    /**
     * @brief Constructor
     * @param clazz The static class
     * @param beholder The beholder class
     * @param cpIndex The constant pool index
     */
    StaticClassFromCPRecord(TR_OpaqueClassBlock *clazz, TR_OpaqueClassBlock *beholder, uint32_t cpIndex)
        : ClassValidationRecord(TR_ValidateStaticClassFromCP)
        , _class(clazz)
        , _beholder(beholder)
        , _cpIndex(cpIndex)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other StaticClassFromCPRecord to compare with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The static class */
    TR_OpaqueClassBlock *_class;
    /** The beholder class */
    TR_OpaqueClassBlock *_beholder;
    /** The constant pool index */
    uint32_t _cpIndex;
};

/**
 * @struct ArrayClassFromComponentClassRecord
 * @brief Validation record for an array class derived from its component class
 *
 * This is also used to validate a component class derived from its array
 * class.
 */
struct ArrayClassFromComponentClassRecord : public ClassValidationRecord {
    /**
     * @brief Constructor
     * @param arrayClass The array class
     * @param componentClass The component class of the array
     */
    ArrayClassFromComponentClassRecord(TR_OpaqueClassBlock *arrayClass, TR_OpaqueClassBlock *componentClass)
        : ClassValidationRecord(TR_ValidateArrayClassFromComponentClass)
        , _arrayClass(arrayClass)
        , _componentClass(componentClass)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other ArrayClassFromComponentClassRecord to compare with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The array class */
    TR_OpaqueClassBlock *_arrayClass;
    /** The component class of the array */
    TR_OpaqueClassBlock *_componentClass;
};

/**
 * @struct SuperClassFromClassRecord
 * @brief Validation record for a superclass relationship
 */
struct SuperClassFromClassRecord : public ClassValidationRecord {
    /**
     * @brief Constructor
     * @param superClass The superclass
     * @param childClass The child class
     */
    SuperClassFromClassRecord(TR_OpaqueClassBlock *superClass, TR_OpaqueClassBlock *childClass)
        : ClassValidationRecord(TR_ValidateSuperClassFromClass)
        , _superClass(superClass)
        , _childClass(childClass)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other SuperClassFromClassRecord to compare with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The superclass */
    TR_OpaqueClassBlock *_superClass;
    /** The child class */
    TR_OpaqueClassBlock *_childClass;
};

/**
 * @struct ClassInstanceOfClassRecord
 * @brief Validation record for instanceof relationship between classes
 */
struct ClassInstanceOfClassRecord : public SymbolValidationRecord {
    /**
     * @brief Constructor
     * @param classOne The first class (object type)
     * @param classTwo The second class (cast type)
     * @param objectTypeIsFixed Whether the object type is fixed
     * @param castTypeIsFixed Whether the cast type is fixed
     * @param isInstanceOf The result of the instanceof check
     */
    ClassInstanceOfClassRecord(TR_OpaqueClassBlock *classOne, TR_OpaqueClassBlock *classTwo, bool objectTypeIsFixed,
        bool castTypeIsFixed, bool isInstanceOf)
        : SymbolValidationRecord(TR_ValidateClassInstanceOfClass)
        , _classOne(classOne)
        , _classTwo(classTwo)
        , _objectTypeIsFixed(objectTypeIsFixed)
        , _castTypeIsFixed(castTypeIsFixed)
        , _isInstanceOf(isInstanceOf)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other ClassInstanceOfClassRecord to compare with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The first class (object type) */
    TR_OpaqueClassBlock *_classOne;
    /** The second class (cast type) */
    TR_OpaqueClassBlock *_classTwo;
    /** Whether the object type is fixed */
    bool _objectTypeIsFixed;
    /** Whether the cast type is fixed */
    bool _castTypeIsFixed;
    /** The expected result of the instanceof check */
    bool _isInstanceOf;
};

/**
 * @struct SystemClassByNameRecord
 * @brief Validation record for a system class looked up by name
 */
struct SystemClassByNameRecord : public ClassValidationRecordWithChain {
    /**
     * @brief Constructor
     * @param systemClass The system class
     */
    SystemClassByNameRecord(TR_OpaqueClassBlock *systemClass)
        : ClassValidationRecordWithChain(TR_ValidateSystemClassByName, systemClass)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other SystemClassByNameRecord to compare with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();
};

/**
 * @struct ClassFromITableIndexCPRecord
 * @brief Validation record for a class from an interface table index
 */
struct ClassFromITableIndexCPRecord : public ClassValidationRecord {
    /**
     * @brief Constructor
     * @param clazz The resolved class
     * @param beholder The beholder class
     * @param cpIndex The constant pool index
     */
    ClassFromITableIndexCPRecord(TR_OpaqueClassBlock *clazz, TR_OpaqueClassBlock *beholder, uint32_t cpIndex)
        : ClassValidationRecord(TR_ValidateClassFromITableIndexCP)
        , _class(clazz)
        , _beholder(beholder)
        , _cpIndex(cpIndex)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other ClassFromITableIndexCPRecord to compare with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The resolved class */
    TR_OpaqueClassBlock *_class;
    /** The beholder class */
    TR_OpaqueClassBlock *_beholder;
    /** The constant pool index */
    int32_t _cpIndex;
};

/**
 * @struct DeclaringClassFromFieldOrStaticRecord
 * @brief Validation record for the declaring class of a field or static member
 */
struct DeclaringClassFromFieldOrStaticRecord : public ClassValidationRecord {
    /**
     * @brief Constructor
     * @param clazz The declaring class
     * @param beholder The beholder class
     * @param cpIndex The constant pool index
     */
    DeclaringClassFromFieldOrStaticRecord(TR_OpaqueClassBlock *clazz, TR_OpaqueClassBlock *beholder, uint32_t cpIndex)
        : ClassValidationRecord(TR_ValidateDeclaringClassFromFieldOrStatic)
        , _class(clazz)
        , _beholder(beholder)
        , _cpIndex(cpIndex)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other DeclaringClassFromFieldOrStaticRecord to compare
     *              with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The declaring class */
    TR_OpaqueClassBlock *_class;
    /** The beholder class */
    TR_OpaqueClassBlock *_beholder;
    /** The constant pool index */
    uint32_t _cpIndex;
};

/**
 * @struct ConcreteSubClassFromClassRecord
 * @brief Validation record for a concrete subclass relationship
 */
struct ConcreteSubClassFromClassRecord : public ClassValidationRecord {
    /**
     * @brief Constructor
     * @param childClass The concrete subclass
     * @param superClass The superclass
     */
    ConcreteSubClassFromClassRecord(TR_OpaqueClassBlock *childClass, TR_OpaqueClassBlock *superClass)
        : ClassValidationRecord(TR_ValidateConcreteSubClassFromClass)
        , _childClass(childClass)
        , _superClass(superClass)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other ConcreteSubClassFromClassRecord to compare with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The concrete subclass */
    TR_OpaqueClassBlock *_childClass;
    /** The superclass */
    TR_OpaqueClassBlock *_superClass;
};

/**
 * @struct ClassChainRecord
 * @brief Validation record for a class chain
 *
 */
struct ClassChainRecord : public SymbolValidationRecord {
    /**
     * @brief Constructor
     * @param clazz The class to be validated
     * @param classChainOffset Offset to the class chain in the shared cache
     * @param aotCacheClassChainRecord AOT cache class chain record (JITServer
     *                                 only)
     */
    ClassChainRecord(TR_OpaqueClassBlock *clazz, uintptr_t classChainOffset,
        const AOTCacheClassChainRecord *aotCacheClassChainRecord = NULL)
        : SymbolValidationRecord(TR_ValidateClassChain)
        , _class(clazz)
        , _classChainOffset(classChainOffset)
    {
#if defined(J9VM_OPT_JITSERVER)
        _aotCacheClassChainRecord = aotCacheClassChainRecord;
#endif /* defined(J9VM_OPT_JITSERVER) */
    }

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other ClassChainRecord to compare with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

#if defined(J9VM_OPT_JITSERVER)
    const AOTCacheClassChainRecord *getAOTCacheClassChainRecord() const { return _aotCacheClassChainRecord; }
#else /* defined(J9VM_OPT_JITSERVER) */
    const AOTCacheClassChainRecord *getAOTCacheClassChainRecord() const { return NULL; }
#endif /* defined(J9VM_OPT_JITSERVER) */

    /** The class to be validated */
    TR_OpaqueClassBlock *_class;
    /** Offset to the class chain in the shared cache */
    uintptr_t _classChainOffset;
#if defined(J9VM_OPT_JITSERVER)
    /** AOT cache class chain record (JITServer only) */
    const AOTCacheClassChainRecord *_aotCacheClassChainRecord;
#endif /* defined(J9VM_OPT_JITSERVER) */
};

/**
 * @struct MethodValidationRecord
 * @brief Base class for method-related validation records
 */
struct MethodValidationRecord : public SymbolValidationRecord {
    /**
     * @brief Constructor
     * @param kind The type of method validation
     * @param method The method being validated
     */
    MethodValidationRecord(TR_ExternalRelocationTargetKind kind, TR_OpaqueMethodBlock *method)
        : SymbolValidationRecord(kind)
        , _method(method)
        , _definingClass(NULL)
    {}

    /**
     * @brief Get the cached defining class
     * @return The defining class (must be already cached)
     */
    TR_OpaqueClassBlock *definingClass()
    {
        TR_ASSERT(_definingClass, "defining class must be already cached");
        return _definingClass;
    }

    /**
     * @brief Get and cache the defining class
     * @param fe The frontend interface
     * @return The defining class
     */
    TR_OpaqueClassBlock *definingClass(TR_J9VM *fe)
    {
        _definingClass = fe->getClassOfMethod(_method);
        return _definingClass;
    }

    /** The method being validated */
    TR_OpaqueMethodBlock *_method;
    /** The class that defines this method (cached) */
    TR_OpaqueClassBlock *_definingClass;
};

/**
 * @struct MethodFromClassRecord
 * @brief Validation record for a method resolved from a class
 */
struct MethodFromClassRecord : public MethodValidationRecord {
    /**
     * @brief Constructor
     * @param method The method being validated
     * @param beholder The class from which the method is acquired
     * @param index The method index in the class
     */
    MethodFromClassRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *beholder, uint32_t index)
        : MethodValidationRecord(TR_ValidateMethodFromClass, method)
        , _beholder(beholder)
        , _index(index)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other MethodFromClassRecord to compare with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The class from which the method is accessed */
    TR_OpaqueClassBlock *_beholder;
    /** The method index in the class */
    uint32_t _index;
};

/**
 * @struct StaticMethodFromCPRecord
 * @brief Validation record for a static method from a constant pool entry
 */
struct StaticMethodFromCPRecord : public MethodValidationRecord {
    /**
     * @brief Constructor
     * @param method The static method
     * @param beholder The beholder class
     * @param cpIndex The constant pool index
     */
    StaticMethodFromCPRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *beholder, int32_t cpIndex)
        : MethodValidationRecord(TR_ValidateStaticMethodFromCP, method)
        , _beholder(beholder)
        , _cpIndex(cpIndex)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other StaticMethodFromCPRecord to compare with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The beholder class */
    TR_OpaqueClassBlock *_beholder;
    /** The constant pool index */
    int32_t _cpIndex;
};

/**
 * @struct SpecialMethodFromCPRecord
 * @brief Validation record for a special method (e.g., constructor) from CP
 */
struct SpecialMethodFromCPRecord : public MethodValidationRecord {
    /**
     * @brief Constructor
     * @param method The special method
     * @param beholder The beholder class
     * @param cpIndex The constant pool index
     */
    SpecialMethodFromCPRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *beholder, int32_t cpIndex)
        : MethodValidationRecord(TR_ValidateSpecialMethodFromCP, method)
        , _beholder(beholder)
        , _cpIndex(cpIndex)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other SpecialMethodFromCPRecord to compare with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The beholder class */
    TR_OpaqueClassBlock *_beholder;
    /** The constant pool index */
    int32_t _cpIndex;
};

/**
 * @struct VirtualMethodFromCPRecord
 * @brief Validation record for a virtual method from a constant pool
 */
struct VirtualMethodFromCPRecord : public MethodValidationRecord {
    /**
     * @brief Constructor
     * @param method The virtual method
     * @param beholder The beholder class
     * @param cpIndex The constant pool index
     */
    VirtualMethodFromCPRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *beholder, int32_t cpIndex)
        : MethodValidationRecord(TR_ValidateVirtualMethodFromCP, method)
        , _beholder(beholder)
        , _cpIndex(cpIndex)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other VirtualMethodFromCPRecord to compare with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The beholder class */
    TR_OpaqueClassBlock *_beholder;
    /** The constant pool index */
    int32_t _cpIndex;
};

/**
 * @struct VirtualMethodFromOffsetRecord
 * @brief Validation record for a virtual method resolved from a vtable offset
 */
struct VirtualMethodFromOffsetRecord : public MethodValidationRecord {
    /**
     * @brief Constructor
     * @param method The virtual method
     * @param beholder The class from which the method is accessed
     * @param virtualCallOffset The offset in the vtable
     * @param ignoreRtResolve Whether to ignore runtime resolution
     */
    VirtualMethodFromOffsetRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *beholder,
        int32_t virtualCallOffset, bool ignoreRtResolve)
        : MethodValidationRecord(TR_ValidateVirtualMethodFromOffset, method)
        , _beholder(beholder)
        , _virtualCallOffset(virtualCallOffset)
        , _ignoreRtResolve(ignoreRtResolve)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other VirtualMethodFromOffsetRecord to compare with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The class from which the method is accessed */
    TR_OpaqueClassBlock *_beholder;
    /** The offset in the vtable */
    int32_t _virtualCallOffset;
    /** Whether to ignore runtime resolution */
    bool _ignoreRtResolve;
};

/**
 * @struct InterfaceMethodFromCPRecord
 * @brief Validation record for an interface method resolved from a constant
 *        pool
 */
struct InterfaceMethodFromCPRecord : public MethodValidationRecord {
    /**
     * @brief Constructor
     * @param method The interface method
     * @param beholder The beholder class
     * @param lookup The class used for method lookup
     * @param cpIndex The constant pool index
     */
    InterfaceMethodFromCPRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *beholder,
        TR_OpaqueClassBlock *lookup, int32_t cpIndex)
        : MethodValidationRecord(TR_ValidateInterfaceMethodFromCP, method)
        , _beholder(beholder)
        , _lookup(lookup)
        , _cpIndex(cpIndex)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other InterfaceMethodFromCPRecord to compare with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The beholder class */
    TR_OpaqueClassBlock *_beholder;
    /** The class used for method lookup */
    TR_OpaqueClassBlock *_lookup;
    /** The constant pool index */
    int32_t _cpIndex;
};

/**
 * @struct MethodFromClassAndSigRecord
 * @brief Validation record for a method resolved from class and signature
 */
struct MethodFromClassAndSigRecord : public MethodValidationRecord {
    /**
     * @brief Constructor
     * @param method The method being validated
     * @param lookupClass The class in which to look up the method
     * @param beholder The beholder class
     */
    MethodFromClassAndSigRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *lookupClass,
        TR_OpaqueClassBlock *beholder)
        : MethodValidationRecord(TR_ValidateMethodFromClassAndSig, method)
        , _lookupClass(lookupClass)
        , _beholder(beholder)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other MethodFromClassAndSigRecord to compare with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The class in which to look up the method */
    TR_OpaqueClassBlock *_lookupClass;
    /** The beholder class */
    TR_OpaqueClassBlock *_beholder;
};

/**
 * @struct StackWalkerMaySkipFramesRecord
 * @brief Validation record for stack walker frame skipping behavior
 */
struct StackWalkerMaySkipFramesRecord : public SymbolValidationRecord {
    /**
     * @brief Constructor
     * @param method The method being checked
     * @param methodClass The class containing the method
     * @param skipFrames Whether the stack walker may skip frames for this
     *                   method
     */
    StackWalkerMaySkipFramesRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *methodClass,
        TR_YesNoMaybe skipFrames)
        : SymbolValidationRecord(TR_ValidateStackWalkerMaySkipFramesRecord)
        , _method(method)
        , _methodClass(methodClass)
        , _skipFrames(skipFrames)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other StackWalkerMaySkipFramesRecord to compare with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The method being checked */
    TR_OpaqueMethodBlock *_method;
    /** The class containing the method */
    TR_OpaqueClassBlock *_methodClass;
    /** Whether the stack walker may skip frames for this method */
    TR_YesNoMaybe _skipFrames;
};

/**
 * @struct ClassInfoIsInitialized
 * @brief Validation record for class initialization status
 */
struct ClassInfoIsInitialized : public SymbolValidationRecord {
    /**
     * @brief Constructor
     * @param clazz The class being validated
     * @param isInitialized Whether the class is initialized
     */
    ClassInfoIsInitialized(TR_OpaqueClassBlock *clazz, bool isInitialized)
        : SymbolValidationRecord(TR_ValidateClassInfoIsInitialized)
        , _class(clazz)
        , _isInitialized(isInitialized)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other ClassInfoIsInitialized to compare with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The class being validated */
    TR_OpaqueClassBlock *_class;
    /** Whether the class is initialized */
    bool _isInitialized;
};

/**
 * @struct MethodFromSingleImplementer
 * @brief Validation record for a method from a single implementer
 */
struct MethodFromSingleImplementer : public MethodValidationRecord {
    /**
     * @brief Constructor
     * @param method The method being validated
     * @param thisClass The class with a single implementer
     * @param cpIndexOrVftSlot CP index or VFT slot for the method
     * @param callerMethod The calling method
     * @param useGetResolvedInterfaceMethod Whether to use getResolvedInterfaceMethod
     */
    MethodFromSingleImplementer(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *thisClass, int32_t cpIndexOrVftSlot,
        TR_OpaqueMethodBlock *callerMethod, TR_YesNoMaybe useGetResolvedInterfaceMethod)
        : MethodValidationRecord(TR_ValidateMethodFromSingleImplementer, method)
        , _thisClass(thisClass)
        , _cpIndexOrVftSlot(cpIndexOrVftSlot)
        , _callerMethod(callerMethod)
        , _useGetResolvedInterfaceMethod(useGetResolvedInterfaceMethod)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other MethodFromSingleImplementer to compare with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The class with a single implementer */
    TR_OpaqueClassBlock *_thisClass;
    /** CP index or VFT slot for the method */
    int32_t _cpIndexOrVftSlot;
    /** The calling method */
    TR_OpaqueMethodBlock *_callerMethod;
    /** Whether to use getResolvedInterfaceMethod */
    TR_YesNoMaybe _useGetResolvedInterfaceMethod;
};

/**
 * @struct MethodFromSingleInterfaceImplementer
 * @brief Validation record for a method from a single interface implementer
 */
struct MethodFromSingleInterfaceImplementer : public MethodValidationRecord {
    /**
     * @brief Constructor
     * @param method The method being validated
     * @param thisClass The interface class with a single implementer
     * @param cpIndex The constant pool index
     * @param callerMethod The calling method
     */
    MethodFromSingleInterfaceImplementer(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *thisClass, int32_t cpIndex,
        TR_OpaqueMethodBlock *callerMethod)
        : MethodValidationRecord(TR_ValidateMethodFromSingleInterfaceImplementer, method)
        , _thisClass(thisClass)
        , _cpIndex(cpIndex)
        , _callerMethod(callerMethod)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other MethodFromSingleInterfaceImplementer to compare
     *              with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The interface class with a single implementer */
    TR_OpaqueClassBlock *_thisClass;
    /** The constant pool index */
    int32_t _cpIndex;
    /** The calling method */
    TR_OpaqueMethodBlock *_callerMethod;
};

/**
 * @struct MethodFromSingleAbstractImplementer
 * @brief Validation record for a method from a single abstract implementer
 */
struct MethodFromSingleAbstractImplementer : public MethodValidationRecord {
    /**
     * @brief Constructor
     * @param method The method being validated
     * @param thisClass The abstract class with a single implementer
     * @param vftSlot The virtual function table slot
     * @param callerMethod The calling method
     */
    MethodFromSingleAbstractImplementer(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *thisClass, int32_t vftSlot,
        TR_OpaqueMethodBlock *callerMethod)
        : MethodValidationRecord(TR_ValidateMethodFromSingleAbstractImplementer, method)
        , _thisClass(thisClass)
        , _vftSlot(vftSlot)
        , _callerMethod(callerMethod)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other MethodFromSingleAbstractImplementer to compare
     *              with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The abstract class with a single implementer */
    TR_OpaqueClassBlock *_thisClass;
    /** The virtual function table slot */
    int32_t _vftSlot;
    /** The calling method */
    TR_OpaqueMethodBlock *_callerMethod;
};

/**
 * @struct ImproperInterfaceMethodFromCPRecord
 * @brief Validation record for an improper interface method from CP
 */
struct ImproperInterfaceMethodFromCPRecord : public MethodValidationRecord {
    /**
     * @brief Constructor
     * @param method The interface method
     * @param beholder The beholder class
     * @param cpIndex The constant pool index
     */
    ImproperInterfaceMethodFromCPRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *beholder, int32_t cpIndex)
        : MethodValidationRecord(TR_ValidateImproperInterfaceMethodFromCP, method)
        , _beholder(beholder)
        , _cpIndex(cpIndex)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other ImproperInterfaceMethodFromCPRecord to compare
     *        with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The beholder class */
    TR_OpaqueClassBlock *_beholder;
    /** The constant pool index */
    int32_t _cpIndex;
};

/**
 * @struct J2IThunkFromMethodRecord
 * @brief Validation record for a J2I thunk
 */
struct J2IThunkFromMethodRecord : public SymbolValidationRecord {
    /**
     * @brief Constructor
     * @param thunk The J2I thunk pointer
     * @param method The method associated with the thunk
     */
    J2IThunkFromMethodRecord(void *thunk, TR_OpaqueMethodBlock *method)
        : SymbolValidationRecord(TR_ValidateJ2IThunkFromMethod)
        , _thunk(thunk)
        , _method(method)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other J2IThunkFromMethodRecord to compare with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The J2I thunk pointer */
    void *_thunk;
    /** The method associated with the thunk */
    TR_OpaqueMethodBlock *_method;
};

/**
 * @struct IsClassVisibleRecord
 * @brief Validation record for class visibility
 */
struct IsClassVisibleRecord : public SymbolValidationRecord {
    /**
     * @brief Constructor
     * @param sourceClass The source class checking visibility
     * @param destClass The destination class being checked
     * @param isVisible Whether destClass is visible from sourceClass
     */
    IsClassVisibleRecord(TR_OpaqueClassBlock *sourceClass, TR_OpaqueClassBlock *destClass, bool isVisible)
        : SymbolValidationRecord(TR_ValidateIsClassVisible)
        , _sourceClass(sourceClass)
        , _destClass(destClass)
        , _isVisible(isVisible)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other IsClassVisibleRecord to compare with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The source class checking visibility */
    TR_OpaqueClassBlock *_sourceClass;
    /** The destination class being checked */
    TR_OpaqueClassBlock *_destClass;
    /** Whether destClass is visible from sourceClass */
    bool _isVisible;
};

/**
 * @struct DynamicMethodFromCallsiteIndexRecord
 * @brief Validation record for a dynamic method from a callsite index
 */
struct DynamicMethodFromCallsiteIndexRecord : public MethodValidationRecord {
    /**
     * @brief Constructor
     * @param method The dynamic method
     * @param caller The calling method
     * @param callsiteIndex The callsite index
     * @param appendixObjectNull Whether the appendix object is null
     */
    DynamicMethodFromCallsiteIndexRecord(TR_OpaqueMethodBlock *method, TR_OpaqueMethodBlock *caller,
        int32_t callsiteIndex, bool appendixObjectNull)
        : MethodValidationRecord(TR_ValidateDynamicMethodFromCallsiteIndex, method)
        , _caller(caller)
        , _callsiteIndex(callsiteIndex)
        , _appendixObjectNull(appendixObjectNull)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other DynamicMethodFromCallsiteIndexRecord to compare
     *              with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The calling method */
    TR_OpaqueMethodBlock *_caller;
    /** The callsite index */
    int32_t _callsiteIndex;
    /** Whether the appendix object is null */
    bool _appendixObjectNull;
};

/**
 * @struct HandleMethodFromCPIndex
 * @brief Validation record for a method handle from a constant pool index
 */
struct HandleMethodFromCPIndex : public MethodValidationRecord {
    /**
     * @brief Constructor
     * @param method The method handle
     * @param caller The calling method
     * @param cpIndex The constant pool index
     * @param appendixObjectNull Whether the appendix object is null
     */
    HandleMethodFromCPIndex(TR_OpaqueMethodBlock *method, TR_OpaqueMethodBlock *caller, int32_t cpIndex,
        bool appendixObjectNull)
        : MethodValidationRecord(TR_ValidateHandleMethodFromCPIndex, method)
        , _caller(caller)
        , _cpIndex(cpIndex)
        , _appendixObjectNull(appendixObjectNull)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other HandleMethodFromCPIndex to compare with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The calling method */
    TR_OpaqueMethodBlock *_caller;
    /** The constant pool index */
    int32_t _cpIndex;
    /** Whether the appendix object is null */
    bool _appendixObjectNull;
};

/**
 * @struct MethodsFromClass
 * @brief Validation record to generate IDs for methods in a class; this
 *        prevents the need for explicit MethodFromClass validation records
 *        when calling the getResolvedMethods frontend query
 */
struct MethodsFromClass : public SymbolValidationRecord {
    /**
     * @brief Constructor
     * @param clazz The class whose methods need IDs associated with them
     * @param startingSymbolID the initial ID that will be used as a starting
     *                         point to associate IDs to methods
     */
    MethodsFromClass(TR_OpaqueClassBlock *clazz, uint16_t startingSymbolID)
        : SymbolValidationRecord(TR_ValidateMethodsFromClass)
        , _startingSymbolID(startingSymbolID)
        , _clazz(clazz)
    {}

    /**
     * @brief Compare this record with another of the same kind
     * @param other The other MethodsFromClass to compare with
     * @return true if this record is less than other, false otherwise
     */
    virtual bool isLessThanWithinKind(SymbolValidationRecord *other);

    /**
     * @brief Print the fields of this record for debugging
     */
    virtual void printFields();

    /** The starting symbol ID */
    uint16_t _startingSymbolID;
    /** The class whose methods will be associated with IDs */
    TR_OpaqueClassBlock *_clazz;
};

/**
 * @class SymbolValidationManager
 * @brief Manages symbol validation for AOT compilation and loading
 *
 * The SymbolValidationManager tracks values (classes, methods, etc.) used
 * during an AOT compilation and generates validation records. It creates IDs
 * (symbols) which are JVM agnostic. During an AOT load, it uses the symbols to
 * materialize values and validates them.
 */
class SymbolValidationManager {
public:
    TR_ALLOC(TR_MemoryBase::SymbolValidationManager)

    /**
     * @brief Constructor
     * @param region TR::Region for allocations
     * @param compilee The method being compiled
     * @param comp The compilation object
     */
    SymbolValidationManager(TR::Region &region, TR_ResolvedMethod *compilee, TR::Compilation *comp);

    /**
     * @struct SystemClassNotWorthRemembering
     * @brief Information about system classes that should not be remembered
     */
    struct SystemClassNotWorthRemembering {
        const char *_className; /**< Name of the class */
        TR_OpaqueClassBlock *_clazz; /**< The class pointer */
        bool _checkIsSuperClass; /**< Whether to check if it's a superclass */
    };

/** Number of well-known classes tracked by the SVM */
#define WELL_KNOWN_CLASS_COUNT 9

    /**
     * @brief Populate the list of well-known classes
     */
    void populateWellKnownClasses();

    /**
     * @brief Validate well-known classes
     * @param wellKnownClassChainOffsets Array of class chain offsets
     * @return true if validation succeeds, false otherwise
     */
    bool validateWellKnownClasses(const uintptr_t *wellKnownClassChainOffsets);

    /**
     * @brief Check if a class is a well-known class
     * @param clazz The class to check
     * @return true if the class is well-known, false otherwise
     */
    bool isWellKnownClass(TR_OpaqueClassBlock *clazz);

    /**
     * @brief Check if a class can see all well-known classes
     * @param clazz The class to check
     * @return true if the class can see all well-known classes, false otherwise
     */
    bool classCanSeeWellKnownClasses(TR_OpaqueClassBlock *clazz);

    /**
     * @brief Get the well-known class chain offsets
     * @return Pointer to the well-known class chain offsets
     */
    const void *wellKnownClassChainOffsets() const { return _wellKnownClassChainOffsets; }

#if defined(J9VM_OPT_JITSERVER)
    /**
     * @brief Get the AOT cache well-known classes record (JITServer only)
     * @return Pointer to the AOT cache well-known classes record
     */
    const AOTCacheWellKnownClassesRecord *aotCacheWellKnownClassesRecord() const
    {
        return _aotCacheWellKnownClassesRecord;
    }
#endif /* defined(J9VM_OPT_JITSERVER */

    /**
     * @enum Presence
     * @brief Indicates whether a symbol is required or optional
     */
    enum Presence {
        SymRequired, /**< Symbol must be present */
        SymOptional /**< Symbol is optional */
    };

    /**
     * @brief Check whether a value has been seen already
     * @return true if the value has been seen, false otherwise
     */
    bool valueHasBeenSeen(void *value);

    /**
     * @brief Get a value from a symbol ID
     * @param id The symbol ID
     * @param type The type of symbol
     * @param presence Whether the symbol is required or optional
     * @return Pointer to the value, or NULL if not found and optional
     */
    void *getValueFromSymbolID(uint16_t id, TR::SymbolType type, Presence presence = SymRequired);

    /**
     * @brief Get a class from a symbol ID
     * @param id The symbol ID
     * @param presence Whether the symbol is required or optional
     * @return The class, or NULL if not found and optional
     */
    TR_OpaqueClassBlock *getClassFromID(uint16_t id, Presence presence = SymRequired);

    /**
     * @brief Get a J9Class from a symbol ID
     * @param id The symbol ID
     * @param presence Whether the symbol is required or optional
     * @return The J9Class, or NULL if not found and optional
     */
    J9Class *getJ9ClassFromID(uint16_t id, Presence presence = SymRequired);

    /**
     * @brief Get a method from a symbol ID
     * @param id The symbol ID
     * @param presence Whether the symbol is required or optional
     * @return The method, or NULL if not found and optional
     */
    TR_OpaqueMethodBlock *getMethodFromID(uint16_t id, Presence presence = SymRequired);

    /**
     * @brief Get a J9Method from a symbol ID
     * @param id The symbol ID
     * @param presence Whether the symbol is required or optional
     * @return The J9Method, or NULL if not found and optional
     */
    J9Method *getJ9MethodFromID(uint16_t id, Presence presence = SymRequired);

    /**
     * @brief Try to get a symbol ID from a value (returns NO_ID if not found)
     * @param value The value to look up
     * @return The symbol ID, or NO_ID if not found
     */
    uint16_t tryGetSymbolIDFromValue(void *value);

    /**
     * @brief Get a symbol ID from a value (asserts if not found)
     * @param value The value to look up
     * @return The symbol ID
     */
    uint16_t getSymbolIDFromValue(void *value);

    /**
     * @brief Check if a value has already been validated
     * @param value The value to check
     * @return true if already validated, false otherwise
     */
    bool isAlreadyValidated(void *value) { return inHeuristicRegion() || tryGetSymbolIDFromValue(value) != NO_ID; }

    // Methods to add validation records during AOT compilation

    /**
     * @brief Add a class-by-name validation record
     * @param clazz The class to validate
     * @param beholder The class from whose perspective the lookup is performed
     * @return true if record was added, false otherwise
     */
    bool addClassByNameRecord(TR_OpaqueClassBlock *clazz, TR_OpaqueClassBlock *beholder);

    /**
     * @brief Add a profiled class validation record
     * @param clazz The profiled class
     * @return true if record was added, false otherwise
     */
    bool addProfiledClassRecord(TR_OpaqueClassBlock *clazz);

    /**
     * @brief Add a class-from-CP validation record
     * @param clazz The class from the constant pool
     * @param constantPoolOfBeholder The constant pool
     * @param cpIndex The constant pool index
     * @return true if record was added, false otherwise
     */
    bool addClassFromCPRecord(TR_OpaqueClassBlock *clazz, J9ConstantPool *constantPoolOfBeholder, uint32_t cpIndex);

    /**
     * @brief Add a defining-class-from-CP validation record
     * @param clazz The defining class
     * @param constantPoolOfBeholder The constant pool
     * @param cpIndex The constant pool index
     * @param isStatic Whether this is for a static member
     * @return true if record was added, false otherwise
     */
    bool addDefiningClassFromCPRecord(TR_OpaqueClassBlock *clazz, J9ConstantPool *constantPoolOfBeholder,
        uint32_t cpIndex, bool isStatic = false);

    /**
     * @brief Add a static-class-from-CP validation record
     * @param clazz The static class
     * @param constantPoolOfBeholder The constant pool
     * @param cpIndex The constant pool index
     * @return true if record was added, false otherwise
     */
    bool addStaticClassFromCPRecord(TR_OpaqueClassBlock *clazz, J9ConstantPool *constantPoolOfBeholder,
        uint32_t cpIndex);

    /**
     * @brief Add an array-class-from-component-class validation record
     * @param arrayClass The array class
     * @param componentClass The component class
     * @return true if record was added, false otherwise
     */
    bool addArrayClassFromComponentClassRecord(TR_OpaqueClassBlock *arrayClass, TR_OpaqueClassBlock *componentClass);

    /**
     * @brief Add a superclass-from-class validation record
     * @param superClass The superclass
     * @param childClass The child class
     * @return true if record was added, false otherwise
     */
    bool addSuperClassFromClassRecord(TR_OpaqueClassBlock *superClass, TR_OpaqueClassBlock *childClass);

    /**
     * @brief Add a class-instanceof-class validation record
     * @param classOne The first class (object type)
     * @param classTwo The second class (cast type)
     * @param objectTypeIsFixed Whether the object type is fixed
     * @param castTypeIsFixed Whether the cast type is fixed
     * @param isInstanceOf The expected instanceof result
     * @return true if record was added, false otherwise
     */
    bool addClassInstanceOfClassRecord(TR_OpaqueClassBlock *classOne, TR_OpaqueClassBlock *classTwo,
        bool objectTypeIsFixed, bool castTypeIsFixed, bool isInstanceOf);

    /**
     * @brief Add a system-class-by-name validation record
     * @param systemClass The system class
     * @return true if record was added, false otherwise
     */
    bool addSystemClassByNameRecord(TR_OpaqueClassBlock *systemClass);

    /**
     * @brief Add a class-from-itable-index-CP validation record
     * @param clazz The class
     * @param constantPoolOfBeholder The constant pool
     * @param cpIndex The constant pool index
     * @return true if record was added, false otherwise
     */
    bool addClassFromITableIndexCPRecord(TR_OpaqueClassBlock *clazz, J9ConstantPool *constantPoolOfBeholder,
        int32_t cpIndex);

    /**
     * @brief Add a declaring-class-from-field-or-static validation record
     * @param clazz The declaring class
     * @param constantPoolOfBeholder The constant pool
     * @param cpIndex The constant pool index
     * @return true if record was added, false otherwise
     */
    bool addDeclaringClassFromFieldOrStaticRecord(TR_OpaqueClassBlock *clazz, J9ConstantPool *constantPoolOfBeholder,
        int32_t cpIndex);

    /**
     * @brief Add a concrete-subclass-from-class validation record
     * @param childClass The concrete subclass
     * @param superClass The superclass
     * @return true if record was added, false otherwise
     */
    bool addConcreteSubClassFromClassRecord(TR_OpaqueClassBlock *childClass, TR_OpaqueClassBlock *superClass);

    /**
     * @brief Add a method-from-class validation record
     * @param method The method
     * @param beholder The class from which the method is accessed
     * @param index The method index
     * @return true if record was added, false otherwise
     */
    bool addMethodFromClassRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *beholder, uint32_t index);

    /**
     * @brief Add a static-method-from-CP validation record
     * @param method The static method
     * @param cp The constant pool
     * @param cpIndex The constant pool index
     * @return true if record was added, false otherwise
     */
    bool addStaticMethodFromCPRecord(TR_OpaqueMethodBlock *method, J9ConstantPool *cp, int32_t cpIndex);

    /**
     * @brief Add a special-method-from-CP validation record
     * @param method The special method
     * @param cp The constant pool
     * @param cpIndex The constant pool index
     * @return true if record was added, false otherwise
     */
    bool addSpecialMethodFromCPRecord(TR_OpaqueMethodBlock *method, J9ConstantPool *cp, int32_t cpIndex);

    /**
     * @brief Add a virtual-method-from-CP validation record
     * @param method The virtual method
     * @param cp The constant pool
     * @param cpIndex The constant pool index
     * @return true if record was added, false otherwise
     */
    bool addVirtualMethodFromCPRecord(TR_OpaqueMethodBlock *method, J9ConstantPool *cp, int32_t cpIndex);

    /**
     * @brief Add a virtual-method-from-offset validation record
     * @param method The virtual method
     * @param beholder The class from which the method is accessed
     * @param virtualCallOffset The vtable offset
     * @param ignoreRtResolve Whether to ignore runtime resolution
     * @return true if record was added, false otherwise
     */
    bool addVirtualMethodFromOffsetRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *beholder,
        int32_t virtualCallOffset, bool ignoreRtResolve);

    /**
     * @brief Add an interface-method-from-CP validation record
     * @param method The interface method
     * @param beholder The beholder class
     * @param lookup The class used for method lookup
     * @param cpIndex The constant pool index
     * @return true if record was added, false otherwise
     */
    bool addInterfaceMethodFromCPRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *beholder,
        TR_OpaqueClassBlock *lookup, int32_t cpIndex);

    /**
     * @brief Add an improper-interface-method-from-CP validation record
     * @param method The interface method
     * @param cp The constant pool
     * @param cpIndex The constant pool index
     * @return true if record was added, false otherwise
     */
    bool addImproperInterfaceMethodFromCPRecord(TR_OpaqueMethodBlock *method, J9ConstantPool *cp, int32_t cpIndex);

    /**
     * @brief Add a method-from-class-and-signature validation record
     * @param method The method
     * @param methodClass The class defining the method
     * @param beholder The class from which the method is accessed
     * @return true if record was added, false otherwise
     */
    bool addMethodFromClassAndSignatureRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *methodClass,
        TR_OpaqueClassBlock *beholder);

    /**
     * @brief Add a method-from-single-implementer validation record
     * @param method The method
     * @param thisClass The class with a single implementer
     * @param cpIndexOrVftSlot CP index or VFT slot
     * @param callerMethod The calling method
     * @param useGetResolvedInterfaceMethod Whether to use getResolvedInterfaceMethod
     * @return true if record was added, false otherwise
     */
    bool addMethodFromSingleImplementerRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *thisClass,
        int32_t cpIndexOrVftSlot, TR_OpaqueMethodBlock *callerMethod, TR_YesNoMaybe useGetResolvedInterfaceMethod);

    /**
     * @brief Add a method-from-single-interface-implementer validation record
     * @param method The method
     * @param thisClass The interface class with a single implementer
     * @param cpIndex The constant pool index
     * @param callerMethod The calling method
     * @return true if record was added, false otherwise
     */
    bool addMethodFromSingleInterfaceImplementerRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *thisClass,
        int32_t cpIndex, TR_OpaqueMethodBlock *callerMethod);

    /**
     * @brief Add a method-from-single-abstract-implementer validation record
     * @param method The method
     * @param thisClass The abstract class with a single implementer
     * @param vftSlot The virtual function table slot
     * @param callerMethod The calling method
     * @return true if record was added, false otherwise
     */
    bool addMethodFromSingleAbstractImplementerRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *thisClass,
        int32_t vftSlot, TR_OpaqueMethodBlock *callerMethod);

    /**
     * @brief Add a dynamic-method-from-callsite-index validation record
     * @param method The dynamic method
     * @param caller The calling method
     * @param callsiteIndex The callsite index
     * @param appendixObjectNull Whether the appendix object is null
     * @return true if record was added, false otherwise
     */
    bool addDynamicMethodFromCallsiteIndex(TR_OpaqueMethodBlock *method, TR_OpaqueMethodBlock *caller,
        int32_t callsiteIndex, bool appendixObjectNull);

    /**
     * @brief Add a handle-method-from-CP-index validation record
     * @param method The method handle
     * @param caller The calling method
     * @param cpIndex The constant pool index
     * @param appendixObjectNull Whether the appendix object is null
     * @return true if record was added, false otherwise
     */
    bool addHandleMethodFromCPIndex(TR_OpaqueMethodBlock *method, TR_OpaqueMethodBlock *caller, int32_t cpIndex,
        bool appendixObjectNull);

    /**
     * @brief Add a stack-walker-may-skip-frames validation record
     * @param method The method being checked
     * @param methodClass The class containing the method
     * @param skipFrames Whether the stack walker may skip frames
     * @return true if record was added, false otherwise
     */
    bool addStackWalkerMaySkipFramesRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *methodClass,
        TR_YesNoMaybe skipFrames);

    /**
     * @brief Add a class-info-is-initialized validation record
     * @param clazz The class being checked
     * @param isInitialized Whether the class is initialized
     * @return true if record was added, false otherwise
     */
    bool addClassInfoIsInitializedRecord(TR_OpaqueClassBlock *clazz, bool isInitialized);

    /**
     * @brief Add a J2I-thunk-from-method validation record
     * @param thunk The J2I thunk pointer
     * @param method The method associated with the thunk
     */
    void addJ2IThunkFromMethodRecord(void *thunk, TR_OpaqueMethodBlock *method);

    /**
     * @brief Add an is-class-visible validation record
     * @param sourceClass The source class checking visibility
     * @param destClass The destination class being checked
     * @param isVisible Whether destClass is visible from sourceClass
     * @return true if record was added, false otherwise
     */
    bool addIsClassVisibleRecord(TR_OpaqueClassBlock *sourceClass, TR_OpaqueClassBlock *destClass, bool isVisible);

    /**
     * @brief Add a methods-from-class validation record
     * @param clazz The class whose methods need IDs associated with them
     */
    void addMethodsFromClassRecord(TR_OpaqueClassBlock *clazz);

    // Methods to validate records during AOT load

    /**
     * @brief Validate a class-by-name record
     * @param classID The symbol ID for the class
     * @param beholderID The symbol ID for the beholder class
     * @param classChain The class chain to validate against
     * @return true if validation succeeds, false otherwise
     */
    bool validateClassByNameRecord(uint16_t classID, uint16_t beholderID, uintptr_t *classChain);

    /**
     * @brief Validate a profiled class record
     * @param classID The symbol ID for the class
     * @param classChainIdentifyingLoader Class chain identifying the loader
     * @param classChainForClassBeingValidated Class chain for the class being validated
     * @return true if validation succeeds, false otherwise
     */
    bool validateProfiledClassRecord(uint16_t classID, void *classChainIdentifyingLoader,
        void *classChainForClassBeingValidated);

    /**
     * @brief Validate a class-from-CP record
     * @param classID The symbol ID for the class
     * @param beholderID The symbol ID for the beholder class
     * @param cpIndex The constant pool index
     * @return true if validation succeeds, false otherwise
     */
    bool validateClassFromCPRecord(uint16_t classID, uint16_t beholderID, uint32_t cpIndex);

    /**
     * @brief Validate a defining-class-from-CP record
     * @param classID The symbol ID for the defining class
     * @param beholderID The symbol ID for the beholder class
     * @param cpIndex The constant pool index
     * @param isStatic Whether this is for a static member
     * @return true if validation succeeds, false otherwise
     */
    bool validateDefiningClassFromCPRecord(uint16_t classID, uint16_t beholderID, uint32_t cpIndex, bool isStatic);

    /**
     * @brief Validate a static-class-from-CP record
     * @param classID The symbol ID for the static class
     * @param beholderID The symbol ID for the beholder class
     * @param cpIndex The constant pool index
     * @return true if validation succeeds, false otherwise
     */
    bool validateStaticClassFromCPRecord(uint16_t classID, uint16_t beholderID, uint32_t cpIndex);

    /**
     * @brief Validate an array-class-from-component-class record
     * @param arrayClassID The symbol ID for the array class
     * @param componentClassID The symbol ID for the component class
     * @return true if validation succeeds, false otherwise
     */
    bool validateArrayClassFromComponentClassRecord(uint16_t arrayClassID, uint16_t componentClassID);

    /**
     * @brief Validate a superclass-from-class record
     * @param superClassID The symbol ID for the superclass
     * @param childClassID The symbol ID for the child class
     * @return true if validation succeeds, false otherwise
     */
    bool validateSuperClassFromClassRecord(uint16_t superClassID, uint16_t childClassID);

    /**
     * @brief Validate a class-instanceof-class record
     * @param classOneID The symbol ID for the first class
     * @param classTwoID The symbol ID for the second class
     * @param objectTypeIsFixed Whether the object type is fixed
     * @param castTypeIsFixed Whether the cast type is fixed
     * @param wasInstanceOf The expected instanceof result
     * @return true if validation succeeds, false otherwise
     */
    bool validateClassInstanceOfClassRecord(uint16_t classOneID, uint16_t classTwoID, bool objectTypeIsFixed,
        bool castTypeIsFixed, bool wasInstanceOf);

    /**
     * @brief Validate a system-class-by-name record
     * @param systemClassID The symbol ID for the system class
     * @param classChain The class chain to validate against
     * @return true if validation succeeds, false otherwise
     */
    bool validateSystemClassByNameRecord(uint16_t systemClassID, uintptr_t *classChain);

    /**
     * @brief Validate a class-from-itable-index-CP record
     * @param classID The symbol ID for the class
     * @param beholderID The symbol ID for the beholder class
     * @param cpIndex The constant pool index
     * @return true if validation succeeds, false otherwise
     */
    bool validateClassFromITableIndexCPRecord(uint16_t classID, uint16_t beholderID, uint32_t cpIndex);

    /**
     * @brief Validate a declaring-class-from-field-or-static record
     * @param definingClassID The symbol ID for the defining class
     * @param beholderID The symbol ID for the beholder class
     * @param cpIndex The constant pool index
     * @return true if validation succeeds, false otherwise
     */
    bool validateDeclaringClassFromFieldOrStaticRecord(uint16_t definingClassID, uint16_t beholderID, int32_t cpIndex);

    /**
     * @brief Validate a concrete-subclass-from-class record
     * @param childClassID The symbol ID for the child class
     * @param superClassID The symbol ID for the superclass
     * @return true if validation succeeds, false otherwise
     */
    bool validateConcreteSubClassFromClassRecord(uint16_t childClassID, uint16_t superClassID);

    /**
     * @brief Validate a class chain record
     * @param classID The symbol ID for the class
     * @param classChain The class chain to validate
     * @return true if validation succeeds, false otherwise
     */
    bool validateClassChainRecord(uint16_t classID, void *classChain);

    /**
     * @brief Validate a method-from-class record
     * @param methodID The symbol ID for the method
     * @param beholderID The symbol ID for the beholder class
     * @param index The method index
     * @return true if validation succeeds, false otherwise
     */
    bool validateMethodFromClassRecord(uint16_t methodID, uint16_t beholderID, uint32_t index);

    /**
     * @brief Validate a static-method-from-CP record
     * @param methodID The symbol ID for the method
     * @param definingClassID The symbol ID for the defining class
     * @param beholderID The symbol ID for the beholder class
     * @param cpIndex The constant pool index
     * @return true if validation succeeds, false otherwise
     */
    bool validateStaticMethodFromCPRecord(uint16_t methodID, uint16_t definingClassID, uint16_t beholderID,
        int32_t cpIndex);

    /**
     * @brief Validate a special-method-from-CP record
     * @param methodID The symbol ID for the method
     * @param definingClassID The symbol ID for the defining class
     * @param beholderID The symbol ID for the beholder class
     * @param cpIndex The constant pool index
     * @return true if validation succeeds, false otherwise
     */
    bool validateSpecialMethodFromCPRecord(uint16_t methodID, uint16_t definingClassID, uint16_t beholderID,
        int32_t cpIndex);

    /**
     * @brief Validate a virtual-method-from-CP record
     * @param methodID The symbol ID for the method
     * @param definingClassID The symbol ID for the defining class
     * @param beholderID The symbol ID for the beholder class
     * @param cpIndex The constant pool index
     * @return true if validation succeeds, false otherwise
     */
    bool validateVirtualMethodFromCPRecord(uint16_t methodID, uint16_t definingClassID, uint16_t beholderID,
        int32_t cpIndex);

    /**
     * @brief Validate a virtual-method-from-offset record
     * @param methodID The symbol ID for the method
     * @param definingClassID The symbol ID for the defining class
     * @param beholderID The symbol ID for the beholder class
     * @param virtualCallOffset The vtable offset
     * @param ignoreRtResolve Whether to ignore runtime resolution
     * @return true if validation succeeds, false otherwise
     */
    bool validateVirtualMethodFromOffsetRecord(uint16_t methodID, uint16_t definingClassID, uint16_t beholderID,
        int32_t virtualCallOffset, bool ignoreRtResolve);

    /**
     * @brief Validate an interface-method-from-CP record
     * @param methodID The symbol ID for the method
     * @param definingClassID The symbol ID for the defining class
     * @param beholderID The symbol ID for the beholder class
     * @param lookupID The symbol ID for the lookup class
     * @param cpIndex The constant pool index
     * @return true if validation succeeds, false otherwise
     */
    bool validateInterfaceMethodFromCPRecord(uint16_t methodID, uint16_t definingClassID, uint16_t beholderID,
        uint16_t lookupID, int32_t cpIndex);

    /**
     * @brief Validate an improper-interface-method-from-CP record
     * @param methodID The symbol ID for the method
     * @param definingClassID The symbol ID for the defining class
     * @param beholderID The symbol ID for the beholder class
     * @param cpIndex The constant pool index
     * @return true if validation succeeds, false otherwise
     */
    bool validateImproperInterfaceMethodFromCPRecord(uint16_t methodID, uint16_t definingClassID, uint16_t beholderID,
        int32_t cpIndex);

    /**
     * @brief Validate a method-from-class-and-signature record
     * @param methodID The symbol ID for the method
     * @param definingClassID The symbol ID for the defining class
     * @param lookupClassID The symbol ID for the lookup class
     * @param beholderID The symbol ID for the beholder class
     * @param romMethod The ROM method
     * @return true if validation succeeds, false otherwise
     */
    bool validateMethodFromClassAndSignatureRecord(uint16_t methodID, uint16_t definingClassID, uint16_t lookupClassID,
        uint16_t beholderID, J9ROMMethod *romMethod);

    /**
     * @brief Validate a method-from-single-implementer record
     * @param methodID The symbol ID for the method
     * @param definingClassID The symbol ID for the defining class
     * @param thisClassID The symbol ID for the class with single implementer
     * @param cpIndexOrVftSlot CP index or VFT slot
     * @param callerMethodID The symbol ID for the caller method
     * @param useGetResolvedInterfaceMethod Whether to use getResolvedInterfaceMethod
     * @return true if validation succeeds, false otherwise
     */
    bool validateMethodFromSingleImplementerRecord(uint16_t methodID, uint16_t definingClassID, uint16_t thisClassID,
        int32_t cpIndexOrVftSlot, uint16_t callerMethodID, TR_YesNoMaybe useGetResolvedInterfaceMethod);

    /**
     * @brief Validate a method-from-single-interface-implementer record
     * @param methodID The symbol ID for the method
     * @param definingClassID The symbol ID for the defining class
     * @param thisClassID The symbol ID for the interface class
     * @param cpIndex The constant pool index
     * @param callerMethodID The symbol ID for the caller method
     * @return true if validation succeeds, false otherwise
     */
    bool validateMethodFromSingleInterfaceImplementerRecord(uint16_t methodID, uint16_t definingClassID,
        uint16_t thisClassID, int32_t cpIndex, uint16_t callerMethodID);

    /**
     * @brief Validate a method-from-single-abstract-implementer record
     * @param methodID The symbol ID for the method
     * @param definingClassID The symbol ID for the defining class
     * @param thisClassID The symbol ID for the abstract class
     * @param vftSlot The virtual function table slot
     * @param callerMethodID The symbol ID for the caller method
     * @return true if validation succeeds, false otherwise
     */
    bool validateMethodFromSingleAbstractImplementerRecord(uint16_t methodID, uint16_t definingClassID,
        uint16_t thisClassID, int32_t vftSlot, uint16_t callerMethodID);

    /**
     * @brief Validate a dynamic-method-from-callsite-index record
     * @param methodID The symbol ID for the method
     * @param callerID The symbol ID for the caller method
     * @param callsiteIndex The callsite index
     * @param appendixObjectNull Whether the appendix object is null
     * @param definingClassID The symbol ID for the defining class
     * @param methodIndex The method index
     * @return true if validation succeeds, false otherwise
     */
    bool validateDynamicMethodFromCallsiteIndex(uint16_t methodID, uint16_t callerID, int32_t callsiteIndex,
        bool appendixObjectNull, uint16_t definingClassID, uint32_t methodIndex);

    /**
     * @brief Validate a handle-method-from-CP-index record
     * @param methodID The symbol ID for the method
     * @param callerID The symbol ID for the caller method
     * @param cpIndex The constant pool index
     * @param appendixObjectNull Whether the appendix object is null
     * @param definingClassID The symbol ID for the defining class
     * @param methodIndex The method index
     * @return true if validation succeeds, false otherwise
     */
    bool validateHandleMethodFromCPIndex(uint16_t methodID, uint16_t callerID, int32_t cpIndex, bool appendixObjectNull,
        uint16_t definingClassID, uint32_t methodIndex);

    /**
     * @brief Validate a stack-walker-may-skip-frames record
     * @param methodID The symbol ID for the method
     * @param methodClassID The symbol ID for the method's class
     * @param couldSkipFrames Whether the stack walker could skip frames
     * @return true if validation succeeds, false otherwise
     */
    bool validateStackWalkerMaySkipFramesRecord(uint16_t methodID, uint16_t methodClassID,
        TR_YesNoMaybe couldSkipFrames);

    /**
     * @brief Validate a class-info-is-initialized record
     * @param classID The symbol ID for the class
     * @param wasInitialized Whether the class was initialized
     * @return true if validation succeeds, false otherwise
     */
    bool validateClassInfoIsInitializedRecord(uint16_t classID, bool wasInitialized);

    /**
     * @brief Validate a J2I-thunk-from-method record
     *
     * For J2IThunkFromMethod records, the actual thunk is loaded if necessary
     * in TR_RelocationRecordValidateJ2IThunkFromMethod::applyRelocation() so
     * that the thunk loading logic can be confined to RelocationRecord.cpp.
     *
     * @param thunkID The symbol ID for the thunk
     * @param thunk The thunk pointer
     * @return true if validation succeeds, false otherwise
     */
    bool validateJ2IThunkFromMethodRecord(uint16_t thunkID, void *thunk);

    /**
     * @brief Validate an is-class-visible record
     * @param sourceClassID The symbol ID for the source class
     * @param destClassID The symbol ID for the destination class
     * @param wasVisible Whether the destination class was visible
     * @return true if validation succeeds, false otherwise
     */
    bool validateIsClassVisibleRecord(uint16_t sourceClassID, uint16_t destClassID, bool wasVisible);

    /**
     * @brief Validate a methods-from-class record
     * @param classID The symbol ID for the class whose methods need IDs
     *                associated with them
     * @param startingSymbolID The starting ID used to generate IDs for the
     *                         methods of the class associated with classID
     * @return true if validation succeeds, false otherwise
     */
    bool validateMethodsFromClassRecord(uint16_t classID, uint16_t startingSymbolID);

    /**
     * @brief Get the base component class of an array class
     * @param clazz The array class
     * @param numDims Output parameter for the number of dimensions
     * @return The base component class
     */
    TR_OpaqueClassBlock *getBaseComponentClass(TR_OpaqueClassBlock *clazz, int32_t &numDims);

    /** Type alias for the list of validation records */
    typedef TR::list<SymbolValidationRecord *, TR::Region &> SymbolValidationRecordList;

    /**
     * @brief Get the list of validation records
     * @return Reference to the validation record list
     */
    SymbolValidationRecordList &getValidationRecordList() { return _symbolValidationRecords; }

    /**
     * @brief Enter a heuristic region (disables validation tracking)
     */
    void enterHeuristicRegion() { _heuristicRegion++; }

    /**
     * @brief Exit a heuristic region
     */
    void exitHeuristicRegion() { _heuristicRegion--; }

    /**
     * @brief Check if currently in a heuristic region
     * @return true if in a heuristic region, false otherwise
     */
    bool inHeuristicRegion() { return (_heuristicRegion > 0); }

    /**
     * @brief Check if SVM assertions should be fatal
     * @return true if assertions are fatal, false otherwise
     */
    static bool assertionsAreFatal();

    /**
     * @brief Get the count of system classes not worth remembering
     * @return The count of system classes not worth remembering
     */
    static int getSystemClassesNotWorthRememberingCount();

#if defined(J9VM_OPT_JITSERVER)
    /**
     * @brief Populate system classes not worth remembering (JITServer only)
     * @param clientData The client session data
     */
    static void populateSystemClassesNotWorthRemembering(ClientSessionData *clientData);
#endif /* defined(J9VM_OPT_JITSERVER) */

    /**
     * @brief Get a system class not worth remembering by index
     * @param idx The index
     * @return Pointer to the SystemClassNotWorthRemembering entry
     */
    SystemClassNotWorthRemembering *getSystemClassNotWorthRemembering(int idx);

    /** Number of system classes not worth remembering */
    static const int SYSTEM_CLASSES_NOT_WORTH_REMEMBERING_COUNT = 2;

private:
    /** Special symbol ID indicating no ID assigned */
    static const uint16_t NO_ID = 0;
    /** First valid symbol ID */
    static const uint16_t FIRST_ID = 1;

    /**
     * @brief Return the current symbol ID
     * @return The current symbol ID
     */
    uint16_t getCurrentSymbolID() { return _symbolID; }

    /**
     * @brief Get a new unique symbol ID
     *
     * Increments and returns the next available symbol ID. Asserts if the ID
     * space is exhausted (reaches 0xFFFF).
     *
     * @return A new unique symbol ID
     */
    uint16_t getNewSymbolID();

    /**
     * @brief Check if a symbol should not be defined
     *
     * Symbols should not be defined if the value is NULL or if we're currently
     * in a heuristic region (where validation tracking is disabled).
     *
     * @param value The value to check
     * @return true if the symbol should not be defined, false otherwise
     */
    bool shouldNotDefineSymbol(void *value) { return value == NULL || inHeuristicRegion(); }

    /**
     * @brief Check if a record should be abandoned
     *
     * Deallocates the record and returns whether we're in a heuristic region.
     * Records are abandoned when they're created in a heuristic region where
     * validation tracking is disabled.
     *
     * @param record The record to deallocate
     * @return true if in a heuristic region, false otherwise
     */
    bool abandonRecord(TR::SymbolValidationRecord *record);

    /**
     * @brief Check if a record already exists in the generated records set
     *
     * Used to avoid generating duplicate validation records for the same
     * validation requirement.
     *
     * @param record The record to check
     * @return true if the record exists in _alreadyGeneratedRecords, false
     *         otherwise
     */
    bool recordExists(TR::SymbolValidationRecord *record);

    /**
     * @brief Check if any ClassFromCP record exists for a class and beholder
     *        pair
     *
     * This is used to avoid redundant ClassByName records when a ClassFromCP
     * record already exists for the same class and beholder, since the
     * ClassFromCP record already validates that the class can be found from the
     * beholder's constant pool.
     *
     * @param clazz The class to check
     * @param beholder The beholder class to check
     * @return true if such a record exists in _classesFromAnyCPIndex, false
     *         otherwise
     */
    bool anyClassFromCPRecordExists(TR_OpaqueClassBlock *clazz, TR_OpaqueClassBlock *beholder);

    /**
     * @brief Append a new record to the validation list
     *
     * Asserts that we're not in a heuristic region and that the record is truly
     * new. Creates a symbol ID for the value if it doesn't already have one,
     * adds the record to both the validation list and the already-generated
     * set, and logs the record if tracing is enabled.
     *
     * @param value The value associated with the record (used for symbol ID
     *              mapping)
     * @param record The record to append (must be new)
     */
    void appendNewRecord(void *value, TR::SymbolValidationRecord *record);

    /**
     * @brief Append a record if it's new, otherwise deallocate it
     *
     * Checks if the record already exists. If it does, deallocates it to avoid
     * duplicates. If it's new, appends it to the validation list.
     *
     * @param value The value associated with the record
     * @param record The record to append or deallocate
     */
    void appendRecordIfNew(void *value, TR::SymbolValidationRecord *record);

    /**
     * @struct ClassChainInfo
     * @brief Information about a class chain
     */
    struct ClassChainInfo {
        /**
         * @brief Constructor
         */
        ClassChainInfo()
            : _baseComponent(NULL)
            , _baseComponentClassChainOffset(TR_SharedCache::INVALID_CLASS_CHAIN_OFFSET)
            , _arrayDims(0)
        {
#if defined(J9VM_OPT_JITSERVER)
            _baseComponentAOTCacheClassChainRecord = NULL;
#endif /* defined(J9VM_OPT_JITSERVER) */
        }

        /** The base component class */
        TR_OpaqueClassBlock *_baseComponent;
        /** Offset to the base component class chain */
        uintptr_t _baseComponentClassChainOffset;
        /** Number of array dimensions */
        int32_t _arrayDims;
#if defined(J9VM_OPT_JITSERVER)
        /** AOT cache class chain record for base component (JITServer only) */
        const AOTCacheClassChainRecord *_baseComponentAOTCacheClassChainRecord;
#endif /* defined(J9VM_OPT_JITSERVER) */
    };

    /**
     * @brief Get class chain information for a class
     *
     * For array classes, this extracts the base component class and the number
     * of dimensions. For non-array classes, it returns the class itself with
     * zero dimensions. Also retrieves the class chain offset from the shared
     * cache.
     *
     * @param clazz The class to analyze
     * @param record The validation record (for error reporting)
     * @param info Output parameter for class chain info (base component,
     *             offset, dimensions)
     * @return true if successful, false if class chain cannot be obtained
     */
    bool getClassChainInfo(TR_OpaqueClassBlock *clazz, TR::SymbolValidationRecord *record, ClassChainInfo &info);

    /**
     * @brief Append class chain info records for a class
     *
     * For multi-dimensional arrays, this creates validation records for each
     * level of the array hierarchy (e.g., for int[][], creates records for
     * int[], int[][], and their relationship to the base component int).
     *
     * @param clazz The array class
     * @param info The class chain info containing base component and dimensions
     */
    void appendClassChainInfoRecords(TR_OpaqueClassBlock *clazz, const ClassChainInfo &info);

    /**
     * @brief Add a vanilla (simple) validation record
     *
     * A "vanilla" record is one that doesn't require special handling for
     * classes or methods. This is the simplest form of record addition that
     * just checks if the symbol should be defined and appends the record if new.
     *
     * @param value The value associated with the record
     * @param record The record to add
     * @return true if record was added, false if it should not be defined or
     *         was abandoned
     */
    bool addVanillaRecord(void *value, TR::SymbolValidationRecord *record);

    /**
     * @brief Add a class validation record
     *
     * Handles class-specific validation record addition. Checks if the class is
     * worth remembering (not a system class we want to ignore), and handles
     * class chain information if needed.
     *
     * @param clazz The class being validated
     * @param record The class validation record to add
     * @return true if record was added, false if class is not worth remembering
     *         or was abandoned
     */
    bool addClassRecord(TR_OpaqueClassBlock *clazz, TR::ClassValidationRecord *record);

    /**
     * @brief Add a class validation record with chain
     *
     * Similar to addClassRecord but specifically for records that include a
     * class chain. Populates the class chain offset in the record and handles
     * multi-dimensional arrays by creating additional records for each array
     * level.
     *
     * @param record The class validation record with chain to add
     * @return true if record was added, false if class is not worth remembering
     *         or was abandoned
     */
    bool addClassRecordWithChain(TR::ClassValidationRecordWithChain *record);

    /**
     * @brief Add multiple array records for multi-dimensional arrays
     *
     * For a multi-dimensional array like int[][][], this creates validation
     * records for each level: int[], int[][], int[][][], and their
     * relationships to the base component type.
     *
     * @param clazz The multi-dimensional array class
     * @param arrayDims The number of array dimensions
     */
    void addMultipleArrayRecords(TR_OpaqueClassBlock *clazz, int arrayDims);

    /**
     * @brief Add a method validation record
     *
     * Handles method-specific validation record addition. Ensures the method's
     * defining class is also validated and tracked.
     *
     * @param record The method validation record to add
     * @return true if record was added, false if method should not be defined
     *         or was abandoned
     */
    bool addMethodRecord(TR::MethodValidationRecord *record);

    /**
     * @brief Check if a field reference class record should be skipped
     *
     * Determines whether a validation record for a field's declaring class can
     * be skipped because an equivalent record already exists (e.g., a
     * ClassFromCP record that already validates the same class from the same
     * beholder).
     *
     * @param definingClass The class that defines the field
     * @param beholder The class from which the field is accessed
     * @param cpIndex The constant pool index
     * @return true if the record should be skipped, false if it's needed
     */
    bool skipFieldRefClassRecord(TR_OpaqueClassBlock *definingClass, TR_OpaqueClassBlock *beholder, uint32_t cpIndex);

    /**
     * @brief Validate a symbol by ID and value
     *
     * Core validation method that checks if a symbol ID maps to the expected
     * value and type. Used during AOT load to ensure symbols are still valid.
     *
     * @param idToBeValidated The symbol ID to validate
     * @param validValue The expected value for this symbol
     * @param type The expected symbol type
     * @return true if the symbol ID maps to validValue with the correct type,
     *         false otherwise
     */
    bool validateSymbol(uint16_t idToBeValidated, void *validValue, TR::SymbolType type);

    /**
     * @brief Validate a class symbol
     *
     * Convenience overload for validating class symbols. Calls the core
     * validateSymbol method with typeClass.
     *
     * @param idToBeValidated The symbol ID to validate
     * @param clazz The expected class value
     * @return true if validation succeeds, false otherwise
     */
    bool validateSymbol(uint16_t idToBeValidated, TR_OpaqueClassBlock *clazz);

    /**
     * @brief Validate a J9Class symbol
     *
     * Convenience overload for validating J9Class symbols. Calls the core
     * validateSymbol method with typeClass.
     *
     * @param idToBeValidated The symbol ID to validate
     * @param clazz The expected J9Class value
     * @return true if validation succeeds, false otherwise
     */
    bool validateSymbol(uint16_t idToBeValidated, J9Class *clazz);

    /**
     * @brief Validate a method symbol with its defining class
     *
     * Validates both the method and ensures it belongs to the expected defining
     * class. This two-level validation ensures the method hasn't been redefined
     * or moved.
     *
     * @param methodID The method symbol ID to validate
     * @param definingClassID The symbol ID of the expected defining class
     * @param method The expected method value
     * @return true if validation succeeds, false otherwise
     */
    bool validateSymbol(uint16_t methodID, uint16_t definingClassID, TR_OpaqueMethodBlock *method);

    /**
     * @brief Validate a J9Method symbol with its defining class
     *
     * Validates both the J9Method and ensures it belongs to the expected
     * defining class.
     *
     * @param methodID The method symbol ID to validate
     * @param definingClassID The symbol ID of the expected defining class
     * @param method The expected J9Method value
     * @return true if validation succeeds, false otherwise
     */
    bool validateSymbol(uint16_t methodID, uint16_t definingClassID, J9Method *method);

    /**
     * @brief Check if a symbol ID has been defined
     *
     * Checks if the symbol ID is within the valid range and has a value
     * assigned. Used during AOT load to verify symbol IDs before accessing
     * their values.
     *
     * @param id The symbol ID to check
     * @return true if the ID is defined in _symbolToValueTable, false otherwise
     */
    bool isDefinedID(uint16_t id);

    /**
     * @brief Set the value of a symbol ID
     *
     * Associates a value and type with a symbol ID. Resizes the symbol-to-value
     * table if necessary. Asserts if the ID already has a value (no
     * redefinition).
     *
     * @param id The symbol ID
     * @param value The value to associate with this ID
     * @param type The type of the symbol
     */
    void setValueOfSymbolID(uint16_t id, void *value, TR::SymbolType type);

    /**
     * @brief Define a guaranteed symbol ID for a value
     *
     * Creates a new symbol ID for a value that is guaranteed to be exist
     * (e.g., the root class, primitive array classes). Adds the value to both
     * the value-to-symbol map and the symbol-to-value table, and marks it as
     * seen. Used during SVM initialization for essential symbols.
     *
     * @param value The value to define
     * @param type The symbol type
     */
    void defineGuaranteedID(void *value, TR::SymbolType type);

    /**
     * @brief Heuristic to determine whether a class is worth remembering
     *
     * Determines if a class should be tracked for AOT validation. Some system
     * classes (like Throwable and its subclasses, or StringBuffer) are excluded
     * because they're unlikely to affect mainline performance and including
     * them increases the risk of AOT load failures. This heuristic balances
     * optimization opportunities against load reliability.
     *
     * @param clazz The class being considered
     * @return true if the class is worth remembering for AOT validation, false
     *         otherwise
     */
    bool isClassWorthRemembering(TR_OpaqueClassBlock *clazz);

    /** Monotonically increasing symbol IDs */
    uint16_t _symbolID;

    /** Heuristic region nesting depth */
    uint32_t _heuristicRegion;

    /** Memory region for allocations */
    TR::Region &_region;

    /** The compilation object */
    TR::Compilation * const _comp;
    /** The VM thread */
    J9VMThread * const _vmThread;
    /** The frontend interface */
    TR_J9VM * const _fej9; // DEFAULT_VM
    /** TR memory interface */
    TR_Memory * const _trMemory;
    /** Persistent class hierarchy table */
    TR_PersistentCHTable * const _chTable;

    /** The root class of the compilation */
    TR_OpaqueClassBlock *_rootClass;
    /** Well-known class chain offsets */
    const void *_wellKnownClassChainOffsets;
#if defined(J9VM_OPT_JITSERVER)
    /** AOT cache well-known classes record (JITServer only) */
    const AOTCacheWellKnownClassesRecord *_aotCacheWellKnownClassesRecord;
#endif /* defined(J9VM_OPT_JITSERVER */

    /** List of validation records to be written to the AOT buffer */
    SymbolValidationRecordList _symbolValidationRecords;

    /** Type alias for record pointer allocator */
    typedef TR::typed_allocator<SymbolValidationRecord *, TR::Region &> RecordPtrAlloc;
    /** Type alias for set of records */
    typedef std::set<SymbolValidationRecord *, LessSymbolValidationRecord, RecordPtrAlloc> RecordSet;
    /** Set of already generated records (for deduplication) */
    RecordSet _alreadyGeneratedRecords;

    /** Type alias for value-to-symbol map allocator */
    typedef TR::typed_allocator<std::pair<void * const, uint16_t>, TR::Region &> ValueToSymbolAllocator;
    /** Type alias for value-to-symbol map comparator */
    typedef std::less<void *> ValueToSymbolComparator;
    /** Type alias for value-to-symbol map */
    typedef std::map<void *, uint16_t, ValueToSymbolComparator, ValueToSymbolAllocator> ValueToSymbolMap;

    /**
     * @struct TypedValue
     * @brief A value with its associated type
     */
    struct TypedValue {
        void *_value; /**< The value */
        TR::SymbolType _type; /**< The type of the value */
        bool _hasValue; /**< Whether a value has been set */
    };

    /** Type alias for symbol-to-value table allocator */
    typedef TR::typed_allocator<TypedValue, TR::Region &> SymbolToValueAllocator;
    /** Type alias for symbol-to-value table */
    typedef std::vector<TypedValue, SymbolToValueAllocator> SymbolToValueTable;

    /** Type alias for seen values set allocator */
    typedef TR::typed_allocator<void *, TR::Region &> SeenValuesAlloc;
    /** Type alias for seen values set comparator */
    typedef std::less<void *> SeenValuesComparator;
    /** Type alias for seen values set */
    typedef std::set<void *, SeenValuesComparator, SeenValuesAlloc> SeenValuesSet;

    /** Map from values to symbol IDs (used during AOT compile) */
    ValueToSymbolMap _valueToSymbolMap;

    /** Table from symbol IDs to values (used during AOT load) */
    SymbolToValueTable _symbolToValueTable;

    /** Set of values that have been seen */
    SeenValuesSet _seenValuesSet;

    /** Type alias for class vector allocator */
    typedef TR::typed_allocator<TR_OpaqueClassBlock *, TR::Region &> ClassAllocator;
    /** Type alias for class vector */
    typedef std::vector<TR_OpaqueClassBlock *, ClassAllocator> ClassVector;
    /** Vector of well-known classes */
    ClassVector _wellKnownClasses;

    /** Type alias for loader vector allocator */
    typedef TR::typed_allocator<J9ClassLoader *, TR::Region &> LoaderAllocator;
    /** Type alias for loader vector */
    typedef std::vector<J9ClassLoader *, LoaderAllocator> LoaderVector;
    /** Vector of loaders that can see well-known classes */
    LoaderVector _loadersOkForWellKnownClasses;

    /**
     * @struct ClassFromAnyCPIndex
     * @brief Tracks classes found in constant pools
     *
     * Remember which classes have already been found in which beholders'
     * constant pools, regardless of CP index. If a class C has already been
     * found in the constant pool of class D (producing a ClassFromCP record),
     * then ClassByName(C, D) is redundant.
     */
    struct ClassFromAnyCPIndex {
        TR_OpaqueClassBlock *clazz; /**< The class */
        TR_OpaqueClassBlock *beholder; /**< The beholder class */

        /**
         * @brief Constructor
         * @param clazz The class
         * @param beholder The beholder class
         */
        ClassFromAnyCPIndex(TR_OpaqueClassBlock *clazz, TR_OpaqueClassBlock *beholder)
            : clazz(clazz)
            , beholder(beholder)
        {}
    };

    /**
     * @struct LessClassFromAnyCPIndex
     * @brief Comparison functor for ClassFromAnyCPIndex
     */
    struct LessClassFromAnyCPIndex {
        /**
         * @brief Compare two ClassFromAnyCPIndex entries
         * @param a First entry
         * @param b Second entry
         * @return true if a < b, false otherwise
         */
        bool operator()(const ClassFromAnyCPIndex &a, const ClassFromAnyCPIndex &b) const
        {
            std::less<TR_OpaqueClassBlock *> lt;
            if (lt(a.clazz, b.clazz))
                return true;
            else if (lt(b.clazz, a.clazz))
                return false;
            else
                return lt(a.beholder, b.beholder);
        }
    };

    /** Type alias for ClassFromAnyCPIndex set allocator */
    typedef TR::typed_allocator<ClassFromAnyCPIndex, TR::Region &> ClassFromAnyCPIndexAlloc;
    /** Type alias for ClassFromAnyCPIndex set */
    typedef std::set<ClassFromAnyCPIndex, LessClassFromAnyCPIndex, ClassFromAnyCPIndexAlloc> ClassFromAnyCPIndexSet;
    /** Set of classes found in constant pools */
    ClassFromAnyCPIndexSet _classesFromAnyCPIndex;

    /** Static array of system classes not worth remembering */
    static SystemClassNotWorthRemembering _systemClassesNotWorthRemembering[];
};

} // namespace TR

#endif // SYMBOL_VALIDATION_MANAGER_INCL
