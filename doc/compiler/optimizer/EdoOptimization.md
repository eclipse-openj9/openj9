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

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
-->
# Mechanics of the Exception Directed Optimization (EDO) Optimization

[Exception-Directed Optimization (EDO)](ControlFlowOptimizationsSummary.md#exception-directed-optimization-edo)
enables more aggressive inlining of methods that throw and catch exceptions to facilitate
the transformation of a throw operation into a goto by
[Value Propagation (VP)](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/optimizer/ValuePropagation.md).
Typically, profiling is used to determine which throw-catch pairs happen often enough to warrant extra inlining.

In OpenJ9, the EDO mechanism starts by adding a snippet that decrements the
[TR_PersistentJittedBodyInfo._counter](https://github.com/eclipse-openj9/openj9/blob/42a61be142bb792a86f3428c07f63d01b0c54982/runtime/compiler/control/RecompilationInfo.hpp#L435)
every time a catch block is executed. This is done in
[VMgenerateCatchBlockBBStartPrologue()](https://github.com/eclipse-openj9/openj9/blob/42a61be142bb792a86f3428c07f63d01b0c54982/runtime/compiler/x/codegen/J9TreeEvaluator.cpp#L11228)
and is gated by a test to `fej9->shouldPerformEDO(block, comp)`. The test makes
sure that we don't instrument big methods (that have more than `TR::Options::_catchSamplingSizeThreshold`
nodes) or if EDO is disabled with `-Xjit:disableEDO`.
When `TR_PersistentJittedBodyInfo._counter` reaches zero, the snippet will execute a jump to a recompilation
helper and the method will be recompiled at the next optimization level (typically hot).
Since higher optimization levels are more aggressive with inlining, sometimes this is enough
to bring the throw and the catch in the method being compiled. If that happens, `VP`
may change the throw into a goto (this transformation can be disabled with `-Xjit:disableThrowToGoto`),
and the process stops there.

The second step is to profile the frequency of the catch block in the newly compiled body with
[TR_CatchBlockProfiler](https://github.com/eclipse-openj9/openj9/blob/42a61be142bb792a86f3428c07f63d01b0c54982/runtime/compiler/runtime/J9Profiler.hpp#L347)
which is created in [J9::Recompilation::beforeOptimization()](https://github.com/eclipse-openj9/openj9/blob/42a61be142bb792a86f3428c07f63d01b0c54982/runtime/compiler/control/J9Recompilation.cpp#L234).
This is a very simple profiler that only includes two counters: one for throws and one
for catch operations (the counter for throws appears to be unused by the optimizer).
These counters are applied globally for the entire method and not per catch block/throw call-site.
The instrumentation is done by the [recompilationModifier](https://github.com/eclipse-openj9/openj9/blob/42a61be142bb792a86f3428c07f63d01b0c54982/runtime/compiler/runtime/J9Profiler.hpp#L260)
optimization which goes over the list of registered profilers and calls
[modifyTrees()](https://github.com/eclipse-openj9/openj9/blob/42a61be142bb792a86f3428c07f63d01b0c54982/runtime/compiler/runtime/J9Profiler.cpp#L92) on them.
In particular, [TR_CatchBlockProfiler::modifyTrees()](https://github.com/eclipse-openj9/openj9/blob/42a61be142bb792a86f3428c07f63d01b0c54982/runtime/compiler/runtime/J9Profiler.cpp#L256) allocates a [TR_CatchBlockProfileInfo](https://github.com/eclipse-openj9/openj9/blob/42a61be142bb792a86f3428c07f63d01b0c54982/runtime/compiler/runtime/J9Profiler.hpp#L811)
object with persistent memory (if it doesn't already exist) and, for each catch block it inserts
an increment instruction for the `TR_CatchBlockProfileInfo._catchCounter` field.
It's important to note that `recompilationModifier` is an optimization pass [enabled on-demand](https://github.com/eclipse-openj9/openj9/blob/42a61be142bb792a86f3428c07f63d01b0c54982/runtime/compiler/optimizer/J9Optimizer.cpp#L998)
in [Optimization::switchToProfiling()](https://github.com/eclipse-openj9/openj9/blob/42a61be142bb792a86f3428c07f63d01b0c54982/runtime/compiler/optimizer/J9Optimizer.cpp#L992)
if a few conditions are true:
- the optimizer decides to switch to profiling for some reason
- the compilation does not use "`optServer`" mode (unless `AggressiveSwitchingToProfiling` is on)

The third step is to recompile again the method and to use the collected profiling
data to guide the inliner to inline more on the throw-catch call-graph. The
[TR_EstimateCodeSize](https://github.com/eclipse-openj9/openj9/blob/42a61be142bb792a86f3428c07f63d01b0c54982/runtime/compiler/optimizer/EstimateCodeSize.hpp#L49)
estimator has a boolean field called [_aggressivelyInlineThrows](https://github.com/eclipse-openj9/openj9/blob/42a61be142bb792a86f3428c07f63d01b0c54982/runtime/compiler/optimizer/EstimateCodeSize.hpp#L128)
which is set to true when the method has catch-block profile information and
the catch counter is greater than [TR_CatchBlockProfileInfo::EDOThreshold](https://github.com/eclipse-openj9/openj9/blob/42a61be142bb792a86f3428c07f63d01b0c54982/runtime/compiler/runtime/J9Profiler.cpp#L2377).
Under those conditions the estimator
[under-estimates the size of the callees](https://github.com/eclipse-openj9/openj9/blob/42a61be142bb792a86f3428c07f63d01b0c54982/runtime/compiler/optimizer/InlinerTempForJ9.cpp#L3355-L3359)
with throws so that they are more likely to be inlined. If inlining succeeds and brings the throw
into the code for the method being compiled, the VP may then transform the throw
into a goto operation.
