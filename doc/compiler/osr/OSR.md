
# Introduction

This document describes several OpenJ9 optimizations that use the OSR mechanism described in [OMR OSR document](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/osr/OSR.md). These optimizations include:

* Full Speed Debug (FSD)
* Next Generation Hot Code Replacement (nextGenHCR)
* Static Final Field Folding
* OSR on guard failure

# FSD

Until around 2014, OpenJ9 only allowed debugging optimized code in the old Full-Speed Debug (FSD) mode.
In the old FSD mode, a few simple optimizations such as trivial inlining, basic block extension, and local common sub-expression elimination were enabled.
The implementation faithfully maintained the correct stack slot values as well as the bytecode index (BCI) ordering as the interpreter would have
at any de-compilation point. For instance, inlining would only be done if there is no possible GC point in the code being inlined.
In addition, partial inlining was allowed only when there were no side-effects.
To solve all these issues, which seriously impacted performance, FSD was implemented using OSR.

New FSD is implemented using involuntary OSR. The set of OSR yield points includes all GC points: call nodes, nodes creating an object instance,
various checks including NULL checks and async checks, any debugging hooks such as method entries and exits, etc.
When a debugging event occurs, the next yield to the VM at a GC point that corresponds to that event causes an exception and the VM transfers control
to the correct catch block. For example, if "break on throw" is set, the next check node that throws an exception will cause control to be transferred to the
OSR catch block.


# nextGenHCR

Hot Code Replace (HCR) is a Java Virtual Machine (JVM) execution mode which aims to allow JVMTI agents to instrument Java code while still maintaining high performance execution.
Under HCR, agents may redefine the implementation of a Java method in terms of bytecodes, but may not alter the inheritance hierarchy or shape of classes
(no adding or removing fields or methods for example). If the code of a method is changed while we are executing that method, we are allowed to finish executing the old code.
For example, assume that A calls B and B is inlined into A. If B's definition changes while we are executing B's code (inlined in A), we can finish it and
the next call to B (from A or otherwise) needs to call the new definition of B.
However, if B's definition changes while we are executing A's code, the next call to B (even if inlined in A), needs to execute B's new code.

The JIT already supported HCR, but enabling HCR support incurred a ~2-3% throughput performance overhead as well as a compilation time overhead.
This meant that the JVM only enabled support for HCR if it detected agents requiring method redefinition at JVM startup.
This is why the HCR implementation was switched to use OSR - so that we could enable HCR all the time and therefore support late attach.


The traditional means of implementing HCR support in the JIT was to add an HCR guard to every inlined method.
The guard would be patched if the method it protected was redefined - this stops us from executing the inlined method body and instead calls the VM to run the new method implementation.
The control-flow diamond this would create is the reason for the throughput performance overhead mentioned previously -
the cold call on the taken side of the guard causes most analyses to become very conservative due to the arbitrary nature of the code the call may run.

Unlike traditional HCR, nextGenHCR implements HCR using OSR. OSR is described in detail in [OMR OSR document](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/osr/OSR.md). NextGenHCR will transition back to the VM when
a redefinition occurs, rather than simply calling the redefined method. This eliminates the control-flow diamond
and allows more optimizations to happen on the inlined path.


## Implementation

In contrast to Full Speed Debug, nextGenHCR is implemented as voluntary OSR and therefore the whole method is compiled in the voluntary OSR mode.
To limit the number of points where OSR transitions may occur under nextGenHCR, method redefinitions are only permitted at a subset of GC points.
These include calls, asyncchecks and monitor enters. There is also an implicit OSR yield point in the method prologue due to the stack overflow check.
NextGenHCR is implemented using post-execution OSR, such that OSR happens after an OSR yield point is executed.

During IL generation, a call to ```potentialOSRPointHelper``` is inserted immediately after every OSR yield point.

During inlining, the traditional HCR implementation runs, wrapping calls in HCR guards.

The **OSRGuardInsertion** optimization is performed after inlining and determines when an HCR guard can be replaced with a set of OSR guards based on **fear analysis**. The optimization uses the following concepts:

