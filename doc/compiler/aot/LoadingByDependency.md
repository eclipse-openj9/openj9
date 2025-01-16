<!--
Copyright IBM Corp. and others 2025

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

# Dependency-based AOT loading

To maximize the performance benefit of AOT code loading, we would like to load cached AOT bodies for methods as soon as possible to transition away from interpreted execution. Traditionally, and temporarily still by default, AOT load triggering has been done purely by method invocation count. This has certain disadvantages that will be detailed below; loading based on dependencies is an alternative that aims to avoid these problems. We will first go over how to control the feature before discussing count-based loading, and then go on to dependency-based loading and the various components of its current implementation.

## Controlling the feature

The dependency tracking feature is enabled with the `-XX:[+|-]TrackAOTDependencies` flag. It will cause the dependencies of a method to be tracked during AOT compilations, and will cause the JIT to schedule AOT loads based on dependencies. It can be enabled or disabled on a per-run basis. Even when enabled, the `-Xaot:disableDependencyTracking` option can be applied to individual methods to prevent their dependencies from being tracked during an AOT compilation.

The verbose options `dependencyTracking` and `dependencyTrackingDetails` can be used to get information about the operation of the feature in the verbose log, including the dependency details of methods being compiled and the tracking of classes and methods in the dependency table. If the environment variable `TR_PrintDependencyTableStats` is set then the number of methods still tracked by the table is printed in the verbose log at JVM shutdown; adding those two verbose options will induce the table to print every method still tracked and the status of their dependencies, just as if the method had been queued for compilation before all of its dependencies were satisfied.

## Count-based loading

When a method is initialized, it goes through `jitHookInitializeSendTarget()` to receive an initial invocation count. Under count-based loading, that function will check to see if the method has a cached compiled counterpart in the local SCC. If so, it will receive a lower invocation count, controlled by the `-Xaot:scount` parameter. (The current default is 20). The method is then executed as usual; the purpose of the lower count is simply to trigger a compilation request for that method earlier than normal. The `preCompilationTasks()` will notice that the method has a corresponding cached AOT body in the SCC, and signal that an AOT load should be performed for that method.

The reason why the initial invocation count is not zero is that the JVM may be in a different, incompatible state at the moment the method is loaded compared to when it was compiled. This is partly due to the larger invocation count a method will receive during the cold run (when the method was AOT-compiled and stored in the SCC in the first place) and partly due to natural variation in the evolution of the JVM state. At the time of compilation, the JIT may have been able to gather a lot of profiling information, resolve a lot of classes, and inline a lot of methods. This will improve the performance of the compiled code, but makes that code harder to relocate, because at load time the relo runtime must be able to re-acquire candidates for all of those classes and methods in the new JVM. Those classes might not be resolvable right when the method is initialized, so the `scount` provides a heuristic load delay to give the running application a chance to load the necessary classes and methods.

Count-based loading thus has to tune the `scount`. If the value is too low, AOT loads will be more likely to fail, wasting compilation thread time and causing the application to miss out on the AOT performance gains. If the value is too high, then the JVM will spend excessive time in interpreted code, also degrading performance. This tuning is generally global and done via the `scount` on the command line or through options setup in the compiler, but there is some per-method `scount` adaptation - if an AOT load fails then we can store a hint in the SCC that the method should receive a higher `scount` on subsequent runs and hopefully avoid a failure the next time. This sort of heuristic loading can be avoided more or less completely, however, if the dependencies of the load are available and tracked.

## AOT dependencies

A dependency of an AOT method is a property of the JVM that must be true in order for the relocation of that method to succeed. The purpose of AOT dependency tracking is to keep a record of those properties that are simple to check, may not be true at method initialization, but can become true over time. We restrict ourselves to these kinds of dependencies so that we have a way of scheduling AOT loads more precisely; if all of the tracked dependencies of a method are satisfied at method initialization in `jitHookInitializeSendTarget()`, then that method can receive an initial invocation count of 0 and be loaded at its first invocation. Otherwise, the remaining dependencies of the method can be tracked, so that when they are all satisfied the invocation count of the method can be set directly to 0, which will trigger an AOT load on the method's next invocation.

Dependency-based loading is also only relevant for runs after the one during which the method was compiled. In that compilation run, we could in theory relocate and use the AOT code right away, but we instead set a high invocation count for the method to delay loading until a lot of valuable profiling information has been gathered for that method, information that is then stored in the SCC as well. Regardless, the dependency tracking system does not need to be involved in this case.

## Types of AOT dependencies

