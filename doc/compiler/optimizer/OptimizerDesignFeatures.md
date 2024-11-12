<!--
Copyright IBM Corp. and others 2022

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

# Design Features in the OpenJ9 JIT Optimizer

The design of an optimizer, specifically for a JIT compiler, must consider many aspects that differentiate
it from a static compiler.

A static compiler has the required compile-time budget to run sophisticated analyses and often repeats them
many times to achieve excellent code quality, whereas a JIT compiler must balance compile-time and runtime
performance.

The other major difference is that the JIT compiler runs concurrently with the application and can therefore
take into consideration aspects of the global state of the program that are relevant, such as the class
hierarchy and profiling data.

Most runtime environments that have a JIT compiler typically also have an interpreter, which is beneficial from a
performance standpoint (infrequently executed code need not incur compilation overhead), as well as from a
functional standpoint (edge cases that are not worth handling in the JIT compiler can be delegated to the
interpreter).

Allowing a "mixed mode" execution model, i.e., compiled code and interpreter running different parts of the
application, provides flexibility in terms of selecting which methods ought to be compiled and when to
compile them.

Given these differences between static compilation and JIT compilation, different aspects of compiler design
and infrastructure gain in importance depending on the context.

Features that we feel are especially important from an optimization perspective are

- [Heuristics on what/when/how to compile](#heuristics)
- [Hot path information and profile guided optimization in general](#hot-path-information-and-profile-guided-optimization)
- [Speculative optimization and compensation techniques as fallback mechanisms when speculation fails](#speculative-optimization-and-compensation-techniques-as-fallback-mechanisms)
- [Avoidance of expensive analyses such as Static Single Assignment(SSA), interprocedural analysis, expensive aliasing analysis, etc.](#avoidance-of-expensive-analyses)
- [IL representation that is more amenable to local analyses](#il-representation)
- [Cooperative threading model, compiled method metadata in conjunction with a stack walker](#cooperative-threading-model-and-compiled-method-metadata)

## Heuristics

Methods are the units of compilation in the OpenJ9 JIT compiler.

In the OpenJ9 JIT compiler, compilations are triggered either based on a method having been [invoked a certain number of times](https://github.com/eclipse-openj9/openj9/blob/20c2e83228780955fd62e1620160a81e7cbec41e/runtime/compiler/control/CompilationRuntime.hpp#L525)
or alternatively due to a method getting sampled frequently during the run.

There is a [JIT sampling thread](../control/CompilationControl.md#sampler-thread)
that periodically tracks the method at the top of the execution stack on each application thread and keeps
sampling statistics for each method.

Depending on the run time in question, the specific thresholds for both [invocation counts](../control/CompilationControl.md#invocation-counts)
and sampling counts can be configured. Initial compilations are typically driven by a combination of
counting and sampling, whereas [recompilations](../runtime/Recompilation.md) typically depend on sampling
since the overhead of counting can be significant in [compiled code](../control/CompilationControl.md#code-and-data-cache-management).

The OpenJ9 JIT compiler supports both [asynchronous](https://github.com/eclipse-openj9/openj9/blob/20c2e83228780955fd62e1620160a81e7cbec41e/runtime/compiler/control/CompilationRuntime.hpp#L764)
and synchronous modes for performing compilations.

In an asynchronous mode, the thread that is requesting compilation of a method continues executing the method
after queueing it for compilation, whereas in a synchronous mode, the requesting thread waits until the
compilation is done.

Most compilations are typically done asynchronously as that allows work to continue to be done on the
requesting thread, but synchronous compilations are necessary in some cases when it is not feasible to
execute the method in any other way (more on this later in the section on speculative optimization).

The compiler also supports doing compilations on multiple [compilation threads](../control/CompilationControl.md#compilation-threads)
concurrently to generate compiled code more quickly.

Performance is not only about peak throughput; other metrics such as [start-up](../control/CompilationControl.md#jit-state)
time and the time to [ramp up](../control/CompilationControl.md#jit-state) to peak throughput can
be improved significantly by asynchronous compilation and using multiple threads to do compilations.

A mixed mode execution model provides the flexibility to choose the basis for doing compilation.

Sometimes, it becomes necessary to transition mid-invocation from interpreter to compiled code (e.g. when
the method is only invoked once) and from compiled code to interpreter (e.g. when the JIT compiler does not
implement some rare edge case or if we choose to skip compiling some paths for performance reasons).
The OpenJ9 JIT compiler has both capabilities.

The mid-invocation transition from interpreter to compiled code is called [Dynamic Loop Transfer (DLT)](../control/CompilationControl.md#dynamic-loop-transfer)
and the mid-invocation transition from compiled code to interpreter is called [On Stack Replacement (OSR)](../hcr/NextGenHCR.md#osr-implementation-details).
They are valuable tools that can be essential when building JIT compilers for highly dynamic languages.

Another aspect of performance is application responsiveness, which is the reason why initial compilations in
the compiler are typically done at [lower optimization levels](https://github.com/eclipse-omr/omr/blob/cf8ddbd1adcd87e388cad6be1fd0e0c085285a29/compiler/compile/CompilationTypes.hpp#L45)
(that are cheaper to do from a compile-time perspective) whereas recompilations are done at higher optimization
levels on a more selective basis to avoid paying a high compile-time penalty for every compiled method.

Also, from a start-up time perspective, it is more important to stop interpreting frequently executed methods
by executing compiled code as soon as possible, even if the code is not of peak quality (the interpreter can
be several times slower than compiled code even at lowest optimization level).

## Hot Path Information and Profile Guided Optimization

The compiler has several optimizations that become more selective depending on the execution
frequency of the code it is optimizing. For example, since
[loop versioning](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/optimizer/IntroLoopOptimizations.md#2-loop-versioner)
and [specialization](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/optimizer/IntroLoopOptimizations.md#4-loop-specializer),
loop unrolling, [inlining](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/optimizer/Inliner.md),
and tail splitting are optimizations that cause code growth,
care must be taken that the code growth and subsequent compile-time increase in optimizations that
run later are worth the runtime benefit.

Frequencies are also considered in some of the most expensive optimizations that are in the OpenJ9 JIT
compiler such as Global Register Allocation (GRA), wherein blocks and candidates that participate in the
analysis can be chosen to keep compile time in check.

There are also many [local optimizations](LocalOptimizationsSummary.md)
that run one extended basic block at a time, and as such it is trivial to constrain these optimizations to
run mostly on more frequently executed basic blocks.

Frequently executed basic blocks are identified by a [profiling framework](https://github.com/eclipse-openj9/openj9/blob/cddb8bf6b5369e216402500b677da356114a38f8/runtime/compiler/runtime/J9Profiler.hpp#L551)
in the compiler that is flexible enough to accept input from varied sources, whether it be
some external profiler such as the interpreter (or even some offline scheme in theory) or a profiler that
is contained within the compiler and operates at the level of compiled code.

The profiler in the compiler is capable of tracking hot code paths as well as values relevant
to optimizations such as:
- Numerical loop invariant expressions, which is used to guide loop specialization
- Types of objects that are receivers of indirect calls, which is used to drive guarded devirtualization
- Array copy lengths, which is used to drive specialized arraycopy sequences

It is no overstatement that the profiling information has a significant impact on both the compile-time and
runtime characteristics for pretty much any JIT compiler, and this is especially true for the OpenJ9
JIT compiler.

## Speculative Optimization and Compensation Techniques as Fallback Mechanisms

Since the JIT compiler runs concurrently with the application, it has access to the runtime state of the
program and can track any changes as they occur.

One important example of this is the [class hierarchy](../jitserver/CHTable.md#vanilla-chtable) of the application,
which is useful when performing certain type related optimizations with much lower overhead than with profiling
information, e.g., devirtualization of indirect calls.

Although devirtualization based on profiling would add in an explicit guard test in the generated code,
it is also possible to devirtualize without a guard test in the generated code by consulting the class
hierarchy in some cases.

If a particular variable in the program is declared to be of a class type that is not yet extended by any
subclass at the time of JIT compilation, indirect calls on such a variable may be possible to devirtualize
without a guard test in the compiled code.

Of course, the compiler needs a mechanism by which it can compensate and ensure functional correctness
if a subclass does get loaded in the future; to this end, the OpenJ9 compiler has a data structure known as
the ["runtime assumptions table"](../runtime/RuntimeAssumption.md)
that essentially holds the information that needs to be re-visited every time the relevant runtime state
of the program changes.

Each runtime assumption has a routine that gets invoked by the JIT runtime infrastructure if the assumption
got violated and this routine performs the necessary fall-back action to preserve functional correctness.
In the case of devirtualization the code is patched at run time to execute the indirect call every time
the code is reached.

The "runtime assumptions table" together with the "class hierarchy table" (holds the current state of the
class hierarchy) and the ["hook infrastructure"](../runtime/JITHooks.md)
(essentially a collection of routines that notify the JIT run time of a change in the
relevant runtime state) form the backbone of the JIT compiler's speculative optimization framework for
type optimizations in any language assuming the right integration with the run time is implemented.

There are also other forms of speculation done by the compiler, especially around exception checks.
Higher level semantic knowledge that exceptions are rarely expected to be raised in most well-behaved
programs is used to drive optimization.

The [loop versioner](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/optimizer/IntroLoopOptimizations.md#2-loop-versioner)
clones a loop and eliminates all the exception checks that are loop invariant or on induction variables
from inside the good version of the loop based on a series of tests it emits before the loop is entered
by checking the equivalent conditions (more efficient since the checking is done once outside the loop
rather than on every loop iteration).

In the unlikely event that any of the tests done outside the loop fail (i.e., an exception was going to be
raised in the program), control is transferred to the slow version of the loop that contains all of the
exception checks.

The exception check abstraction in the OpenJ9 JIT compiler can be reused in any language that has the
concept of checked exceptions. By doing so, a rich set of speculative optimizations become available in the
JIT compiler for that language.

Historically, the compiler has relied more on "slow" compiled code paths rather than OSR
transition into the interpreter to handle the fall-back cases when speculation fails. This means the
"performance cliff" that a user experiences when speculation fails is not as steep as it might have been
if the fall back was exiting the compiled code completely.

Of course, the OpenJ9 JIT compiler does have an OSR infrastructure now as well, so the choice on what
to do on fall-back paths can be chosen heuristically.

## Avoidance of Expensive Analyses

Despite being selective with compiling methods, using lower optimization levels and hot path information to
reduce compile time, it is still a reality that some analyses that may be considered essential in a static
compiler are simply too expensive in a JIT compiler.

For example, the OpenJ9 JIT compiler analyses such as value numbering have been implemented to function
without Static Single Assignment(SSA) form representation, which would be too expensive to compute and
recompute as optimizations transform the IL of the compiled method.

SSA in particular would make the task of implementing certain analyses and optimizations simpler but the
extra complexity in implementation effort was a trade-off that was worth the compile-time savings that
accrued as result.

Another example of a standard static compiler analysis that is not present in the OpenJ9 JIT compiler is
sophisticated aliasing of any sort involving pointers or even much code analysis.

While presenting the lack of sophisticated aliasing as a feature may appear counter-intuitive at first, it
makes sense for a JIT compiler given the subtlety that JIT compilers are typically employed in languages with
a high degree of dynamism both in terms of call targets and the side effects that are permitted at various
program points.

Thus, the payoff of doing a sophisticated aliasing analysis is likely not worth the compile-time cost and
the basic aliasing that is there relies more on properties of the fundamental
properties of the symbols in use, e.g. a call aliases all static symbols.

Of course, the aliasing in particular can be extended or improved fairly easily and the optimizations in
the compiler are shielded from the internals of how the aliasing is computed via well-defined APIs.

Interprocedural analysis is another concept that is common in static compilers but once again the payoffs
in a dynamic language for a JIT compiler are probably not worth the effort to build and maintain a call
graph and associated data structures.

Another cool capability in the OpenJ9 JIT compiler that somewhat softens the blow of having to run
optimizations iteratively is the capability that optimizations may enable other optimizations as they run.
In other words, an optimization can either create or detect an opportunity that makes it important to run
another optimization, but the infrastructure allows later optimization passes to be enabled on-demand rather
than having to be run unconditionally, meaning we only pay the compile time of repeating optimizations if there
is some expected benefit.

## IL Representation

The OpenJ9 JIT compiler uses a Directed Acyclic Graph (DAG) to represent individual computations in
the program. A JIT compiler probably can only afford to run many expensive optimizations locally (i.e., per
extended basic block) given the compile-time constraints it operates under, and the DAG representation is
particularly convenient for doing local optimizations.

The DAG representation is less expensive from a memory footprint viewpoint since a single value within an
extended basic block is represented by a single node regardless of how many consumers (parent nodes in the
OpenJ9 JIT [IL representation](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/il/IntroToTrees.md))
of the value there are in that extended basic block.

It also makes the concept of "value equality" trivial for most local analyses, since this basically just
boils down to "node equality" making the code much cleaner and obviates the need for a separate analysis
step that tracks "value equality".

Another aspect of the OpenJ9 JIT IL representation that is novel is the use of exception check opcodes
and exception edges in the Control Flow Graph (CFG).

Together these two notions avoid chopping up the compiled method into many small basic blocks ending every
time an exception check must be done.

Essentially exception check opcodes represent control flow exiting the extended basic block going to the
exception catch block (if there exists one in the method) and exception edges represent this flow in the CFG.

Crucially this is done without ending the basic block every time an exception check is encountered, and the
"special" nature of exception checks and edges is known to the compiler's generic
[dataflow analysis](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/optimizer/DataFlowEngine.md) framework,
so that it does the right operations every time those special idioms are encountered.

This decreases the complexity of the CFG in general and makes the control flow look largely like it would have
done for a language without exception checking (e.g., C) and restricts the area of complexity in the compiler
to the dataflow analysis framework that is only changed very rarely in practice.

## Cooperative Threading Model and Compiled Method Metadata

Dynamic languages that use JIT compilers often require support for garbage collection (GC),
exception stack trace, debug code and hot code replacement.

In most of these cases, there is a need to cope with a change of state as execution proceeds to support the
desired functionality, while at the same time, the desired functionality is only needed in rare instances
(e.g., a user wanting to do a debug session is rare as is the need for an exception stack trace usually).

A naive solution might be to update the state as execution proceeds on the main line path, but this has the downside
that there is a lot of work done on the main line path that may never (or rarely) in fact be used. Most of these
requirements really hinge around locating the state needed to support the functionality at a given program point.

The key insight here is that this problem can be solved more efficiently by
- Restricting the program points where such state could be needed
- Forcing threads to only yield at these restricted program points and employing a stack walker to
figure out where each thread is
- Locating the metadata (capturing the state) laid down by the JIT at compile time for each of these
restricted program points

This eliminates the need to maintain the state in the main line code and reduces the overhead of supporting these
kinds of functionalities significantly.

The OpenJ9 JIT compiler already has a well-defined set of [yield points](../control/CompilationControl.md#async-checkpoint-sampling)
that are considered by all of the compiler and there are also well-defined extension points where the necessary
metadata can be emitted.

The threading and GC components also assume a cooperative model which makes it easier to implement this solution
in that infrastructure.
