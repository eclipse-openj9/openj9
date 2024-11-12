<!--
Copyright IBM Corp. and others 2021

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
-->

# Different Inline Fast Path Optimization Locations

## Inline Fast Path Optimizations

When there is a call to a method, the call can be executed in a cheaper way rather than making a call out to the target
method. The inlined fast path converts a recognized call into a cheaper sequence of IL trees. One example is a call to a
`java.lang.Math.abs()`. It can be converted into an `iabs` (absolute value of integer) [opcode](https://github.com/eclipse-omr/omr/blob/e3a15a993c8aba80582aa1d6f3071e122acbd4c4/compiler/il/OMROpcodes.enum#L1911). The call to `java.lang.Math.abs()` can be eliminated because there is a better
representation of the execution in IL, which is cheaper. This is a basic pattern of taking a call, recognizing them
in a certain way, and optimizing it, sometimes using opcode, sometimes using a sequence of IL trees etc.


There are multiple locations in our code where we do inline fast path optimizations:

1. [IL Generator](https://github.com/eclipse-openj9/openj9/tree/master/runtime/compiler/ilgen)
2. [Inliner](https://github.com/eclipse-omr/omr/blob/master/compiler/optimizer/Inliner.cpp)
3. [RecognizedCallTransformer](https://github.com/eclipse-omr/omr/blob/master/compiler/optimizer/OMRRecognizedCallTransformer.cpp)
4. [UnsafeFastPath](https://github.com/eclipse-openj9/openj9/blob/master/runtime/compiler/optimizer/UnsafeFastPath.cpp)

## Inlining Fast Path Optimization In ILGen

ILGen does not operate on trees, but it operates on bytecodes. It produces trees. It operates on the same operand stack
in Java bytecodes. It must track how many things are on the stack. It is not a very intuitive format to work with for
transformations. It is easier to do the transformation on well-formed trees rather than during the trees are being generated.

ILGen should be kept as simple as possible. The task of optimizations should not be mixed with the task of generating IL.
After IL is generated, [ilgenStrategyOpts](https://github.com/eclipse-omr/omr/blob/e3a15a993c8aba80582aa1d6f3071e122acbd4c4/compiler/optimizer/OMROptimizer.cpp#L535-L544) will run before other optimizations see the IL.
`ilgenStrategyOpts` always runs as soon as IL is generated. For example for normal inlining, after ILGen runs to produce the IL
for the callee method, `ilgenStrategyOpts` also runs on the generated IL before the method is inlined into the caller.

To avoid the complexity in the ILGen and to keep things simpler, the recognized methods in IlGen could be moved to
`RecognizedCallTransformer` whose purpose is to transform recognized calls.

## RecognizedCallTransformer

`RecognizedCallTransformer` is similar to what the Inliner does with respect to how it recognizes calls and converts them
into sequences of IL trees. However. Inliner is more complicated. There are a lot of things going on with respect to heuristics.
Inliner constantly checks: Do I have a budget and can I inline this call? There may be checks that prevent us from inlining
a particular method into another one because too many methods have been inlined already.

`RecognizedCallTransformer` does not need to worry about budget. It is much simpler. It does not have any particular heuristics.
It performs transformations unconditionally because 99% of the chance it is beneficial to do so.

`RecognizedCallTransformer` is run as part of `ilgenStrategyOpts`. It is viewed as a minor cleanup or a basic simplification path.
It converts calls into better performing or simpler IL trees. Since it runs early on, the later passes do not even see the
call that is converted.

Over the time, the intent may have been to take some of the other fast paths that happen inside the Inliner and the
recognized methods into `RecognizedCallTransformer`. The majority of them have not been done. Only a few methods are moved
to `RecognizedCallTransformer` to avoid the complexities of doing unsafe transformations in the middle of an already
complex transformation to inline calls.

## UnsafeFastPath

`UnsafeFastPath` focuses on `sun.misc.Unsafe` methods. `sun.misc.Unsafe` methods allow low level programming. For example,
`sun.misc.Unsafe.getByte(Object o, long offset)` takes the address of an object and adds it with the offset and reads a
byte out of it. Even if the object `o` might not be a byte array or might not even be an array, you could still read a
byte from the address. Unsafe calls allow to read any memory.

Another example is that we can dereference an object to get a `J9Class` pointer. The `J9Class` pointer can be dereferenced
into a class property pointer which can be dereferenced again. All these kinds of dereferences cannot be written in normal
Java code because they go through the JVM native data structures. They can be written in unsafe code. Unsafe path allows
us to access memory without any safe type checking or bound checking, etc. It also allows us to look at native memory
(`sun.misc.Unsafe.allocateMemory()`).

Permission change is required to use unsafe APIs. It is not meant to be used in general programing. It is meant to be a
tool to be used internally within JCL. Unsafe inlining is important because unsafe methods are used often by JCL. Calling
into natives is slow. It is a lot faster to use JIT inlined expressions.

`UnsafeFastPath` reduces unsafe calls to a "no control flow" sequence of IL trees to do the required operation. For example,
`getInt(Object o, long offset)` adds the address of an object `o` with the offset and loads an integer. They can be expressed
as the required operations directly (e.g. `load` or `store`).

The control flow that `UnsafeFastPath` avoids is what needs to be checked for determining which of the various cases the
unsafe access falls in, e.g. the cases when we are accessing a static, array etc. It tries to avoid general cases and
focuses on the specific cases/contexts where it knows enough about what the unsafe access is for to avoid that control flow
in the pass.

For example if an unsafe call is made in `java.util.concurrent.ConcurrentHashMap<K,V>`.  We could reason that the unsafe
call in `ConcurrentHashMap` will never see a `NULL` array object. It will be guaranteed to be a reference array, non static.
Things like this can be determined during the compilation. Therefore, all the checking can be skipped. We could just do
the load or store.

`UnsafeFastPath` also [runs as a part of](https://github.com/eclipse-omr/omr/blob/e3a15a993c8aba80582aa1d6f3071e122acbd4c4/compiler/optimizer/OMROptimizer.cpp#L543) `ilgenStrategyOpts` before `recognizedCallTransformer`.
It does a relatively simple substitution of certain calls very early without adding control flow before the inliner even
runs (so it never sees these calls in the IL trees).

Not all unsafe fast path is done in [UnsafeFastPath.cpp](https://github.com/eclipse-openj9/openj9/blob/master/runtime/compiler/optimizer/UnsafeFastPath.cpp). It is only used when there is no control flow. Inliner does the general unsafe inlining.
It checks many conditions before it does unsafe inlining such as if it accesses an array or accesses a static variable,
or if it is `NULL` or non `NULL`. There are various ways that these things can be checked.

`UnsafeFastPath` is different from the other three optimizations. The other three optimizations are not aware of the
context/caller. They do transformations in a general setting. `UnsafeFastPath` is more about the context where the unsafe
call is in since it is the context that allows us to generate a no control flow sequence.
