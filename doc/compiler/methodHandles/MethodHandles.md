<!--
Copyright IBM Corp. and others 2026

This program and the accompanying materials are made available under
the terms of the Eclipse Public License 2.0 which accompanies this
distribution and is available at https://www.eclipse.org/legal/epl-2.0/
or the Apache License, Version 2.0 which accompanies this distribution and
is available at https://www.apache.org/licenses/LICENSE-2.0.

This Source Code may also be made available under the following
Secondary Licenses when the conditions for such availability set
forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
General Public License, version 2 with the GNU Classpath
Exception [1] and GNU General Public License, version 2 with the
OpenJDK Assembly Exception [2].

[1] https://www.gnu.org/software/classpath/license.html
[2] https://openjdk.org/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0

Assisted-by: IBM Bob
-->

# MethodHandle and VarHandle in the OpenJ9 JIT Compiler

## Table of Contents

1. [Introduction](#introduction)
2. [Background: Java MethodHandle Architecture](#background-java-methodhandle-architecture)
   - [What is a MethodHandle?](#what-is-a-methodhandle)
   - [LambdaForm: The Execution Engine](#lambdaform-the-execution-engine)
   - [Key JCL Classes](#key-jcl-classes)
   - [MethodHandle Invocation Bytecodes](#methodhandle-invocation-bytecodes)
3. [OpenJ9 MethodHandles vs OpenJDK MethodHandles](#openj9-methodhandles-vs-openjdk-methodhandles)
4. [OpenJDK MethodHandle Execution Model](#openjdk-methodhandle-execution-model)
   - [Invoke Cache and MemberName](#invoke-cache-and-membername)
   - [linkTo* Methods](#linkto-methods)
   - [Appendix Objects](#appendix-objects)
5. [Known Object Table Integration](#known-object-table-integration)
6. [IL Generation Phase](#il-generation-phase)
   - [invokedynamic Handling](#invokedynamic-handling)
   - [invokehandle Handling](#invokehandle-handling)
   - [loadInvokeCacheArrayElements](#loadinvokecachearrayelements)
   - [Tracking Flags and Bitsets](#tracking-flags-and-bitsets)
7. [LambdaForm Method Compilation](#lambdaform-method-compilation)
8. [MethodHandleTransformer Optimization](#methodhandletransformer-optimization)
   - [When It Runs](#when-it-runs)
   - [Known Object Propagation](#known-object-propagation)
   - [invokeBasic Refinement](#invokebasic-refinement)
   - [linkTo* Refinement](#linkto-refinement)
   - [checkExactType Elimination](#checkexacttype-elimination)
   - [checkCustomized Elimination](#checkcustomized-elimination)
   - [checkVarHandleGenericType Folding](#checkvarhandlegenerictype-folding)
9. [J9TransformUtil: Call Refinement](#j9transformutil-call-refinement)
   - [refineMethodHandleInvokeBasic](#refinemethodhandleinvokebasic)
   - [refineMethodHandleLinkTo](#refinemethodhandlelinkto)
10. [RecognizedCallTransformer: Fallback MethodHandle
    Lowering](#recognizedcalltransformer-fallback-methodhandle-lowering)
    - [Fallback invokeBasic Lowering](#fallback-invokebasic-lowering)
    - [Fallback linkToStatic / linkToSpecial Lowering](#fallback-linktostatic--linktospecial-lowering)
    - [processVMInternalNativeFunction: The Diamond
      Transform](#processvminternalnativefunction-the-diamond-transform)
    - [Fallback linkToVirtual and linkToInterface
      Lowering](#fallback-linktovirtual-and-linktointerface-lowering)
11. [Inliner Support](#inliner-support)
    - [InterpreterEmulator and Callee Refinement](#interpreteremulator-and-callee-refinement)
    - [TR_J9MethodHandleCallSite](#tr-j9methodhandlecallsite)
    - [TR_J9MutableCallSite and MutableCallSite Guards](#tr-j9mutablecallsite-and-mutablecallsite-guards)
12. [GlobalValuePropagation: Late MethodHandle
    Refinement](#globalvaluepropagation-late-methodhandle-refinement)
    - [How GVP Refines MethodHandle Calls](#how-gvp-refines-methodhandle-calls)
    - [Delayed Inlining](#delayed-inlining)
13. [Code Generation: linkTo* Tree Lowering](#code-generation-linkto-tree-lowering)
    - [Storing the Argument Slot Count in
      vmThread.tempSlot](#storing-the-argument-slot-count-in-vmthreadtempslot)
    - [Communicating Appendix Removal via
      vmThread.floatTemp1](#communicating-appendix-removal-via-vmthreadfloattemp1)
    - [linkToNative: Reserving Stack Space for the
      Appendix](#linktonative-reserving-stack-space-for-the-appendix)
14. [VarHandle Support](#varhandle-support)
    - [VarHandle Architecture](#varhandle-architecture)
    - [checkVarHandleGenericType Folding (OpenJDK MH)](#checkvarhandlegenerictype-folding-openjdk-mh)
    - [UnsafeFastPath for VarHandle Operation Methods](#unsafefastpath-for-varhandle-operation-methods)
    - [VarHandleTransformer (Legacy OpenJ9 MH Only)](#varhandletransformer-legacy-openj9-mh-only)
15. [Recognized Methods](#recognized-methods)
16. [Compilation Restrictions](#compilation-restrictions)
    - [AOT Compilation](#aot-compilation)
    - [Full Speed Debug (FSD)](#full-speed-debug-fsd)
17. [End-to-End Optimization Pipeline](#end-to-end-optimization-pipeline)
    - [MethodHandle Example](#methodhandle-example)
    - [VarHandle Example](#varhandle-example)
18. [Real-World Example and JIT Walkthrough](#real-world-example-and-jit-walkthrough)
    - [JIT Pipeline Walkthrough](#jit-pipeline-walkthrough)
19. [Performance Optimization Opportunities](#performance-optimization-opportunities)
20. [Key Source File Reference](#key-source-file-reference)

---

## Introduction

MethodHandles are a central feature of modern Java, underpinning lambda expressions,
`invokedynamic`-based constructs, `VarHandle` memory access, Reflection (in Java 21 and later),
and general dynamic dispatch. For the JIT compiler to generate high-quality code for these
constructs, it must see through multiple layers of indirection introduced by the MethodHandle framework.

This document describes how the OpenJ9 JIT compiler handles `MethodHandle` and `VarHandle`
operations, covering IL generation, optimization passes, inlining, and the Known Object Table. It is
primarily focused on the **OpenJDK MethodHandle** implementation (currently enabled in Java 17+), which uses
LambdaForm-generated adapter methods and `MemberName` objects. Brief differences from the older
**OpenJ9 MethodHandle** implementation (currently used in Java 8 and 11) are noted where relevant.
The document is not going to go in-depth into the implementation of OpenJ9 MethodHandles, as it is going to be
phased out and all Java versions will be using OpenJDK MethodHandle implementation.

The guard macro `J9VM_OPT_OPENJDK_METHODHANDLE` is used for code that is specific to
OpenJDK MethodHandles.

---

## Background: Java MethodHandle Architecture

### What is a MethodHandle?

A `MethodHandle` is a typed reference to a method, constructor, field, or other callable entity. It
is implemented in `java.lang.invoke.MethodHandle` and other classes in the `java.lang.invoke` package.

Key properties:
- **Signature polymorphism**: `invokeExact` and `invoke` are signature-polymorphic, meaning the
  Java compiler emits them as `invokevirtual` with a descriptor matching the call site, not the
  nominal `(Object[])Object` descriptor.
- **Type safety**: Every `MethodHandle` carries a `MethodType` describing its parameter and return
  types. `invokeExact` enforces exact type matching; `invoke` performs adaptation.
- **Immutability**: MethodHandle objects are effectively immutable, making compile-time
  specialization possible when the JIT can determine the handle's identity.

### LambdaForm: The Execution Engine

`LambdaForm` is the internal symbolic representation of a MethodHandle's invocation semantics. It is a simple
intermediate language that captures a sequence of named values and function applications.

A LambdaForm is compiled into a Java method by the JCL at runtime (via bytecode generation or
interpreted via a built-in interpreter). When compiled, these become **LambdaForm generated
methods** — regular Java methods that chain together invocations such as `invokeBasic`,
`linkToStatic`, `linkToVirtual`, and `linkToInterface`.

For example, a `DirectMethodHandle` to a static method might produce a LambdaForm body that calls:
1. `Invokers.checkExactType(mh, expectedType)` — type check
2. `Invokers.checkCustomized(mh)` — customization check
3. `MethodHandle.invokeBasic(mh, ...args)` — the actual dispatch

The JIT compiler, when it compiles a LambdaForm-generated method, must optimize away these
intermediate calls to reach the final target.

### Key JCL Classes

| Class | Role |
|-------|------|
| `MethodHandle` | Base class; holds `type` (MethodType) and `form` (LambdaForm) |
| `LambdaForm` | Symbolic invocation graph; compiled to bytecode at runtime |
| `DirectMethodHandle` | MH directly wrapping a method/field; has `member` (MemberName) |
| `BoundMethodHandle` | MH with bound arguments (captured variables); used by lambdas |
| `DelegatingMethodHandle` | Delegates to another MH (e.g., for `asType` adapters) |
| `MemberName` | A lightweight reference to a method/field with VM-internal vmtarget pointer |
| `Invokers` | Utility class providing `invokeExact`/`invoke`/`invokeBasic` entry stubs |
| `MethodHandles.Lookup` | Factory for creating MethodHandles with access control |
| `MethodType` | Describes the parameter and return types of a MethodHandle |
| `VarHandle` | Typed reference for memory access operations with ordering guarantees |

### MethodHandle Invocation Bytecodes

The JVM uses two specialized bytecodes (or virtual bytecodes in J9) for MethodHandle
invocation:

| Bytecode | Description |
|----------|-------------|
| `invokedynamic` | Dynamic call site linked via `BootstrapMethod`; used for lambdas, `invokedynamic` constructs |
| `invokehandle` | Virtual bytecode in J9; represents a signature-polymorphic `invokeExact` or `invoke` call on a `MethodHandle` |
| `invokehandlegeneric` | Similar to `invokehandle` but for `invoke` (inexact); requires `asType` conversion |
| `invokevirtual` | Normal virtual dispatch; used internally by MethodHandle adapters |
| `invokestatic` | Used by `linkToStatic`, `linkToSpecial` internally |

`invokehandle` and `invokedynamic` are the two primary entry points for MethodHandle compilation in
the JIT. `invokevirtual` and `invokestatic` are used internally by the LambdaForm adapter methods
that the JCL generates for MethodHandle dispatch.

---

## OpenJ9 MethodHandles vs OpenJDK MethodHandles

Prior to Java 17 (when running on OpenJ9 with Java 8 or Java 11), OpenJ9 used its own proprietary
MethodHandle implementation (**OpenJ9 MethodHandles**). Key characteristics of the old approach:

- MethodHandle objects had a different internal layout.
- Invocation was driven by **MethodHandle thunks**: hand-crafted IL trees that the compiler
  generated for common MethodHandle patterns.
- VarHandle operations were handled through separate machinery.
- The compiler recognized specific archetype patterns and inlined them directly.
- `TR_J9MethodHandleCallSite` was the primary mechanism for inlining thunk archetype methods.

Starting with Java 17 (and controlled by the `J9VM_OPT_OPENJDK_METHODHANDLE` compile-time macro),
OpenJ9 switched to the **OpenJDK MethodHandle** implementation. Key differences:

- MethodHandle invocation is implemented entirely in JCL Java code via LambdaForm-generated adapter
  methods.
- The VM provides a set of low-level intrinsic methods (`invokeBasic`, `linkToStatic`, etc.) that
  the JCL uses as building blocks.
- VarHandle operations are dispatched through a table of MethodHandles.
- The JIT must inline and optimize through multiple levels of JCL-generated Java code.

The remainder of this document focuses on the **OpenJDK MethodHandle** path.

---

## OpenJDK MethodHandle Execution Model

### Invoke Cache and MemberName

When `invokedynamic` or `invokehandle` is resolved by the VM, the result is stored in an **invoke
cache array** (a two-element array):
- **Index 0 (memberName)**: A `MemberName` object pointing to the actual target method.
- **Index 1 (appendix)**: An optional object appended as the last argument to the call (e.g., the
  `MethodType` for type-checked dispatch, or a bound `CallSite` object).

The `MemberName` object carries:
- `vmtarget`: A pointer to the J9Method (or vtable slot) of the resolved target.
- `vmindex` (refKind): The reference kind (`REF_invokeVirtual`, `REF_invokeStatic`, etc.).

Constants for array indices:
- `JSR292_invokeCacheArrayMemberNameIndex = 0`
- `JSR292_invokeCacheArrayAppendixIndex = 1`

### linkTo* Methods

The JCL uses a family of low-level VM intrinsics called `linkTo*` to dispatch through a
`MemberName`:

| Method | Dispatch Kind |
|--------|--------------|
| `MethodHandle.linkToStatic(args..., MemberName mn)` | Static or special dispatch to `mn.vmtarget` |
| `MethodHandle.linkToSpecial(receiver, args..., MemberName mn)` | Special (non-virtual) dispatch |
| `MethodHandle.linkToVirtual(receiver, args..., MemberName mn)` | Virtual dispatch using `mn.vmindex` as vtable slot |
| `MethodHandle.linkToInterface(receiver, args..., MemberName mn)` | Interface dispatch |
| `MethodHandle.linkToNative(args..., MemberName mn)` | Native method dispatch |

These methods are recognized by the JIT and refined into direct or indirect IL calls when the
`MemberName` is known.

### Appendix Objects

The appendix plays different roles depending on the call site:
- For `invokedynamic` backed by a `ConstantCallSite` pointing to a lambda: the appendix may be a
  captured-argument array or null.
- For `invokehandle` (i.e., `mh.invokeExact(...)`): the appendix is the `MethodType` expected at
  the call site, used for type checking.
- For resolved call sites with a direct target: the appendix is passed as the last argument to the
  resolved method.

---

## Known Object Table Integration

The **Known Object Table** (`TR::KnownObjectTable`) is the backbone of MethodHandle optimization. It
maintains a mapping from heap object addresses (valid at compilation time) to compiler-opaque
indices (`KnownObjectTable::Index`). The term **KOI** (Known Object Index) refers to a specific
index into this table. Understanding KOT is a prerequisite for the IL generation, transformer, and
inliner sections that follow.

Key uses in MethodHandle compilation:

| Use Case | How KOT Is Used |
|----------|----------------|
| Appendix objects | Loaded from invoke cache; annotated on symRef for downstream optimization |
| MemberName objects | Loaded from invoke cache or from DirectMethodHandle.member |
| MethodHandle receivers | Tracked through local variables by MethodHandleTransformer |
| MethodType objects | Used by checkExactType elimination |
| VarHandle/AccessDescriptor | Used by checkVarHandleGenericType folding |
| MutableCallSite targets | Used by TR_J9MutableCallSite for epoch-based inlining guards |

Symbol references for loads from known objects are enhanced with KOI information, enabling:
- **Constant folding**: Loads from known immutable fields become constants.
- **Type refinement**: The exact class of a known object is available, enabling more precise
  devirtualization.
- **Guard elimination**: Type and identity checks against known objects can be folded.

---

## IL Generation Phase

### invokedynamic Handling

**Entry point:** `TR_J9ByteCodeIlGenerator::genInvokeDynamic(int32_t callSiteIndex)`

This function handles the `invokedynamic` bytecode. Under `J9VM_OPT_OPENJDK_METHODHANDLE`, it
implements the following logic:

**Resolved call site with non-null appendix:**
```
call <target method from MemberName>
  arg0, arg1, ..., argN        // original call arguments
  aloadi <appendix object>     // from invoke cache [1]
```
The target method and appendix object are obtained from the resolved invoke cache array. The
appendix is loaded from the cache and appended as the last argument.

**Resolved call site with null appendix:**
```
call <target method from MemberName>
  arg0, arg1, ..., argN
```
No appendix is appended.

**Unresolved call site:**
```
ResolveCHK
  aload <CallSiteTableEntry>
treetop
  aloadi <appendix>             // from invoke cache [JSR292_invokeCacheArrayAppendixIndex]
treetop
  aloadi <memberName>           // from invoke cache [JSR292_invokeCacheArrayMemberNameIndex]
treetop
  call linkToStatic(arg0..argN, appendix, memberName)
```
For unresolved sites, the compiler generates a resolution check and delegates to `linkToStatic`,
passing both the appendix and `MemberName` as trailing arguments.

**Compilation restrictions checked here:**
- AOT compilation is rejected unless both `TR_EnableMHRelocatableCompile` and
  `TR_UseSymbolValidationManager` options are active.
- Full Speed Debug (FSD) is rejected unless the current compilation is a peeking pass.

### invokehandle Handling

**Entry point:** `TR_J9ByteCodeIlGenerator::genInvokeHandle(int32_t cpIndex)`

This handles the `invokehandle` bytecode, which corresponds to a signature-polymorphic call to
`MethodHandle.invokeExact` at a specific call-site type. The structure is similar to
`invokedynamic`:

**Resolved with non-null appendix:**
```
call <target method from MemberName>
  aload <MethodHandle receiver>
  arg0, ..., argN
  aloadi <appendix>
```

**Unresolved:**
```
ResolveCHK
  aload <MethodTypeTableEntry>
treetop
  aloadi <appendix>
treetop
  aloadi <memberName>
treetop
  call linkToStatic(receiver, arg0..argN, appendix, memberName)
```

The overloaded `genInvokeHandle(TR::SymbolReference *invokeExactSymRef, TR::Node
*invokedynamicReceiver)` is called for the old (non-OpenJDK) path and handles the invokeExact
expansion, setting:
- `_methodSymbol->setHasMethodHandleInvokes(true)` — flags the method as containing MH
  invocations
- `_methodSymbol->setMayHaveIndirectCalls(true)` — enables indirect call optimizations

### loadInvokeCacheArrayElements

**Function:** `TR_J9ByteCodeIlGenerator::loadInvokeCacheArrayElements(TR::SymbolReference
*tableEntrySymRef, uintptr_t *invokeCacheArray, bool isUnresolved)`

This helper generates IL to load the appendix and (for unresolved sites) the `MemberName` from the
invoke cache array:

1. **Known appendix with const refs enabled**: Directly uses a constant symbol reference for the
   known object, avoiding a load entirely.
2. **Known appendix without const refs**: Loads from the cache array but annotates the load's
   symbol reference with the known object index (KOI) for downstream optimization.
3. **Unresolved**: Loads both appendix (at index 1) and memberName (at index 0) with unresolved
   references.

### Tracking Flags and Bitsets

During IL generation, several bitsets are maintained to track which bytecode indices contain
MethodHandle-related invocations:

| Bitset | Purpose |
|--------|---------|
| `_methodHandleInvokeCalls` | Bytecode indices of `invokeExact`-style invocations |
| `_invokeHandleCalls` | Indices of `invokehandle` bytecodes |
| `_invokeHandleGenericCalls` | Indices of `invokehandlegeneric` bytecodes |
| `_invokeDynamicCalls` | Indices of `invokedynamic` bytecodes |
| `_ilGenMacroInvokeExactCalls` | Indices of IL generation macro invocations |

These are used by subsequent optimization passes to identify and categorize method handle
invocations.

---

## LambdaForm Method Compilation

When the JIT compiles a **LambdaForm-generated method** (a bytecode-compiled adapter produced by the
JCL's `InvokerBytecodeGenerator` or similar), it encounters a sequence of calls like:

```java
// Typical LambdaForm-compiled body:
void invoke_MT(MethodHandle mh, Object arg0, Object arg1) {
    Invokers.checkExactType(mh, MT_[Object,Object]void);
    Invokers.checkCustomized(mh);
    mh.invokeBasic(arg0, arg1);
}
```

The JIT recognizes these methods (via `isLambdaFormGeneratedMethod()` on the resolved method) and
treats them differently:
- **`MethodHandleTransformer`** runs during the ILGen optimization strategy to propagate known
  object information and refine `invokeBasic`/`linkTo*` calls.
- **`InterpreterEmulator`** (with state information) is used by the inliner during
  `estimateCodeSize` to symbolically execute the adapter and determine the ultimate call target,
  rather than using peeking ILGen.
- Checks like `checkExactType` and `checkCustomized` are candidates for elimination.
- `invokeBasic` is a candidate for refinement to a direct static call.

---

## MethodHandleTransformer Optimization

### When It Runs

`TR_MethodHandleTransformer::perform()` only activates when the compilation target is either:
1. A **LambdaForm-generated method** (identified by `isLambdaFormGeneratedMethod()`), or
2. A **peeking ILGen** pass (identified by `isPeekingMethod()`).

The transformer runs at several points in the compilation pipeline:

- **During ILGen optimization** (as part of `ilgenStrategyOpts`): When the method being compiled is
  itself a LambdaForm-generated method, the transformer runs immediately after ILGen to refine
  `invokeBasic` and `linkTo*` calls into their underlying target methods.
- **During peeking ILGen**: When the inliner is evaluating a non-LambdaForm callee for inlining
  (e.g., a regular method that contains `invokedynamic` or `invokehandle`), peeking ILGen generates
  temporary IL for the callee, then runs `MethodHandleTransformer` on it to propagate known object
  information. The generated IL is discarded after the inliner extracts the information it needs.
  Peeking ILGen is **not** used for LambdaForm-generated methods themselves — those are handled by
  `InterpreterEmulator` with state information (see [InterpreterEmulator and Callee
  Refinement](#interpreteremulator-and-callee-refinement)).
- **During inlining transformation**: When a LambdaForm-generated method is being inlined into a
  non-LambdaForm caller (e.g., a regular Java method), the transformer runs on the inlined body
  as part of the `methodHandleInvokeInliningGroup` to refine the MethodHandle calls within the
  newly inlined code.

This restriction is important: the transformer assumes it is operating within an adapter method
where MethodHandle objects flow through a small set of local variables with known identities.
Running it on arbitrary methods would be incorrect and expensive.

### Known Object Propagation

The transformer implements a **dataflow analysis** over address-typed local variables:

1. **Index assignment**: All address-typed parameters and automatics are assigned a local index.
2. **Block-level propagation**: A reverse post-order CFG traversal propagates `ObjectInfo` — a
   vector of `KnownObjectTable::Index` values — from block entries to exits.
3. **Merge at joins**: At control flow joins, object info is conservatively merged: a slot retains
   its known-object index only if all predecessors agree on the same index (intersection semantics).
4. **Field folding**: The transformer recognizes field loads from known objects (e.g.,
   `DirectMethodHandle.member`, `DelegatingMethodHandle.target`) and folds them to their known
   values, extending the set of known objects in scope.

The result is that when the transformer encounters a call like `invokeBasic(mh, ...)`, it may know
that `mh` is a specific `MethodHandle` instance from the Known Object Table, enabling precise
refinement.

### invokeBasic Refinement

**Function:** `process_java_lang_invoke_MethodHandle_invokeBasic(TreeTop *tt, Node *node)`

When the first argument to `invokeBasic` is a known `MethodHandle` object:
1. The VM front-end extracts the target `J9Method` from the handle via
   `fej9()->targetMethodFromMethodHandle(mhIndex)`.
2. A new symbol reference is created for the target method.
3. The `invokeBasic` call is replaced with a **direct static call** to the target.
4. A `NULLCHK` is separated from the call if needed.
5. `doNotProfile` is set to `false` to preserve OSR (On-Stack Replacement) capability.

Before refinement:
```
call invokeBasic
  aload mh          // known object: KOI #7
  aload arg0
  aload arg1
```

After refinement:
```
NULLCHK mh
call <DirectMethodHandle target> [static direct]
  aload mh
  aload arg0
  aload arg1
```

### linkTo* Refinement

**Function:** `process_java_lang_invoke_MethodHandle_linkTo(TreeTop *tt, Node *node)`

When the last argument to a `linkTo*` call is a known `MemberName`:
1. Delegates to `TR::TransformUtil::refineMethodHandleLinkTo()`.
2. The `MemberName`'s `vmtarget` and `refKind` are retrieved via
   `fej9()->getMemberNameMethodInfo()`.
3. The call is refined based on the ref kind (see [J9TransformUtil: Call
   Refinement](#j9transformutil-call-refinement)).

### checkExactType Elimination

**Function:** `process_java_lang_invoke_Invokers_checkExactType(TreeTop *tt, Node *node)`

`Invokers.checkExactType(mh, expectedType)` verifies that `mh.type() == expectedType`. When both
arguments are known objects:
- **Types match**: The check is statically provable; replaced with a `PassThrough` node
  (eliminated).
- **Types do not match**: The check will always fail; replaced with a `ZEROCHK` that throws
  `WrongMethodTypeException`:
  ```
  ZEROCHK
    acmpeq
      aload expectedType    // known object
      aloadi mh.type        // load from known MH
  ```
  A null check on the type field is also inserted.

### checkCustomized Elimination

**Function:** `process_java_lang_invoke_Invokers_checkCustomized(TreeTop *tt, Node *node)`

`Invokers.checkCustomized(mh)` is a hook for the JVM's MethodHandle customization mechanism. When
`mh` is a known object, the customization state is already fixed at compile time, and the check is
unconditionally replaced with `PassThrough` (eliminated).

### checkVarHandleGenericType Folding

**Function:** `process_java_lang_invoke_Invokers_checkVarHandleGenericType(TreeTop *tt, Node
*node)`

`Invokers.checkVarHandleGenericType(vh, ad)` determines which `MethodHandle` to use for a given
`VarHandle` operation described by an `AccessDescriptor`. When both the `VarHandle` and
`AccessDescriptor` are known objects:

1. The transformer computes the table index from the `AccessDescriptor`'s access mode.
2. **JDK ≤ 17**: Loads `typesAndInvokers` field, then indexes into `methodHandle_table`.
3. **JDK ≥ 21**: Loads directly from `methodHandleTable` field.
4. An `aload` or array-element load with the folded known-object index is substituted.
5. A `SpineCHK` is inserted if arraylet support is enabled.

This transformation is critical for VarHandle performance: it converts a runtime table lookup (with
a full type check) into a direct load of the target MethodHandle.

---

## J9TransformUtil: Call Refinement

These functions are guarded by `#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)`.

### refineMethodHandleInvokeBasic

**Signature:**
```cpp
bool J9::TransformUtil::refineMethodHandleInvokeBasic(
    TR::Compilation *comp,
    TR::TreeTop *treetop,
    TR::Node *node,
    TR::KnownObjectTable::Index mhIndex,
    bool trace)
```

**Preconditions:**
- `isResolvedDirectDispatchGuaranteed()` must return true (ensures safe devirtualization).
- `mhIndex` must be a valid, non-null KOI.

**Transformation:**
1. Retrieves the target `J9Method` from the MethodHandle KOI via
   `fej9()->targetMethodFromMethodHandle()`.
2. Creates a resolved method via `fej9()->createResolvedMethod()`.
3. Separates any `NULLCHK` via `TransformUtil::separateNullCheck()`.
4. Creates a static method symbol for the target.
5. Replaces the `invokeBasic` call node with a **direct static call**.
6. Calls `keepaliveRefinedCallee()` to anchor the refined method reference for GC safety.

### refineMethodHandleLinkTo

**Signature:**
```cpp
bool J9::TransformUtil::refineMethodHandleLinkTo(
    TR::Compilation *comp,
    TR::TreeTop *treetop,
    TR::Node *node,
    TR::KnownObjectTable::Index mnIndex,
    bool trace)
```

Handles all `linkTo*` variants. The `MemberName` KOI is the last argument.

**Call kind mapping:**

| `linkTo*` Method | Dispatch Kind | IL Opcode |
|-----------------|---------------|-----------|
| `linkToStatic` | Static | `call` (direct) |
| `linkToSpecial` | Special | `call` (direct) |
| `linkToVirtual` | Virtual (or direct if final) | `calli` (indirect) or `call` |

`linkToInterface` is **not** handled by this function — calling it with `linkToInterface` would
trigger `TR_ASSERT_FATAL`. Interface calls always go through the `RecognizedCallTransformer`
fallback path instead (see [Fallback linkToVirtual and linkToInterface
Lowering](#fallback-linktovirtual-and-linktointerface-lowering)).

**For virtual calls:**
- Retrieves `vTableSlot` from the `MemberName`'s `vmindex`.
- Computes JIT vTable offset: `fej9()->vTableSlotToVirtualCallOffset(vmSlot)`.
- Generates a VFT (Virtual Function Table) load node: `aloadi <vft> (receiver)`.
- The call becomes `calli <offset> (vft, receiver, args...)`.
- If the resolved method is final, a direct call is used instead.

**For static/special calls:**
- Removes the trailing `MemberName` argument.
- Updates the call's symbol reference to point directly to the target.

---

## RecognizedCallTransformer: Fallback MethodHandle Lowering

`J9RecognizedCallTransformer` handles `invokeBasic`, `linkToStatic`, `linkToSpecial`,
`linkToVirtual`, and `linkToInterface` calls that were **not** refined by the known-object-based
passes — i.e., the fallback path when neither `MethodHandleTransformer` nor `InterpreterEmulator`
was able to determine the MethodHandle or MemberName identity at compile time. It runs as part of
the ILGen optimization strategy (in `ilgenStrategyOpts`, after `MethodHandleTransformer` and
`UnsafeFastPath`), and also appears in the warm/hot/scorching/cold optimization strategies to catch
any remaining unrefined calls after the inlining phases.

All four handlers are guarded by `#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)`.

### Fallback invokeBasic Lowering

**Function:** `process_java_lang_invoke_MethodHandle_invokeBasic(TreeTop *treetop, Node *node)`

When `invokeBasic` reaches this pass unrefined, the target `J9Method` is extracted at runtime by
loading the chain `mh.form.vmentry.vmtarget`:

```
mh  →  MethodHandle.form  (LambdaForm)
    →  LambdaForm.vmentry (MemberName)
    →  MemberName.vmtarget (J9Method*)
```

**If `enableJitDispatchJ9Method()` is true** (simpler path): the call is retargeted to
`dispatchJ9Method`, with the extracted `J9Method*` prepended as the first argument.

**Otherwise** (diamond path): delegates to `processVMInternalNativeFunction` (see below).

### Fallback linkToStatic / linkToSpecial Lowering

**Function:** `process_java_lang_invoke_MethodHandle_linkToStaticSpecial(TreeTop *treetop, Node
*node)`

The target `J9Method` is taken directly from `MemberName.vmtarget` (the trailing argument). The
`MemberName` argument is then removed since the dispatch goes directly to the resolved address.

For `linkToSpecial`, a `NULLCHK` on the receiver (first argument) is also inserted before the call.

**If `enableJitDispatchJ9Method()` is true**: retargets to `dispatchJ9Method` with the `J9Method*`
prepended.

**Otherwise**: delegates to `processVMInternalNativeFunction`.

### processVMInternalNativeFunction: The Diamond Transform

**Function:** `processVMInternalNativeFunction(TreeTop *treetop, Node *node, Node *vmTargetNode,
list<SymbolReference*> *argsList, Node *inlCallNode)`

This function handles the case where `enableJitDispatchJ9Method()` is false. It generates a
**control-flow diamond** that tests whether the target `J9Method` is already JIT-compiled:

```
                [store args to temps]
                        |
         [load J9Method->extra field]
                        |
         [test extra & J9_STARTPC_NOT_TRANSLATED]
                      /    \
                   != 0    == 0
           (compiled)      (not compiled)
               |                  |
    [compute JIT entry point]   [fall back to original]
    [from linkage info at -4]   [inlCallNode: re-invoke]
    [dispatchComputedStaticCall]
               |                  |
               \__________________/
                        |
                   [merge point]
```

- **Compiled branch**: reads the JIT entry point from `J9Method.extra` (adjusting via the linkage
  info word at offset -4 for non-x86 platforms), then calls
  `JITHelpers.dispatchComputedStaticCall()` — a computed static call that jumps directly to the JIT
  entry.
- **Interpreted branch**: re-issues the original call node (`inlCallNode`), which will trap into the
  interpreter stub.

For `linkToSpecial` in the compiled branch, a `NULLCHK` on the receiver is also inserted.

### Fallback linkToVirtual and linkToInterface Lowering

Both are converted into computed calls through `JITHelpers.dispatchVirtual()` via the shared helper
`makeIntoDispatchVirtualCall`.

**`linkToVirtual`** (`process_java_lang_invoke_MethodHandle_linkToVirtual`):
1. Loads `MemberName.vmindex` (a long) as the interpreter VFT offset.
2. Loads the VFT from the receiver: `aloadi <vft> (receiver)`.
3. A `NULLCHK` is inserted on the VFT load to null-check the receiver.
4. Calls `makeIntoDispatchVirtualCall`.

**`linkToInterface`** (`process_java_lang_invoke_MethodHandle_linkToInterface`):
1. Calls the VM helper `lookupDynamicPublicInterfaceMethod(vft, memberName)` at runtime to find the
   iTable index. This helper locates the declaring interface and itable slot from the `MemberName`,
   and throws `IllegalAccessError` if the found method is non-public.
2. The returned value is used as the VFT offset by `makeIntoDispatchVirtualCall`.

> **Note:** `linkToInterface` always goes through this fallback — `refineMethodHandleLinkTo` does
> not support interface dispatch at all (it asserts on unrecognised `linkTo*` methods). The reason,
> as noted in a code comment, is that the compiler currently has no representation for resolved
> interface calls without constant pool entries.

**`makeIntoDispatchVirtualCall`**:
- Reconstructs the call node as a computed indirect call to `JITHelpers.dispatchVirtual()V`.
- Prepends two extra arguments: the JIT method entry point loaded from the VFT slot
  (`vft[sizeof(J9Class) - vmindex]`), and the JIT VFT offset.
- Removes the trailing `MemberName` argument.

---

## Inliner Support

### InterpreterEmulator and Callee Refinement


During the inliner's `estimateCodeSize` phase, the inliner must determine call targets for
MethodHandle-related methods. There are two mechanisms, and the choice between them depends on the
callee:

- **InterpreterEmulator with state information**: Used for **LambdaForm-generated methods**. The
  emulator symbolically executes the bytecodes while tracking known object operands on a modelled
  stack, performing transformations analogous to what `MethodHandleTransformer` does — refining
  `invokeBasic` and `linkTo*` calls based on propagated known object information. This is the
  **only** mechanism through which the target of a MethodHandle invocation gets identified for
  inlining in the ideal case. Peeking ILGen is never used for LambdaForm-generated methods.

- **Peeking ILGen**: Used for **non-LambdaForm methods** that contain `invokedynamic` or
  `invokehandle` bytecodes. Peeking ILGen runs the full ILGen pipeline (including
  `MethodHandleTransformer`) on the callee to propagate known object information from the caller.
  The generated IL is discarded after analysis; it serves only to let the inliner see through the
  MethodHandle dispatch chain in the callee.

The `InterpreterEmulator` maintains an **operand stack** of typed operands, including:

| Operand Type | Description |
|-------------|-------------|
| `KnownObjOperand` | A compile-time known object (KOI index) |
| `MutableCallsiteTargetOperand` | Wraps both a MethodHandle KOI and a MutableCallSite KOI |
| `ObjectOperand` | An object of known class but unknown identity |
| `PreexistentObjectOperand` | Preexistent object |

**Callee refinement for MethodHandle methods** (`refineResolvedCalleeForInvokestatic`):

- **`ILGenMacros_invokeExact*`**: Extracts the top-of-stack MethodHandle operand and creates an
  archetype specimen via `fej9()->createMethodHandleArchetypeSpecimen()`.
- **`DirectHandle_directCall` / `VirtualHandle_virtualCall`**: Uses
  `fej9()->methodOfDirectOrVirtualHandle()` to get the actual `vmtarget` and `vmSlot` from the
  MethodHandle, then creates a resolved method with the appropriate vtable slot.
- **`linkToStatic` / `linkToSpecial` / `linkToVirtual`**: Retrieves the `MemberName` from the top
  of the operand stack, calls `fej9()->getMemberNameMethodInfo()`, creates a resolved method, and
  pops the `MemberName` from the stack.
- **`linkToVirtual`**: Additionally validates `refKind == MH_REF_INVOKEVIRTUAL` and sets the vTableSlot.

**`getReturnValue()` for key MethodHandle methods:**

- **`DelegatingMethodHandle.getTarget()`**: Returns the delegated target MethodHandle's KOI via
  `fej9()->delegatingMethodHandleTarget()`.
- **`MutableCallSite.getTarget()` / `Invokers.getCallSiteTarget()`**: Loads the current `target`
  field of the `MutableCallSite` at compile time (if known), returning a
  `MutableCallsiteTargetOperand` that carries both the MH and MCS KOIs.
- **`DirectMethodHandle.internalMemberName()`**: Returns the `member` field's KOI via
  `fej9()->getMemberNameFieldKnotIndexFromMethodHandleKnotIndex()`.
- **`Invokers.checkVarHandleGenericType()`**: Performs a compile-time table lookup on the VarHandle
  to return the appropriate MethodHandle KOI.

### TR_J9MethodHandleCallSite

`TR_J9MethodHandleCallSite::findCallSiteTarget()` handles inlining of MethodHandle thunk archetype
methods (primarily for the legacy OpenJ9 MH path). It adds the target with `TR_NoGuard`
(unconditional inline), since the method is statically known.

### TR_J9MutableCallSite and MutableCallSite Guards

`TR_J9MutableCallSite::findCallSiteTarget()` handles inlining through `MutableCallSite` targets,
which can be reassigned at runtime.

**Pattern detection** (`isMutableCallSiteTargetInvokeExact`):
- Detects `mcs.target.invokeExact(...)` or `mcs.getTarget().invokeExact(...)`.
- Can be disabled via `TR_DisableMutableCallSiteGuards`.
- Requires a resolved, non-null receiver.

**Guard creation:**
1. Validates the `MutableCallSite` is a known object.
2. Retrieves the current target MethodHandle (the epoch).
3. Creates a `TR_MutableCallSiteTargetGuard` with:
   - `_mutableCallSiteObject`: the MCS instance KOI
   - `_mutableCallSiteEpoch`: the current target MethodHandle KOI
4. For OpenJDK MHs: refines the callee to the target via `invokeBasic`.
5. For legacy MHs: uses the archetype specimen.

The guard is deoptimized (patched) if the `MutableCallSite`'s target is changed via `setTarget()`,
ensuring correctness while enabling aggressive inlining.

---

## GlobalValuePropagation: Late MethodHandle Refinement

`GlobalValuePropagation` (GVP) runs later in the optimization pipeline, after inlining. It can
discover known object information that was not available to `MethodHandleTransformer` or
`InterpreterEmulator` — for example, when constant propagation, type narrowing, or other
optimizations expose the identity of a MethodHandle or MemberName that was opaque during earlier
passes. This makes GVP an important supplementary mechanism for MethodHandle optimization.

### How GVP Refines MethodHandle Calls

In `constrainRecognizedMethod()`, GVP handles the following recognized methods:

- **`invokeBasic`**: When the first argument (the MethodHandle) has a `VPKnownObject` constraint,
  GVP calls `J9::TransformUtil::refineMethodHandleInvokeBasic()` — the same utility used by
  `MethodHandleTransformer` — to replace the `invokeBasic` call with a direct static call to the
  target method.

- **`linkToStatic` / `linkToSpecial` / `linkToVirtual`**: When the last argument (the `MemberName`)
  has a `VPKnownObject` constraint, GVP calls `J9::TransformUtil::refineMethodHandleLinkTo()` to
  refine the call.

### Delayed Inlining

After refining an `invokeBasic` or `linkTo*` call, GVP queues the refined call for **delayed
inlining** via `processRefinedMethodHandleINLCall()`. This creates `TR_PrexArgInfo` entries with
known object information for the refined call's arguments, and adds them to the
`_refinedMethodHandleINLMethodsToInline` list. During `doDelayedTransformations()`, GVP invokes
`TR_InlineCall` to inline each refined target, allowing the final inlined body to benefit from all
preceding optimizations.

This delayed inlining mechanism is what allows GVP to unlock performance improvements for
MethodHandle calls that could not be optimized during the main inlining phase.

---

## Code Generation: linkTo* Tree Lowering

During the code generation tree-lowering phase, the compiler inserts additional IL trees
immediately before certain `linkTo*` call nodes. This is necessary because `linkTo*` methods are
**signature-polymorphic INL (interpreter native library) calls**: when the JIT frame returns to the
interpreter (e.g., during deoptimization, exception handling, or because the callee itself is
interpreted), the VM must be able to locate the call arguments on the JIT stack. The mechanisms
below communicate the necessary information via fields of the current `J9VMThread`.

### Storing the Argument Slot Count in vmThread.tempSlot

For all four resolving `linkTo*` methods — `linkToStatic`, `linkToSpecial`, `linkToVirtual`, and
`linkToInterface` — a store to `vmThread->tempSlot` is prepended to the call tree:

```
lstore/istore vmThread.tempSlot
  lconst/iconst <numParameterStackSlots>
call linkTo*(...)
```

`numParameterStackSlots` is the number of stack slots occupied by all parameters of the `linkTo*`
call (obtained from `getNumParameterSlots()` on the resolved method symbol). The VM uses this value
to find the base of the argument list on the JIT execution stack, which is needed to:
- Locate the receiver for virtual/interface dispatch.
- Walk the stack frame correctly during deoptimization.

Note that `invokeBasic` does **not** require this treatment: for `invokeBasic` calls, the VM can
always find the argument count from the JIT body's metadata, so no `tempSlot` store is needed.

### Communicating Appendix Removal via vmThread.floatTemp1

For `linkToStatic` and `linkToSpecial`, an additional store to `vmThread->floatTemp1` is also
prepended:

```
lstore/istore vmThread.floatTemp1
  lconst/iconst <numArgs>
lstore/istore vmThread.tempSlot
  lconst/iconst <numParameterStackSlots>
call linkToStatic/linkToSpecial(...)
```

The value stored in `floatTemp1` (the name is misleading — it is always an integer or long
constant) depends on whether the call was generated for an **unresolved** `invokedynamic` or
`invokehandle` bytecode:

**Unresolved call site (dummy `TR_ResolvedMethod`):**

When an `invokedynamic` or `invokehandle` bytecode is unresolved at compile time, the compiler
cannot determine whether the invoke cache's appendix slot is null or non-null. The IL is generated
with a call to a dummy `linkToStatic` symbol (marked via `setDummyResolvedMethod()`), and both an
appendix and a `MemberName` are loaded from the invoke cache array and passed as trailing arguments.

At runtime, the VM resolves the call site and discovers the actual appendix value. If the appendix
turns out to be null, the VM must **remove** it from the stack before transferring to the target
(since the resolved target method does not expect a null appendix). To do this, the VM needs to know
how many slots to skip over. The value stored in `floatTemp1` is:

```
numArgs = numParameterStackSlots - 1
```

This equals the number of stack slots for the ROM method signature (including the receiver for
`invokehandle`) plus the appendix slot, minus the trailing `MemberName` slot. The VM uses this count
to locate and remove the appendix if needed.

**Resolved call site (real `TR_ResolvedMethod`):**

For a fully resolved call, the compiler knows the appendix at compile time and never pushes a null
one. No stack adjustment is needed, so `floatTemp1` is set to `-1` as a sentinel:

```
floatTemp1 = -1   // no stack adjustment required
```

Although `linkToSpecial` shares the same VM handling mechanism as `linkToStatic`, stack adjustments
for it are never actually needed (its call sites are always fully resolved in practice), so
`floatTemp1` will always be `-1` for `linkToSpecial`.

**Summary table:**

| Call | `tempSlot` | `floatTemp1` |
|------|-----------|-------------|
| `linkToVirtual` | `numParameterStackSlots` | not written |
| `linkToInterface` | `numParameterStackSlots` | not written |
| `linkToStatic` (resolved) | `numParameterStackSlots` | `-1` |
| `linkToStatic` (unresolved / dummy) | `numParameterStackSlots` | `numParameterStackSlots - 1` |
| `linkToSpecial` (always resolved) | `numParameterStackSlots` | `-1` |
| `invokeBasic` | not written | not written |

### linkToNative: Reserving Stack Space for the Appendix

`linkToNative` receives special treatment that is distinct from the other `linkTo*` methods. It does
**not** receive a `tempSlot` store (the VM has an alternative mechanism to determine the argument
count for native calls). Instead, the compiler adds a dummy null argument to the call node itself:

```cpp
TR::Node *dummyNull = TR::Node::aconst(node, 0);
node->addChildren(&dummyNull, 1);
```

**Why:** When a JIT-compiled frame calls `linkToNative`, the VM's interpreter stub will push one
extra argument — the appendix object — before rearranging the native call arguments. If no space has
been reserved for the appendix on the JIT stack, this push would shift the stack pointer by one slot
relative to what the JIT frame expects, causing incorrect argument addressing and a corrupt stack
pointer on return or during stack walks. The dummy null child reserves exactly one extra stack slot
so the interpreter's appendix push lands in that reserved slot without disturbing the rest of the
frame.

---

## VarHandle Support

### VarHandle Architecture

`VarHandle`
provides typed access to variables (fields, array elements, off-heap memory) with a range of memory
ordering guarantees (plain, opaque, acquire/release, volatile). Each access mode corresponds to a
distinct method on `VarHandle`.

Under the OpenJDK MethodHandle implementation, a `VarHandle` instance holds a **method handle
table** (`methodHandleTable` in JDK 21+, or `typesAndInvokers.methodHandle_table` in JDK ≤ 17).
Each access mode maps to an index in this table, and the table entry is a `MethodHandle` whose
LambdaForm eventually invokes a concrete **VarHandle operation method** — a JCL class like
`VarHandleX$FieldInstanceReadOnlyOrReadWrite` — that performs the actual `Unsafe` memory access with
the correct ordering semantics.

A VarHandle access call like:
```java
vh.getVolatile(obj)
```
goes through the following JCL dispatch chain at runtime:

1. `VarHandle.getVolatile` dispatches through `Invokers.checkVarHandleGenericType(vh,
   accessDescriptor)`, which returns the appropriate `MethodHandle` from the VarHandle's method
   handle table.
2. `invokeBasic(mh, vh, obj)` is called on the selected MethodHandle.
3. The MethodHandle's LambdaForm body eventually calls the concrete operation method, e.g., a method
   recognized as `java_lang_invoke_VarHandleX_FieldInstanceReadOnlyOrReadWrite_method`.
4. The operation method performs `Unsafe.getObjectVolatile(base, offset)` to read the field.

The JIT must inline through all these layers to produce efficient code. The key optimizations
involved are `checkVarHandleGenericType` folding (by `MethodHandleTransformer` and
`InterpreterEmulator`) and `Unsafe` call lowering by `UnsafeFastPath`.

### checkVarHandleGenericType Folding (OpenJDK MH)

This is covered in the [MethodHandleTransformer
Optimization](#methodhandletransformer-optimization) section above (see [checkVarHandleGenericType
Folding](#checkvarhandlegenerictype-folding)), but it is worth summarising its role in the VarHandle
context here.

`Invokers.checkVarHandleGenericType(vh, accessDescriptor)` is the gateway from the generic
`VarHandle.getVolatile(...)` call to the specific MethodHandle stored in the table. The
`MethodHandleTransformer` can fold this call at compile time if both the `VarHandle` and
`AccessDescriptor` are known objects:

1. The access mode index is computed from the `AccessDescriptor` KOI.
2. The method handle table is loaded from the `VarHandle` object (`methodHandleTable` for JDK 21+,
   or `typesAndInvokers.methodHandle_table` for JDK ≤ 17).
3. The table entry at the access mode index is loaded directly, and the call is replaced with an
   annotated `aload` of the known MethodHandle object.

The `InterpreterEmulator` performs the equivalent folding during peeking, so that the inliner can
immediately see a known MethodHandle as the callee of the subsequent `invokeBasic`, and thus refine
it to the concrete operation method.

Once the concrete operation method is the inlining target, the normal MethodHandle inlining pipeline
(invokeBasic refinement → direct call → inliner) applies.

### UnsafeFastPath for VarHandle Operation Methods

After the concrete VarHandle operation method (e.g.,
`VarHandleX_FieldInstanceReadOnlyOrReadWrite_method`) is inlined, its body contains calls to
`Unsafe.get*` / `Unsafe.put*` / `Unsafe.compareAndSet*` methods. The `TR_UnsafeFastPath`
optimization converts these into direct IL load/store/CAS nodes with the correct memory ordering.

**Recognized operation methods (under `J9VM_OPT_OPENJDK_METHODHANDLE`):**

| Recognized Method | Description |
|------------------|-------------|
| `java_lang_invoke_VarHandleX_Array_method` | Array element access |
| `java_lang_invoke_VarHandleX_FieldInstanceReadOnlyOrReadWrite_method` | Instance field access |
| `java_lang_invoke_VarHandleX_FieldStaticReadOnlyOrReadWrite_method` | Static field access |
| `java_lang_invoke_VarHandleByteArrayAsX_ArrayHandle_method` | Byte-array-as-X access |

(`VarHandleByteArrayAsX_ByteBufferHandle_method` is explicitly excluded from fast-path due to
complications with byte buffer utility methods.)

**How `UnsafeFastPath` processes VarHandle Unsafe calls:**

1. **Caller identification**: `getVarHandleAccessMethodFromInlinedCallStack()` walks the inlined
   call stack to find a VarHandle operation method in the caller chain. This handles the case where
   the Unsafe call is several levels deep in an inlined body.

2. **Access classification**: Based on the recognized operation method, the pass determines:
   - **Array vs. field**: whether the access is to an array element or object field, affecting
     address computation.
   - **Static vs. instance**: static field accesses require loading `ramStatics` from the class and
     masking the low-tag bit from the offset.
   - **Memory ordering**: derived from the Unsafe method name (e.g., `getObjectVolatile` →
     `TR::Symbol::MemoryOrdering::Volatile`, acquire/release variants → `AcquireRelease`, opaque →
     `Opaque`).

3. **Unsafe call lowering**: The recognized `Unsafe.get*` / `Unsafe.put*` call is replaced with an
   IL `iloadi` / `istorei` (or equivalent) with the appropriate `MemoryOrdering` annotation on the
   symbol reference. This causes the code generator to emit the correct memory barriers and
   instruction sequences.

4. **Atomic operations**: For `Unsafe.compareAndSet*`, `Unsafe.getAndSet*`, `Unsafe.getAndAdd*`
   inside VarHandle operation methods:
   - CAS calls on non-static, non-arraylet fields are marked with
     `setIsSafeForCGToFastPathUnsafeCall(true)`, allowing the code generator to inline them as
     native atomic instructions.
   - `getAndSet` / `getAndAdd` calls are replaced with equivalent atomic intrinsic symbol references
     (`atomicSwapSymbol`, `atomicFetchAndAddSymbol`) when the target platform supports them.

5. **Off-heap array support**: When sparse heap allocation (`J9VM_GC_SPARSE_HEAP_ALLOCATION`) is
   enabled and the access is to an array element, the base object pointer is rewritten to use
   `generateDataAddrLoadTrees()` to obtain the true off-heap data address before the atomic
   operation.

**Memory ordering values:**

| Unsafe method suffix | MemoryOrdering |
|---------------------|---------------|
| (plain, no suffix) | None (relaxed) |
| `Volatile` | `Volatile` |
| `Acquire` / `Release` | `AcquireRelease` |
| `Opaque` | `Opaque` |

### VarHandleTransformer (Legacy OpenJ9 MH Only)

> **Note:** `TR_VarHandleTransformer` is part of the **legacy OpenJ9 MethodHandle** implementation
> (Java 8/11, without `J9VM_OPT_OPENJDK_METHODHANDLE`). It is **not** used in the OpenJDK
> MethodHandle path. Under OpenJDK MH, VarHandle dispatch is handled entirely through the JCL's
> LambdaForm machinery, and the compiler optimizes it via `MethodHandleTransformer` (for
> `checkVarHandleGenericType` folding) and `UnsafeFastPath` (for Unsafe call lowering). This section
> is included for completeness.

In the legacy path, `TR_VarHandleTransformer::perform()` intercepts calls to VarHandle access
methods (e.g., `VarHandle.get`, `VarHandle.getVolatile`, etc.) directly in the IL and rewrites them
as explicit `MethodHandle.invokeExact` calls, materializing the method handle table lookup in IL.
This was necessary because the old JCL did not use LambdaForm-based dispatch for VarHandle.

Under `J9VM_OPT_OPENJDK_METHODHANDLE`, the `VarHandleTransformer` is not invoked for VarHandle
access method calls, as those calls never appear directly in the IL — the entire dispatch chain goes
through the JCL's LambdaForm-generated code.

---

## Recognized Methods

The compiler maintains a table of **recognized methods** for MethodHandle-related JCL methods.
**MethodHandle core methods:**

| Recognized Method | Description |
|------------------|-------------|
| `java_lang_invoke_MethodHandle_invokeExact` | Signature-polymorphic exact invocation |
| `java_lang_invoke_MethodHandle_invokeBasic` | Low-level dispatch intrinsic |
| `java_lang_invoke_MethodHandle_invoke` | Signature-polymorphic inexact invocation |
| `java_lang_invoke_MethodHandle_linkToStatic` | Static/special dispatch via MemberName |
| `java_lang_invoke_MethodHandle_linkToVirtual` | Virtual dispatch via MemberName |
| `java_lang_invoke_MethodHandle_linkToSpecial` | Special dispatch via MemberName |
| `java_lang_invoke_MethodHandle_linkToInterface` | Interface dispatch via MemberName |
| `java_lang_invoke_MethodHandle_linkToNative` | Native dispatch via MemberName |
| `java_lang_invoke_MethodHandle_invokeExactTargetAddress` | Returns J9Method* as long |
| `java_lang_invoke_MethodHandle_asType` | Returns adapted MH matching target MethodType |

**Invokers helper methods:**

| Recognized Method | Description |
|------------------|-------------|
| `java_lang_invoke_Invokers_checkExactType` | Checks MH type matches expected |
| `java_lang_invoke_Invokers_checkCustomized` | Checks MH customization state |
| `java_lang_invoke_Invokers_checkVarHandleGenericType` | Selects MH from VarHandle table |
| `java_lang_invoke_Invokers_getCallSiteTarget` | Gets MutableCallSite's current target |

**DirectMethodHandle accessors:**

| Recognized Method | Description |
|------------------|-------------|
| `java_lang_invoke_DirectMethodHandle_internalMemberName` | Returns member field KOI |
| `java_lang_invoke_DirectMethodHandle_internalMemberNameEnsureInit` | Returns member after init |
| `java_lang_invoke_DirectMethodHandle_constructorMethod` | Returns constructor MemberName |
| `java_lang_invoke_DirectMethodHandle_Accessor_checkCast` | Cast check for accessor |

**DelegatingMethodHandle:**

| Recognized Method | Description |
|------------------|-------------|
| `java_lang_invoke_DelegatingMethodHandle_getTarget` | Returns the delegated MH |

**VarHandle operation methods (OpenJDK MH — recognized for `UnsafeFastPath` and always-inline
decisions):**

| Recognized Method | Description |
|------------------|-------------|
| `java_lang_invoke_VarHandleX_Array_method` | Array element access operation |
| `java_lang_invoke_VarHandleX_FieldInstanceReadOnlyOrReadWrite_method` | Instance field access operation |
| `java_lang_invoke_VarHandleX_FieldStaticReadOnlyOrReadWrite_method` | Static field access operation |
| `java_lang_invoke_VarHandleByteArrayAsX_ArrayHandle_method` | Byte array viewed as another type |
| `java_lang_invoke_VarHandleByteArrayAsX_ByteBufferHandle_method` | ByteBuffer-backed VarHandle (excluded from `UnsafeFastPath`) |

These are the concrete per-VarHandle-kind operation methods generated by the JCL. They are always
considered worth inlining (`isVarHandleOperationMethod()` returns `true`), and `UnsafeFastPath` uses
them to identify Unsafe calls that can be lowered to direct memory operations.

---

## Compilation Restrictions

### AOT Compilation

`invokedynamic` and `invokehandle` bytecodes are **not supported** in standard AOT compilation. This
is because:
- The resolved invoke cache arrays contain heap object addresses that are not relocatable without
  special handling.
- The `MemberName.vmtarget` pointer must be validated across JVM restarts.

AOT support is possible only when **both** of the following options are active:
- `TR_EnableMHRelocatableCompile`
- `TR_UseSymbolValidationManager` (SVM)

Under these conditions, the compiler uses the Symbol Validation Manager to record and validate all
MethodHandle-related symbols, enabling safe relocation.

### Full Speed Debug (FSD)

FSD mode disables many optimizations to ensure debuggability. `invokedynamic` and `invokehandle` are
not supported in FSD compilation, **except** when the compilation is a peeking pass. Peeking is
allowed because it is an ephemeral, read-only exploration of a callee's IL for inlining decisions,
not a final compiled body.

---

## End-to-End Optimization Pipeline

The following describes the overall ordering of compiler phases relevant to MethodHandle optimization.
Understanding this ordering is essential for reasoning about which pass handles a given call and why.

```
1. IL Generation (Walker.cpp)
   - Converts invokedynamic/invokehandle bytecodes into IL trees
   - Loads invoke cache (MemberName + appendix), generates calls

2. ILGen Optimization Strategy (ilgenStrategyOpts)
   a. MethodHandleTransformer
      - Runs ONLY if the method being compiled is a LambdaForm-generated method
      - Propagates known objects, refines invokeBasic/linkTo*, eliminates checks
   b. UnsafeFastPath
      - Lowers Unsafe calls inside VarHandle operation methods
   c. RecognizedCallTransformer
      - Fallback: lowers any invokeBasic/linkTo* that were NOT refined above

3. Inliner (estimateCodeSize phase)
   - For each callee being considered for inlining:
     a. LambdaForm-generated callees: InterpreterEmulator with state info
        - Symbolically executes bytecodes, propagates known objects
        - Refines invokeBasic/linkTo* to find the ultimate call target
        - This is the primary mechanism for resolving MH dispatch chains
     b. Non-LambdaForm callees with MH bytecodes: Peeking ILGen
        - Runs ILGen + MethodHandleTransformer on the callee (then discards IL)
        - Propagates object info from caller into the callee's MH calls

4. Inliner (inlining transformation phase)
   - Inlines selected callees into the root method
   - methodHandleInvokeInliningGroup runs on inlined bodies:
     - MethodHandleTransformer (on newly inlined LambdaForm code)
     - targetedInlining (to continue inlining deeper MH chains)
     - This group can repeat recursively for multi-level MH chains
   - requestAdditionalOptimizations() triggers further rounds if
     remaining invokeBasic/invokeExact calls are detected

5. UnsafeFastPath (post-inlining)
   - Lowers Unsafe.get*/put*/CAS calls inside inlined VarHandle operation methods

6. RecognizedCallTransformer (in warm/hot/scorching strategies)
   - Catches any remaining unrefined invokeBasic/linkTo* calls

7. GlobalValuePropagation (GVP)
   - Can discover new known objects via constraint propagation
   - Refines invokeBasic/linkTo* calls that earlier passes missed
   - Performs delayed inlining of refined targets
   - See [GlobalValuePropagation: Late MethodHandle
     Refinement](#globalvaluepropagation-late-methodhandle-refinement)

8. Code Generation (lowerTreeIfNeeded)
   - Inserts vmThread.tempSlot / floatTemp1 stores for linkTo* calls
   - Adds dummy null argument for linkToNative
```

### MethodHandle Example

The following traces the optimization of `mh.invokeExact(arg0, arg1)` where `mh` resolves to a
`DirectMethodHandle` targeting a static method `Foo.bar(Object, Object)`:

```
[Source] mh.invokeExact(arg0, arg1)
     |
     | (javac emits invokehandle bytecode with call-site type descriptor)
     v
[1. IL Generation - Walker.cpp genInvokeHandle()]
  - Loads invoke cache: appendix (MethodType for type check), MemberName (target)
  - Generates: call <LambdaForm adapter>(mh, arg0, arg1, appendix)
  - Sets hasMethodHandleInvokes = true
     |
     v
[3. Inliner - estimateCodeSize]
  - The LambdaForm adapter is a candidate for inlining
  - InterpreterEmulator runs with state info on the adapter's bytecodes:
    - mh (parameter 0) carries known object info: KOI #5 (DirectMethodHandle)
    - checkExactType(mh, expectedType): known objects -> folded
    - checkCustomized(mh): known object -> folded
    - invokeBasic(mh, arg0, arg1): extracts target from KOI #5
      -> refines to Foo.bar() as the inlining target
     |
     v
[4. Inliner - inlining transformation]
  - Inlines the LambdaForm adapter into the root method
  - methodHandleInvokeInliningGroup runs on the inlined body:
    - MethodHandleTransformer propagates known objects:
      - checkExactType: types match -> eliminated (PassThrough)
      - checkCustomized: mh known -> eliminated (PassThrough)
      - invokeBasic(mh, arg0, arg1): refines to direct call to Foo.bar()
        (via refineMethodHandleInvokeBasic in J9TransformUtil)
  - Foo.bar() is then inlined if profitable
     |
     v
[Final IL]
  // Direct call or inlined body - zero MethodHandle overhead
  call Foo.bar(arg0, arg1)
```

### VarHandle Example

For a **VarHandle** operation like `vh.getVolatile(obj)` where `vh` is a known instance-field
`VarHandle`:

```
[Source] vh.getVolatile(obj)
     |
     | (the JCL body of VarHandle.getVolatile calls:
     |    Invokers.checkVarHandleGenericType(vh, accessDescriptor) -> mh
     |    mh.invokeBasic(vh, obj))
     v
[3. Inliner - estimateCodeSize]
  - InterpreterEmulator runs on the LambdaForm adapter body with state info:
    - vh is known object (KOI #3), accessDescriptor is known (KOI #4)
    - Folds checkVarHandleGenericType: compile-time table lookup
      -> returns known MethodHandle KOI #7 (the GET_VOLATILE entry)
    - Refines invokeBasic(KOI#7, vh, obj) -> direct call to
        VarHandleX_FieldInstanceReadOnlyOrReadWrite_method
     |
     v
[4. Inliner - inlining transformation]
  - Inlines the LambdaForm adapter, then the VarHandle operation method
  - methodHandleInvokeInliningGroup runs:
    - MethodHandleTransformer folds checkVarHandleGenericType, refines invokeBasic
  - VarHandle operation method body: Unsafe.getObjectVolatile(base, offset)
     |
     v
[5. UnsafeFastPath]
  - Detects Unsafe.getObjectVolatile inside inlined VarHandle operation method
    (via getVarHandleAccessMethodFromInlinedCallStack)
  - Access is non-static, non-array -> instance field load
  - Memory ordering: Volatile
  - Replaces call with: iloadi/aloadi [memOrdering=Volatile] (base + offset)
     |
     v
[8. Code Generation]
  - Emits field load with full volatile memory barriers
  // Zero VarHandle dispatch overhead in final code
```

---

## Real-World Example and JIT Walkthrough

The following example demonstrates a pattern common in high-performance frameworks (serialization
libraries, ORM tools, `java.util.concurrent` internals): a virtual method accessed through a `static
final` MethodHandle in a hot path.

```java
import java.lang.invoke.*;

public class Processor {
    // Storing in a static final field lets the JIT treat this as a
    // compile-time known object once the class is initialized.
    private static final MethodHandle STRIP_MH;

    static {
        try {
            // findVirtual produces a DirectMethodHandle whose type is
            // (String)String — the receiver is the first (and only) parameter.
            STRIP_MH = MethodHandles.publicLookup().findVirtual(
                String.class, "strip",
                MethodType.methodType(String.class));
        } catch (ReflectiveOperationException e) {
            throw new AssertionError(e);
        }
    }

    /** Called in a tight loop; JIT-compiled after warm-up. */
    public String process(String input) throws Throwable {
        // javac emits an invokehandle bytecode here.
        // After JIT optimization this incurs zero MethodHandle overhead.
        return (String) STRIP_MH.invokeExact(input);
    }
}
```

MethodHandles are the standard Java mechanism for reflective dispatch with near-direct-call
performance after JIT warm-up. They also underpin several invisible Java features:
- **Lambda expressions**: `LambdaMetafactory` links lambdas (`Runnable r = () -> ...`) via
  `invokedynamic` and MethodHandles.
- **String concatenation / pattern matching**: the JDK uses `invokedynamic` bootstrap methods that
  produce MethodHandles.
- **`VarHandle` memory access**: `VarHandle.get()`, `compareAndSet()`, etc. are dispatched through a
  per-VarHandle MethodHandle table (see [VarHandle Support](#varhandle-support)).

### JIT Pipeline Walkthrough

The following traces `process(input)` through the compiler's MethodHandle pipeline, using the example
above to illustrate each phase described in detail later in this document.

**1. IL Generation** (see [IL Generation Phase](#il-generation-phase))

`genInvokeHandle` processes the `invokehandle` bytecode for `STRIP_MH.invokeExact(input)`. `STRIP_MH`
is a resolved, non-null static field, so the compiler loads the invoke cache and generates:

```
call <LambdaForm adapter method>
  aload input          // receiver String (the sole argument)
  aloadi <appendix>    // MethodType "(String)String" from invoke cache[1]
```

The `MemberName` (pointing to `String.strip`) and the appendix (`MethodType`) are extracted from the
invoke cache array. If the call site were unresolved, the IL would call `linkToStatic(input,
appendix, memberName)` instead.

**2. LambdaForm Adapter** (see [LambdaForm Method Compilation](#lambdaform-method-compilation))

The call target is a JCL-generated LambdaForm adapter for a `(String)String` virtual `invokeExact`.
Its effective bytecode body is:

```java
// Generated by InvokerBytecodeGenerator at runtime:
String invoke_MT(MethodHandle mh, String s, MethodType appendix_mt) {
    Invokers.checkExactType(mh, appendix_mt); // exact-type guard
    Invokers.checkCustomized(mh);             // customization hook
    return (String) mh.invokeBasic(s);        // dispatch through DirectMethodHandle
}
```

**3. Inliner: estimateCodeSize** (see [Inliner Support](#inliner-support))

The inliner considers the LambdaForm adapter as an inlining candidate. Because it is a
LambdaForm-generated method, `InterpreterEmulator` runs **with state information** to symbolically
execute its bytecodes:

- `STRIP_MH` is a `static final` field, so the JIT's Known Object Table holds a reference to the
  concrete `DirectMethodHandle` instance at compile time (KOI #N).
- `mh` (parameter 0) carries KOI #N from the caller.
- `checkExactType(mh, appendix_mt)`: both arguments are known objects — folded.
- `checkCustomized(mh)`: `mh` is known — folded.
- `invokeBasic(mh, s)`: `mh` = KOI #N → extracts the target method from the `DirectMethodHandle`
  → resolves `String.strip` as the callee for inlining.

This is the critical step: `InterpreterEmulator` is the primary mechanism for identifying the
ultimate call target through the MethodHandle dispatch chain.

**4. Inliner: Inlining Transformation** (see [MethodHandleTransformer
Optimization](#methodhandletransformer-optimization))

The LambdaForm adapter is inlined into `process()`. The `methodHandleInvokeInliningGroup` then runs
on the inlined body, which includes `MethodHandleTransformer`:

- `checkExactType(mh, appendix_mt)`: types match → eliminated (`PassThrough`).
- `checkCustomized(mh)`: `mh` known → eliminated (`PassThrough`).
- `invokeBasic(mh, s)`: `mh` = KOI #N → `targetMethodFromMethodHandle(#N)` returns the `J9Method`
  for `String.strip` → the `invokeBasic` call is replaced with a **direct static call** (see
  [J9TransformUtil: Call Refinement](#j9transformutil-call-refinement)).

`String.strip` is then inlined if the inlining budget allows.

**5. Fallback: RecognizedCallTransformer** (see [RecognizedCallTransformer: Fallback
MethodHandle Lowering](#recognizedcalltransformer-fallback-methodhandle-lowering))

If the KOI for `STRIP_MH` were **not** available — for instance if the handle were stored in a
non-final instance field whose value could not be tracked at compile time — neither
`InterpreterEmulator` nor `MethodHandleTransformer` would be able to refine the `invokeBasic` call.
`RecognizedCallTransformer` would then handle it by reading the target `J9Method` at runtime via
`mh.form.vmentry.vmtarget` and generating a compiled/interpreted diamond:

```
    [test J9Method.extra & J9_STARTPC_NOT_TRANSLATED]
              /                          \
       compiled                      not compiled
  dispatchComputedStaticCall       original invokeBasic call
  (direct JIT entry point)         (interpreter stub)
```

**6. Late Refinement: GVP** (see [GlobalValuePropagation: Late MethodHandle
Refinement](#globalvaluepropagation-late-methodhandle-refinement))

Even later in the pipeline, `GlobalValuePropagation` may discover known object information for
MethodHandle or MemberName objects that earlier passes could not resolve — for example, through
constant propagation or type narrowing. GVP can then refine remaining `invokeBasic` or `linkTo*`
calls and perform delayed inlining of the refined targets.

**Final result**: the compiled `process()` body is a direct virtual call to `String.strip` (or its
inlined body) — the `checkExactType`, `checkCustomized`, and `invokeBasic` layers have been entirely
compiled away. The MethodHandle abstraction carries zero runtime cost in steady state.

---

## Performance Optimization Opportunities

Understanding the above pipeline reveals several areas where further optimization work can improve
MethodHandle and VarHandle performance:

### 1. Improving Known Object Coverage

The effectiveness of `MethodHandleTransformer` and `checkVarHandleGenericType` folding depends on
whether MethodHandle and VarHandle objects can be identified as known objects at compile time.
Improving the coverage of the Known Object Table (e.g., by tracking objects through more field loads
or across more bytecode patterns) directly increases the scope of optimization.

### 2. Appendix Handling

When an appendix is a known constant (e.g., a `MethodType` known at compile time), it should be
fully folded. Ensuring that `useConstRefs()` is enabled and that the appendix KOI is propagated
through to `loadInvokeCacheArrayElements` eliminates unnecessary heap loads.

### 3. Inlining Depth for LambdaForm Adapters

LambdaForm adapters are often short chains of 2–4 calls (`checkExactType` → `checkCustomized` →
`invokeBasic`). Increasing the inlining budget for LambdaForm-generated methods can expose more
optimization opportunities in the final inlined body.

### 4. VarHandle Table Folding

Currently, `checkVarHandleGenericType` folding requires both the `VarHandle` and the
`AccessDescriptor` to be known objects. Improving recognition of common patterns (e.g., `VarHandle`
stored in a `static final` field) would increase the frequency of this optimization.

### 5. AOT Support

Full AOT support for `invokedynamic` and `invokehandle` (beyond the SVM-based approach) would
improve startup performance for applications that use lambdas and MethodHandles heavily. This
requires solving the relocation problem for invoke cache arrays and MemberName vmtarget pointers.

### 6. MutableCallSite Guard Efficiency

`MutableCallSite` guards currently cause deoptimization on any reassignment of the target. For call
sites where the target rarely changes, a more sophisticated approach (e.g., versioning with multiple
guard targets) could reduce deoptimization overhead.

### 7. linkToVirtual Direct Devirtualization

When `linkToVirtual` targets a final class or a method that is effectively final (no overrides in
loaded classes), the indirect virtual call can be converted to a direct call. Improved class
hierarchy analysis (CHA) integration in `refineMethodHandleLinkTo` could enable this.

### 8. Extending UnsafeFastPath Coverage for VarHandle

`UnsafeFastPath` handles the most common VarHandle operation methods but explicitly excludes
`VarHandleByteArrayAsX_ByteBufferHandle_method` due to complications with byte buffer utility
methods accessing fields as arrays. Extending fast-path support to more byte buffer view operations
(with correct address computation for both field and array shapes) would improve performance for
`ByteBuffer`-backed `VarHandle` accesses.

---