**Fear point**: a point in the IL where the JIT has transformed the code in a way that depends on one or more assumptions that can be invalidated at an OSR yield point. As an example, a fear point can be the entry point of an inlined method that can potentially be redefined. Another example of a fear point is a point at which a static final field load has been constant-folded (see [Static Final Field Folding](#static-final-field-folding)).

**HCR guard analysis**: a forward union analysis that determines at each program point whether the most recently run OSR yield point may have been uninducible. This analysis determines where fear points can be introduced, and therefore where assumptions can be made. A fear point can be introduced iff the most recent OSR yield point is necessarily inducible. Recall that method entry always contains an inducible OSR yield point. In particular, this analysis is used to determine which HCR guards may be treated as fear points and subsequently eliminated.

**Fear point analysis**: is a backward union analysis that determines whether it's possible for execution to reach a fear point without first hitting an OSR yield point. OSR guards are inserted after each OSR yield point at which this is possible. This ensures that every fear point is protected by the closest possible OSR guard. In addition, all of the resulting OSR guards are in positions where it's possible to induce OSR because HCR guard analysis has prevented the creation of any fear points that would require OSR guards where OSR is uninducible.

OSR guards are placed after OSR yield points and behave similarly to HCR guards. The guard will not necessarily be placed immediately after the OSR yield point,
rather several stores may be located between the two. These will include stores to pending push slots, to determine the stack at the transition point (as these are used for [rematerialization](../optimizer/LocalOptimizationsSummary.md#Rematerialization)),
and stores of call or allocation results.
On the passing side of an OSR guard, one or more fear points will be reachable without going through another OSR guard. On failure, the guard will transfer control to an OSR induce block.
The OSR induce block will contain a call to the ```jitInduceOSRAtCurrentPC``` or ```jitInduceOSRAtCurrentPCAndRecompile``` helper and will have exception edges to the **OSR catch block**.

As an optimization, we might have a store of the call result into an auto other than a pending push temp. This is because call results are often immediately stored to autos in the bytecode. Since we need to store it to the auto anyway, we want to avoid storing to a pending push temp, then potentially inducing OSR, and then storing again to the original auto. Instead we notice that the call result is stored into an auto (and presumably that what we store into the auto is always the result of the call, i.e. there's no branch target in between), and we push the OSR point forward past the store.

Runtime assumptions are added to patch the inserted OSR guards in the event of a method redefinition,
triggering the transition of execution to the JVM via the OSR mechanism. The OSR guards are permanently patched during an OSR yield to the JVM at which invalidation happens (e.g. a redefinition occurs). An important distinction is that all OSR guards will be patched in the event of any redefinition,
rather than just the required set to protect a removed HCR guard. This is due to the high overhead of tracking and describing the minimal set of OSR guards that would need to be patched on invalidation of each individual assumption.

## OSR and virtual guard merging

As an optimization, we can mark some existing virtual guard as an OSR guard, therefore merging them together. This is achieved by making virtual guards kill fear. In this case, the taken side of a merged OSR guard will be a cold call (vs. OSR induce block). The disadvantage of guard merging is that a later type analysis that would be sufficient to remove a virtual guard won't be able to also remove the corresponding OSR guard, which likely prevents the elimination of a control flow diamond.


## OSR induce blocks

The main purpose of the OSR induce blocks is to voluntarily induce OSR.
In addition, since OSR catch blocks apply for any possible transition points in a method, the OSR induce block allows bookkeeping to be specified at an individual OSR point.
It contains **irrelevant stores**, which store default values to symrefs.
These ensure that variables that are dead at a specific OSR point don't spuriously appear to be live based on uses in the OSR code block.
Moreover, the block potentially contains rematerializaion of live symrefs and monitor enters, to further reduce the constraints on other optimizations.
The OSR induce blocks end with an OSR induce (a call to ```jitInduceOSRAtCurrentPC``` helper) which transitions to the VM, and an exception edge to the OSR catch block.
Once the induce helper is called, we transition to the VM.


## Cannot attempt OSR

If the JIT executes an operation via intermediate states that differ from the VM, then we cannot transition during that execution since
the semantics of what the JIT has done cannot be expressed in terms of the semantics the VM would have executed.
This is evident in the JIT's thunk archetype based implementation of MethodHandles.
The calls have their BCIs flagged as doNotProfile. If those calls are later inlined (which is likely), the doNotProfile bit causes the corresponding inline frames to satisfy ```cannotAttemptOSRDuring()```.
All other unmarked BCIs in ILGen where one of nextGenHCR's OSR points may be generated must correctly represent the VM's stack with stores to pending push slots.


## CFG Examples

In this example, method A calls B, B calls C. B is inlined into A. C is not inlined into B.
Merging of OSR and virtual guards disabled for the sake of generality.


After IlGen:

```

                   --------------------------
                   |        entry           |--> catch 1
                   --------------------------
                              |
                   --------------------------   --------------------
                   |call B                  |-->|   catch 1        |
                   |potentialOSRPointHelper |   |                  |
                   --------------------------   --------------------
                              |                          |
                   --------------------------   ---------------------
                   |        exit            |   |prepareForOSR      |__> exit
                   --------------------------   |  iconst -1        |
                                                ---------------------
```


After Inlining:
```

                    --------------------------
                    |        entry           |---> catch 1
                    --------------------------
                              |
                    --------------------------
                    | non-overriden guard 1  |
                    --------------------------
                   /          |
                  /           |
                 /  --------------------------
                /   |  HCR guard             |---> catch 1
               /    --------------------------
              /    /          |
             /    /           |
  ----------------  --------------------------
  |call B (cold) |  |    call C (from B)     |---> catch 2
  |              |  |potentialOSRPointHelper |
  ----------------  --------------------------
                 \            |
                  \           |
                    --------------------------
                    |potentialOSRPointHelper |---> catch 1
                    --------------------------
                              |
                    -------------------------     ---------------------    ---------------------
                    |     exit              |     | catch 2           |    | catch 1           |
                    -------------------------     ---------------------    ---------------------
                                                           |                        |
                                                  ---------------------    ---------------------
                                                  |prepareForOSR      |___>|prepareForOSR      |__> exit
                                                  |   iconst 0        |    |  iconst -1        |
                                                  ---------------------    ---------------------
```
After OSRGuardinsertion:
```

                    --------------------------
                    |     entry              |---> catch 1
                    --------------------------
                             |
                    --------------------------
                    |   OSR guard            |
                    --------------------------
                   /         |
                  /          |
  ----------------  --------------------------
  |jitInduceOSR  |  |  non-overriden guard 1 |---> catch 1
  |AtCurrentPC   |  |                        |
  ----------------  --------------------------
        |          /         |
    catch 1       /          |
                 /           |
  ---------------   --------------------------
  |call B(cold) |   |    call C (from B)     |---> catch 2
  ---------------   --------------------------
       |         \           |
    catch 1       \          |
                    --------------------------
                    |                        |---> catch 1
                    --------------------------
                             |
                    --------------------------    ---------------------    ---------------------
                    |     exit               |    | catch 2           |    | catch 1           |
                    --------------------------    ---------------------    ---------------------
                                                          |                         |
                                                  ---------------------    ---------------------
                                                  |prepareForOSR      |___>|prepareForOSR      |___>  exit
                                                  |   iconst 0        |    |  iconst -1        |
                                                  ---------------------    ---------------------

```

After OSRExceptionEdgeRemoval (removes all OSR exception edges except those that originate from OSR induce blocks):
```


                    --------------------------
                    |     entry              |
                    --------------------------
                             |
                    --------------------------
                    |   OSR guard            |
                    --------------------------
                   /         |
                  /          |
  ----------------  --------------------------
  |jitInduceOSR  |  |  non-overriden guard 1 |
  |AtCurrentPC   |  |                        |
  ----------------  --------------------------
        |          /         |
    catch 1       /          |
                 /           |
  ---------------   --------------------------
  |call B(cold) |   |    call C (from B)     |
  ---------------   --------------------------
                 \           |
                  \          |
                    --------------------------
                    |                        |
                    --------------------------
                             |
                    --------------------------    ---------------------
                    |     exit               |    | catch 1           |
                    --------------------------    ---------------------
                                                          |
                                                  ---------------------
                                                  |prepareForOSR      |___>  exit
                                                  |   iconst -1       |
                                                  ---------------------


```

## Supporting Classes and Helpers

```OSRGuardAnalysis```  : detects OSR guards that are no longer required.

```OSRGuardInsertion``` :  replaces HCR guards with OSR guards based on the fear analysis described above.

```OSRGuardRemoval``` : removes OSR guards that became redundant due to optimizations after ```OSRGuardInsertion```. Uses ```OSRGuardAnalysis```.

```osrFearPointHelper``` : marks a place that has been optimized with runtime assumptions and requires protection of OSR mechnism. Similar to ```potentialOSRPointHelper```, this helper is not a must for optimizations requiring OSR mechnism. It works as a tool to help locating the actual fear (being part of the program that may be removed or optimized to something else).


## nextGenHCR Control Options

As this feature relies on HCR and OSR, both will be enabled when in nextGenHCR.
To prevent these from being enabled and to avoid any additional modifications to the trees, use ```-Xjit:disableNextGenHCR```.
In the event that disableOSR or the environment variable ```TR_DisableHCR``` is set, nextGenHCR will disable itself.
It should be noted that if the VM requests HCR when nextGenHCR is disabled, the traditional implementation will be used.
Setting the environment variable ```TR_DisableHCRGuards``` will result in nextGenHCR working as described above,
but it will not insert any OSR guards as there will be no HCR guards to replace.
This is a debug mode to evaluate OSR overhead and it will not correctly implement redefinitions.
There is a similar ```TR_DisableOSRGuards``` variable, which will prevent nextGenHCR from removing HCR guards and inserting OSR guards,
but will still include the other OSR and HCR overheads.

Both HCR implementations will be disabled under FSD, as HCR implements a subset of its functionality.
Moreover, nextGenHCR requires VM support with a feature described by the flag OSRSafePoint.
If this is not enabled, nextGenHCR will disable itself.
Finally, there are a number of situations where we will fallback on the traditional HCR implementation to reduce compile time overhead.
These include cold compiles, profiling compiles and DLT.


# Static Final Field Folding

This optimization uses the same infrastructure as nextGenHCR except that the fear points are the points just before the initial value of a static final field being used.


# OSR On Guard Failure

If OSR on guard failure is enabled (with ```-Xjit:enableOSROnGuardFailure```) when a guard fails, instead of taking the cold path we trigger an OSR. This is implemented as a voluntary pre-execution OSR.