There are only two types of dependencies currently tracked:

1. A class with a particular class chain offset must be loaded in the current JVM
2. A class with a particular class chain offset must be fully initialized in the current JVM

These dependencies arise when a class needs to be resolvable by the relo runtime when it is processing the relocation records of a method. This might be because it needs to be able to check that a class entry at an index in a certain constant pool is resolved and matches what was recorded at compile time, or because it needs to be able to check that a class looked up by name in some loader exists and is a match. At the very least, there has to be a class loaded in the current JVM with the right class chain and initialization status for these kinds of operations to have any chance of succeeding. The initialization status is important because some classes must be fully initialized at relocation time, and we can't simply unconditionally require that all the classes in a method's dependencies to be fully initialized because some classes are never fully initialized.

## Tracking dependencies for AOT compilations

To have the dependency information for a cached method readily available, the relocatable compilation infrastructure will use `J9::Compilation::addAOTMethodDependency()` to make a note of:

1. The class chain offset of any class that will be relevant for relocation
2. The initialization status of that class

These dependencies are encoded in a "dependency chain" after the relocation records of the method are processed in `J9::AheadOfTimeCompile::processRelocations()`, and the chain will be stored in the local SCC. The compiled method will then be flagged as having its dependencies tracked, and an offset to the chain will be stored in the AOT method header. As an optimization, if a method has zero dependencies then no chain is stored in the SCC - we need the separate "tracks dependencies" flag to distinguish those methods from ones that do not track dependencies at all, because those latter methods must use traditional count-based loading.

As it happens, the only class load/initialization dependencies for a compiled method that need to be tracked are ones for class chain offsets that are mentioned in one of the relocation records of that method. These are the only classes that the relo runtime will want to resolve during relocation. For that reason, the dependencies are mostly gathered in `RelocationRecord.cpp`, in the methods that set the class chain offsets that are stored in the relo records. Specifically, a dependency is registered right next to every call to `J9::AheadOfTimeCompile::addClassSerializationRecord` and `J9::AheadOfTimeCompile::addClassChainSerializationRecord`, because those methods are what register all the class/class chain offset uses for JITServer AOT cache compilations (and so must cover every such use). Some dependency tracking also takes place in `SymbolValidationManager.cpp`, mostly related to well-known classes; instead of adding the contents of the entire well-known classes chain as dependencies, we only add those well-known class chain offsets that are actually used in the relocation records. This reduces the number of dependencies somewhat.

When dependency tracking is disabled for a method, we simply skip gathering dependencies for that method. Future JVMs will then be able to detect that the cached code did not have its dependencies tracked, and will use the usual count-based loading strategy for that method.

## Tracking dependencies for AOT loads with the dependency table

The other component to dependency-based loading, of course, is tracking dependencies for uncompiled methods with cached AOT code, so loads can be triggered when all the dependencies are satisfied. This happens through the `TR_AOTDependencyTable`. At method initialization in `jitHookInitializeSendTarget()`, we attempt to register the method with the table using `TR_AOTDependencyTable::trackMethod()`. That can have a few outcomes:

1. The method has no cached AOT body, or that body was not compiled with dependency tracking, in which case tracking fails and the method's initial count is set in some other way
2. Otherwise, the method's dependencies are all satisfied, in which case we do not track the method in the dependency table and the method gets an initial count of 0 (for immediate AOT loading)
3. Otherwise, the method remains tracked in the dependency table and it receives a high initial invocation count

If a method remains tracked, the dependency table will update the current state of the dependencies of the method as classes are loaded, initialized, unloaded, and redefined. Each of these events is registered with the dependency table in the appropriate JIT hook with a public dependency table operation. (The method `TR_AOTDependencyTable::classLoadEvent()` is responsible for registering class loading and initialization, for instance). If the table detects during that operation that the dependencies of a method have been satisfied, it will queue that method for removal from the table. At the end of the operation, it resolves all the pending methods with `TR_AOTDependencyTable::resolvePendingLoads()` by setting their counts to 0. Note that an operation can "unsatisfy" the dependencies of a method, say if a dependency was unloaded or redefined, which is why the pending loads can only be resolved once the operation is complete.

Since a tracked method's invocation count is still initialized and updated, it's possible for the count to reach zero before all of the dependencies of a method have been satisfied. If that happens, and the method will be queued for compilation, the table is notified so it can stop tracking the method. The resulting compilation will then most likely result in an AOT load attempt, which we do not actually prevent. Based on some experimentation, this kind of early load does not happen very often, and can have two outcomes:

1. The load succeeds, likely because the method was in the compilation queue for enough time that the depedencies became satisfied, even if they weren't satisfied at queue time.
2. The load fails, usually because the dependencies are still unsatisfied.

Methods will also be removed from tracking if their defining class is unloaded or redefined.

## Information stored in the dependency table and its uses

To track methods and their dependencies for AOT loading, the dependency table must maintain a couple of maps:

1. The `_methodMap` associates tracked `J9Method *` values with a `MethodEntry` that stores their current dependency state. This state includes the full dependency chain for the method as well as the number of dependencies that still need to be satisfied before the method can be loaded.
2. The `_offsetMap` associates ROM class offsets with an `OffsetEntry` that stores every `J9Class *` currently loaded that has a ROM class with that offset, and sets of `MethodEntry *` values that point to the entries of methods that depend on that offset. The classes are additionally required to have a valid class chain; the keys of the `_offsetMap` are by ROM class offsets to allow us to look up an `OffsetEntry` without having to look up the class chain for a class. (This doesn't conflict with the dependencies referring to class chains, because there can be at most one class chain per ROM class offset of the first class of the chain).

The `_methodMap` only needs to track the number of remaining dependencies in its entries because the dependency table will decrease that number on dependency satisfaction and increase it on dependency "unsatisfaction". It also keeps around the original dependency chain so the `_offsetMap` can be updated when a method is removed from tracking.

The `_offsetMap` is a complete record of all the classes in the JVM that, at class load time, had a valid class chain in the SCC. (The table only tracks these classes because only they are relevant for relocation after loading from the SCC). The dependency table can thus be used as a way of finding classes with a particular class chain offset, given only that offset. The relocation runtime uses the dependency table in this way in a few places, as a fallback when it cannot find a class in a particular class loader. (Note that some relocations need the class to be registered and locatable in a particular class loader for correctness, so this fallback cannot be used in those situations). The dependency table could be used as an alternative and very precise implementation of the `enableClassChainValidationCaching` feature, though some modifications would need to be made: the table would need to `rememberClass()` if a class did not have a chain in the SCC or it would need to be notified in `rememberClass()` when a class was remembered, and this would have to be done in the JIT class load hook before anything else that might need to know the class chain validation status of a class.

The `_offsetMap` needing to be complete is the reason why handling class redefinition in the dependency table is so complicated - if the underlying ROM class of the redefined class changed, the dependency table must:

1. Invalidate the old class and all of its subclasses by treating them as if they were just unloaded.
2. Revalidate the new class and every subclass of that class by treating them as if they were just loaded (and possibly initialized).

Class redefinition with a ROM class change only ever flips the validation status of a class and its subclasses, so to speak; the classes with valid class chains (chains matching the ones in the SCC) will necessarily no longer have valid class chains, and classes with invalid chains may become valid as a result of the redefinition. This allows the dependency table to skip some work when a class is redefined.

## Other difficulties with dependencies

Dependency tracking as it exists does decrease both the number of AOT load failures and the delay between method initialization and AOT loading. However, even if all the registered dependencies of a method are satisfied, the subsequent load may still fail during relocation because a needed class cannot be found, either directly or during certain method validations. This is relatively uncommon, and in some situations we are even able to recover from this by compile-time resolving the class and attempting the lookup again. (This is possible with certain unresolved constant pool entries - see `TR::SymbolValidationManager::validateStaticClassFromCPRecord()` for an example). Unfortunately, the failure might have arisen because a class was not yet registered in the class loader the relocation runtime was constrained to use for the lookup, even though the class was fully initialized. In this case, the Java spec appears to prevent the required compile-time resolution from taking place (see [this section on user-defined class loaders](https://docs.oracle.com/javase/specs/jvms/se23/html/jvms-5.html#jvms-5.3.2)), because even if the class loading had been delegated to another loader that had alreaded loaded the class, arbitrary Java code would still need to be run to ensure that the class is permitted to be loaded by that original loader.

Class-by-name lookup failures are in principle avoidable, at least in SVM, but at the cost of changing how relocations are performed. Another type of dependency (of the form "a class with name N is registered in class loader L") could be gathered and tracked, but that is inadequate, because the identity of these loaders might only become known to the SVM in the middle of relocation a method, as a result of the success of previous validations. For that reason, the JIT would need the ability to start relocating a method, pause relocation if a class hadn't yet been resolved in a loader, and then resume once that dependency were satisfied. (Being able to expand compile-time resolution to cover class registration in a loader would also work, if the above is wrong and such resolution isn't totally forbidden by the spec).
