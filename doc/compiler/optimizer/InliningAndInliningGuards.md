<!--
Copyright IBM Corp. and others 2023

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

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
-->

# High Level Outline

1. The Benefits Of Inlining
2. The Downside Of Inlining
3. Balance Multiple Paths Of Optimizations
3. Overview of Guards
4. Different Types Of Guards

# 1. The Benefits of Inlining

Inliner, local CSE (Common Subexpression Elimination), GRA (Global Register Allocation), and Global Value Propagation
are some of the most impactful optimizations from the perspective of improving performance throughput.
These highly effective optimizations are important for scorching and hot compilations as well as for a flat
profile with warm compilations.

## Basic Concepts
- **Call site** ([TR_CallSite](https://github.com/eclipse/omr/blob/88427d6420f5fa1bf00f897d1f589390bf2cafae/compiler/optimizer/CallInfo.hpp#L268))
is the code that makes the call. It contains the call node (caller) that makes the call and the call target
(callee) that is inlined into the call site.
- **Call target** ([TR_CallTarget](https://github.com/eclipse/omr/blob/88427d6420f5fa1bf00f897d1f589390bf2cafae/compiler/optimizer/CallInfo.hpp#L130))
represents the callee or the specific method that is inlined into the call site.

## Eliminate Calling Overhead

The benefits of inlining are wide ranging. An application with a flat profile generally has a lot of compiled
methods. The calling overhead in a flat profile could add up across all methods. The most direct benefit of
inlining is to get rid of call overhead such as call instruction, return instruction, prologue, epilogue, etc.
**The key question when it comes to inlining is how much is going to be executed versus how much code there is
in the callee**. For example, if a method calls another method that executes a thousand instructions, the call
overhead is probably not significant. However, if the method that is being called executes only 10 instructions,
or even just one or two instructions, or if a method returns a constant or returns a simple field read, the cost
of call instruction, return instruction, prologue, and epilogue adds up and can be substantial, compared to the
amount of the code that is actually executed.

## Allow Other Optimizations To Be Applied To The Inlined Code

Getting rid of call related overhead is only one aspect of the benefits of inlining. Inlining also exposes larger
blocks of code to the optimizer which provides more opportunities for other optimizations to improve the program.
Driven by the expectation that the benefits of other optimizations such as constant folding, branch simplification,
etc. that could occur, inlining enables JIT to do other powerful optimizations downstream of inlining.

For example, if an inlined method passes a constant from the caller to the callee and the constant is used on
the callee side, the code can be simplified dramatically by folding away the branches or changing loads into
constants. Such as when passing a constant `String` from a caller to a callee and `hashCode()` or `equal()` is
called on the constant `String` in the callee, these operations can be completely folded away during the compile
time rather than walking the entire `String` and compute the hash code in runtime. For this `String`, the answer
is already known during the compile time. Therefore, the call to `hashCade()` is replaced with a constant `String`.
It is powerful because a call with a non-trivial logic is replaced with a simple constant.

Similarly, if the callee compares the `String` that is passed in with some other constant strings, the comparison
can also be performed during the compile time. The call `equal()` is completely eliminated in the runtime.

Inlining also allows to stack allocate an object better because of a broader scope of things as a result of inlining.
A return object from the callee to the caller could also be eliminated if the method is inlined. If the caller uses
the object and then stops using it, it is possible to allocate the object on the stack. When the callee is brought
into the caller, there is no need to save and restore registers across the call. Register assignment can also be
done in a better way.

Inlining is an enabler for other optimizations that run later. That is partially the reason why inliner runs
fairly early in an optimization strategy. Code should be exposed as much as possible and as early as possible
to other optimizations so that other optimizations can take advantage of what inlining has been done and optimize
the code better.

# 2. The Downside Of Inlining

The downside is that the method gets much bigger if many callees are inlined into a method. In general, for a
language like Java that has so many calls and is object oriented, it is always beneficial to inline a method on
the hot path, unless the callee is really large and it executes lots of instructions. A small method on a hot
path tends to get inlined in a language like Java because of so much calling and object orientedness built into it.

# 3. Balance Multiple Paths Of Optimizations

There is a division of responsibilities among the paths of optimizations in different parts of the JIT. We do not
want to pollute every optimization with every other optimization. For example, we do not want to do folding or
dead tree elimination while we are doing inlining. The optimizations that generate the dead trees could have
added some code to eliminate the dead trees, but we choose not to do that. Instead, the dead trees will be cleaned
up by dead tree elimination optimization.

When inlining is taking place, we are aware of the opportunity for other optimizations because of inlining. Such
as when looking at the arguments of the call at the call site and there is a constant `String`, that should increase
the priority of inlining the call. It makes it more likely to inline the call because there might be other
optimizations that can happen downstream because of passing in this constant `String`. But inlining will not do
that optimization, that optimization will be done separately by other optimization paths when they run.

Generally speaking, other optimization do not differentiate if the trees are inlined calls or not.

Occasionally, some optimizations might be interested in where the code comes from. For example, if we know a method
is from `String` class, we know all the access of the method will be within the array bound. It is impossible to
raise an exception there. The bound check can be skipped for the method that comes from String class. Such thing
is normally restricted to sets of Java class library methods. Even if the `String` method is inlined into some
other arbitrary method `foo()`, that property should not change. The bound check should not be left in place
simply because `foo()` is being compiled. If it comes from the `String` code, regardless of whether it is inlined
or not, we should still be able to omit that bound check. This is an example of an awareness that the code comes
from an inlined method and where we differentiate the inlined code.

There are other enhancements that have been done over the years, such as method handle inlining. There is code
that analyzes the callee’s bytecode and looks at whether there are opportunities to do some optimization. More
and more code has been added in Inliner to be incrementally aware of the opportunities when going into the callees.

# 4. Overview Of Guards

## Hot Code Replacement (HCR) Guard

HCR is on by default. Any Java method can be redefined by the user while the program is running. As a result,
we must be able to reflect the output when any of the redefinitions happen. Even if there is a direct call such
as `invoke_static` or `invoke_special`, a guard must be added around it to preserve the functional correctness
in case that the method is redefined. The HCR guard works as a nopable guard (discussed in the vnext section).
The checking is not done explicitly in the generated code, which is good since there will be no overhead in doing
the extra check. If HCR is not enabled, a direct call is inlined without any guard.

HCR guards can be treated as a second guard. If there is no other guard, it introduces a new guard. If there is
a guard, it adds another layer as a second guard. For example, it is like two conditions, a virtual guard and
a HCR guard both exist in place to indicate if the method is overridden, or if the method is redefined.

## Nopable Guard

Nopable guard is like a conditional test conceptually except there is no explicit testing in the generated code.
There is an `“if”` in IL, but it will always fall into the inlined code during the compilation. If the user
redefines the method that is inlined in the future, we will go patch the code where it is inlined and take the
slow path to invoke the new version of the method. Such events are stop-the-world events where application
threads are guaranteed to not be executing, and hence patching in a full instruction into the instruction
stream is safe.

For a nopable guard to be used, it depends on the state of the class hierarchy at compile time. For example, `B`
subclasses an interface `A` when the compilation is done. `A` declares a method `foo()` which is implemented by
`B`. When the method is compiled and inlined, there is only one target `B`. In the future, if a subclass `C` is
loaded after the compilation is done and the code has been generated, there could be an object type of `B` as
well as type `C` that goes through `A.foo()`. Once `C` gets loaded, nopable guards cannot be used any more.
Before `C` gets loaded, JIT has an infrastructure that receives a notification that a new class is being loaded.
It assesses if any of the guards will be invalidated. We will patch the code where we have the nopable guards to
go to the slow path to invoke the call. Afterwards, the class `C` will be allowed to be loaded and to enter the
system. When we do nopable guard patching, the guard changes the state. We convert an unconditional fall through
into an unconditional goto.  Even if it is the object `B` that flows through, it will still jump to the slow
path to invoke the virtual call. Nopable guards require JIT to monitor the changes of program behaviour such as
class loading/unloading and do the patching to ensure inlined code is not invoked when we are not allowed to.

## Explicit Guard

Explicit guards are the guards that we generate the code for what we have in the trees.

There are two different cases. One case is that there is [a bit in every J9Method](https://github.com/eclipse-openj9/openj9/blob/8393a7277cbd664acc8c9477d66b2dc9a9d6f412/runtime/compiler/env/j9method.cpp#L5074-L5077)
indicating whether the method is overridden or not.  Any time a subclass is loaded, the VM checks if the method
in that class overrides the method in the super class and it sets that bit for that method. We can always
inline the method with a guard that checks the method. We explicitly test that bit in our IL trees. If it is not
overridden, the inline code is executed, otherwise it invokes the virtual call.

Another case of explicit test is based on profiling which is more common by default. Profiler tracks what type
of objects we encounter when we have the virtual call or the interface call. That is the receiver type.
The receiver is the object that is receiving the message to invoke the call. Either the interpreter or profiler
or JProfiler keeps track of the type of an object.

## Conditions Where A Nopable Guard Cannot Be Used

If there is more than one target based on the class hierarchy and the state of the program, an explicit guard
must be used. Java is a dynamic language and classes can be loaded in the future. Nopable guard can only be used
if there is one and only target regarding to the current state of the program or the current class hierarchy
when the compilation is done.

For direct calls there is exactly one possibility. When a method is invoked on an abstract class, the object that
will receive the method cannot be the type of the abstract class, because abstract class cannot be instantiated.
It will be a subclass of the abstract class. If there is only one subclass `A` of the abstract class `B`, there
is no point of checking if the object you use at the location is `B` or not. `A` being `B` is the only
possibility that exists right now.

When HCR is disabled, we know which method will be called and the bytecode within that method will not change.
With HCR, we know which method will be called but the bytecode within that method can change. Therefore, we
still need to have an HCR guard that is nopable.

For virtual calls, we do not know what to call, it could be `A.foo()` or `B.foo()` or `C.foo()`. We only know it
will be a subclass of `A`. If there is only one subclass or one target, then we can use nopable virtual guard.
If there are multiple possibilities, we must resort to profiling at compile time.

There is another possibility when nopable guards cannot be used: If the inlined method is too small, we cannot
use a nopable guard because we have to guarantee that there is enough space from the binary encoding of the
callee to encode the branch instruction.

## The Cost of Guards

Typically, in a normal application where the inliner functions well, 80~90% of the calls that are executed at
the runtime will have been inlined. This is in terms of the actual execution, and this is not a static count
done at compile time. For languages with virtual calls, such as Java, if two thirds of the calls are virtual or
interface calls, there will be guards in all those cases, some of these could be small methods. If you have
explicit guards as in guards with code generated for, in all those cases, it would be noticeable.

Using explicit guards everywhere in those 80-90% places where methods are inlined, it could have 5-10%
throughout impact. However, that is where we have the notion of nopable guards that we do not generate code for.
A good portion of the guards, at least half, that we lay down are nopable guards. Even if the guards are there,
we do not generate explicit test and branch. Roughly speaking, the cost of doing explicit guards is around 4-5%
in most cases. If inlining benefit gains ~50% throughput, the benefit of inlining gain ~45% after paying for
explicit guards.

# 5. Different Types Of Guards

Profiled guards are explicit guards and they are applied when nopable guards cannot be used such as when there
is more than one target in the class hierarchy.

Nopable guards are preferred when it is possible because it does not add overhead.

Non-overridden guards can be either explicit or inexplicit/nopable.

## Class Comparison Guard Or VFT Guard (Profiled Guard)

The class pointer in the object header is sometimes also referred to as a VFT (Virtual Function Table) pointer.
For example, `A` is an abstract class, and it has two subclasses `B` and `C`, both of which implements `foo()`.
Both `B` and `C` are loaded. Either of the types (`B` or `C`) could flow through `A.foo()`. It cannot be assumed
that it will unconditionally call either `B.foo()` or `C.foo()`. Inliner checks the receiver type of a virtual c
all which is the profiling information that either comes from an interpreter before a method is compiled, or
from JProfiling. When `o.foo()` is called, profiling tracks which type shows up and gives the probability of the
top value that has been observed. If the call is executed several times, 80% of the time the receiver type is
`B`, and 20% of the time the receiver type is `C`. If the probability of the top class is over certain threshold
(e.g. 75%), it is frequent enough to be inlined with an explicit test that does the following: load the class
pointer from `o` and check if `o.classPtr == B` (`J9Class`). If it is `B`, it executes the inlined code `B.foo()`,
else it branches to do a virtual call `A.foo()` and it goes to wherever the receiver is.

```
n11973n   ifacmpne --> block_10 BBStart at n11969n (inlineProfiledGuard )
n11971n     aloadi  <vft-symbol>[#294  Shadow] [flags 0x18607 0x0 ]
n11900n       ==>aloadi
n11972n     aconst 0xd99300 (com/ibm/crypto/provider/aN.class) (classPointerConstant sharedMemory )
```

## Method Comparison Guard (Profiled Guard)

Rather than comparing `J9Class` pointer as in VFT guard, alternatively method comparison can be used.
For example, if `o.class.vtable-entry` for `foo() == B.foo`, execute `B.foo()`, else do the virtual call.
Comparing method is more expensive because of that extra level of indirection: First you have to get `o.class`
and then you have to get the method entry for the particular method.

Method test is, however, also more robust. Using the above example, `B.foo()` is inlined. If in the future, a
new class `D` is loaded, and it extends `B` and does not override `foo()`, then it inherits `B.foo()`. The entry
for `foo()` on the VFT for `B` and `D` points to the same location. If you get an object of type `D` and do class
pointer comparison:  `o.classPtr == B` (`J9Class`), the condition check will fail and the inlined code will not
be called. A method comparison between `o.class.vtable-entry` for `foo()` and `B.foo()` will succeed. The
inlined code will be called, regardless of the object `o` being a type of `B` or `D` in this case.

```
n12450n   ifacmpne --> block_75 BBStart at n12445n (inlineProfiledGuard )
n12448n     aloadi  <vtable-entry-symbol>[#1655  Shadow +464] [flags 0x10607 0x0 ]
n12447n       aloadi  <vft-symbol>[#294  Shadow] [flags 0x18607 0x0 ]
n11616n         ==>aloadi
n12449n     aconst 0xd5a390 (methodPointerConstant sharedMemory )
```

If there is only one extended class `B`, or if the profiling information shows the receiver type is 100% `B`,
it might be safe to use a VFT test or a class pointer comparison guard. If more than one subclass is loaded
such as B, D, E, etc., and none of them overrides `B.foo()`, this is a sign to use a method test rather than
a VFT test. If the top value is 35% which is below the threshold of 75%, but its subclasses show 25% of `D`
and 25% of `E`, and three of them add up to more than 75% and all three classes map to `B.foo()`, the inliner
will choose the method comparison guard because it will succeed 75% of the time, but the class comparison
guard will have less chance of succeeding.

## Nonoverridden Guard (Explicit Guard or Nopable guard)

Nonoverridden Guard can either be an explicit guard or a nopable guard.
There is [a bit in every J9Method](https://github.com/eclipse-openj9/openj9/blob/8393a7277cbd664acc8c9477d66b2dc9a9d6f412/runtime/compiler/env/j9method.cpp#L5074-L5077)
indicating whether the method is overridden or not.  Any time a subclass is loaded, the VM checks if the method
in that class overrides the method in the super class and it sets that bit for the method.

```
n11271n   iflcmpne --> block_34 BBStart at n11265n (inlineNonoverriddenGuard )
n11270n     land
n11267n       lload      0xc28068[#1492  Static] [flags 0x307 0x0 ]
n11268n       lconst 4
n11269n     lconst 0
```

The method can be inlined with an explicit guard that checks the bit of the method in IL trees. If it is not
overridden, the inlined code is executed, otherwise it invokes the virtual call.

The same test can be nopable. When it gets to code gen (`virtualGuardHelper()`), it checks if the method is overridden
or not at compile time. If not, test code is not generated, and instead a runtime assumption is generated.
The runtime assumption can be interpreted as "if this method ever gets overridden, go patch this location to go
to the slow path”. It stores the information of where it is in the code and which method it depends on. Before
a class is being loaded, the JIT runs through all the relevant runtime assumptions and patches the locations.

Virtual guard noping can be disabled by setting `-Xjit:disableVirtualGuardNOPing`. There are also cases where
we do not want to use nopable guard. During start-up, all kinds of classes are still being loaded and the class
hierarchy has not settled down. By using nopable guard too early on, we would pay the price for patching because
after the guard is patched, the execution goes to the slow path forever. In this case, we might choose to be more
conservative and use explicit guards although they are less efficient. At least some proportion of the time,
the execution could go to the inlined path.

## Single Abstract Implementation Guard (Nopable guard)

When there is only one concrete class extending the abstract class and there is a call on an abstract class,
there is no need for profiling to tell which object or which method is going to be executed. It is going to be
 that single concrete implementation since there is no other concrete implementation. It gets patched when
there is a new concrete implementation of that abstract class is loaded. Meanwhile it is also difficult to do
a test because we do not want to walk through all the subclasses of the abstract class to figure out how many
entries there are and which entry it should be.  It could also get expensive to do the test if the method just
returns a constant field.

## Single Interface Implementation Guard (Nopable Guard)

Similarly, single interface implementation guard is added when there is a call on an interface and there is only
one implementation of the interface. The single implementer is inlined. In the future, when a new implementation
gets loaded that violates the runtime assumption, the single interface implementation guard is patched.

## Hierarchy Guard (Nopable Guard)

For example, there is a call `Object.equals()` in the bytecode. There are tons of classes that could implement
`equals()`. If JIT knows the call is invoked on an object of `Map` or an object of a subclass of `Map` through
value propagation and type propagation, a nopable guard can be used as long as there is only one implementation
of `Map` at compile time. If a new implementation gets loaded, the code is patched, and the slow path will be
invoked. In this case, although `Map.equals()` is not in the bytecode, the nopable guard can be generated based
on the information learned through value propagation and type propagation.

## Inner Preexistence Guard

Inner preexistence guard is a single guard that combines multiple guards that exist for other calls within the
method. When a method is inlined, a guard is added around it. Within that inlined code, there could be a need
to apply more guards such as inlining other code. The inner preexistence guard pushes some or all the guards
from the inner calls up into the outer call guard. The outer call guard can check more than one condition,
more than one inline site, more than one method. If any of the condition is violated, it goes to the slow path.
The inner preexistence guard is a multiple condition guard. The failed path out of the guard is the outer call
that is inlined. Instead of having control flow on the top level and another control flow in the inner level
again, inner preexistence guard combines the control flow on the top level.

## Side Effect Guard

Side effect guard is used rarely. After code analysis and call graph are done on a few methods, side effect
guard can be used to say if any of the methods in the block of code that is analyzed changes in the future, it
should go to the slow path where some compensation will be done to recover from the failed condition.

## Privatize The Receivers Before Guards

Before entering a guard, Inliner stores the receiver for the inner call as well as the receiver for the outer
call into temps to privatize the receivers. After the receivers are stored into local temporaries, even if a
new class is loaded in the future, the new object will not make it into the local variables. There will be no
mismatch between the type of the object and the method that is being dispatched.

Here is an example:
```
public class A {

   public B f;

   void foo() {
      f.bar();
   }
}

public class B { // Loaded by Thread T1
   void bar() {
      System.output.println(“I am B”);
}

public class D extends B { // Loaded by Thread T2
   void bar() {
      System.output.println(“I am D”);
}

o.foo();  // o is of type A at run time
```

An inner preexistence guard will be inserted before `foo()` that combines the guard for inlining both `foo()`
and `bar()` before entering `foo()`. There is no guard before entering `bar()` since the guard for entering
`bar()` is moved up to the top outer call `foo()`.

```
- Insert guards checks the methods are “A.foo && B.bar” before entering foo
- Load A.f  // enter A.foo
- Run System.output.println(“I am B”) from B
```

A thread `T1` executes and checks if the condition for `foo()` is satisfied and if the condition for `bar()`
is satisfied. If it is yes for both conditions, it enters into `foo(`) after passing the guard check.

Before it enters `bar()`, `T1` is context switched out. Another thread `T2` gets to run and loads another class
`D` and creates a new instance from the loaded class, sets up a new object in `o.f` which is a type of `D`
instead of the original type `B`.

The original thread `T1` is context switched back. `T1` is supposed to patch all the inlined locations for
`bar()` because `o.f` references a new instance. However because there is no guard before `o.f.bar()`,
it cannot do anything.

If the receivers are not privatized, the inlined `bar()` is mismatched to the newly created object which is a
type of `D` instead of the original type `B`. It will try to execute method `B::bar()` with an object of `D`,
which is incorrect.

With privatization, `o.f` which is a type of `B` has already been stored into a temp before the guard.
A temp is a local variable that other thread cannot change for thread `T1`, although `o.f` has been changed.
`B::bar()` will be executed with an object of `B` which is stored in the temp after thread `T1` is context
switched back.
