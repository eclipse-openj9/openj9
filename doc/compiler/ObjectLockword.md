<!--
Copyright (c) 2019, 2020 IBM Corp. and others

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
[2] http://openjdk.java.net/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
-->

# Table of Contents
1. [Background](#background)
2. [Original Lockword Format](#original-format)
    1. [Original Lockword Bit Pattern](#original-bit-pattern)
    2. [Original Locking States](#original-states)
    3. [Original Lock Reservation](#original-reservation)
    4. [State Transition Details](#original-transitions)
3. [Current Lockword Format](#current-format)
    1. [Current Lockword Bit Pattern](#current-bit-pattern)
    2. [Current Locking States](#current-states)
    3. [Current Lock Reservation](#current-reservation)
    4. [State Transition Details](#current-transitions)

# Background <a name="background"></a>
To facilitate synchronization, Java objects have a lockword to track information regarding locking. In the simple case, the per object lockword tracks which thread has locked the object and other threads need to wait for the object to be unlocked before they themselves can lock the object and continue into their own critical section. It is possible for the same thread to lock the same object multiple times before unlocking the object. This is known as nested locking. For an object to be fully unlocked so it can be locked by other threads the object must be unlocked an equal number of times to the number of times it was locked and the lockword also needs to keep track of this.

Since multiple threads may have an interest in updating the lockword to lock the object it is frequently necessary to use CAS to update the lockword. This is slower than using plain loads and stores to update the lockword. As a result, there is an interest in using lock reservation optimization to potentially speed things up. Lock reservation allows threads to reserve an object. What this means is that if a thread that owns the reservation on an object attempts to lock the object, it will be able to use plain loads and stores to update the lockword. The tradeoff is that if a different thread attempts to lock the reserved object this will trigger a very expensive process to cancel the lock reservation.

# Original Lockword Format <a name="original-format"></a>
## Original Lockword Bit Pattern <a name="original-bit-pattern"></a>
```
64 bit Lockword:
| TID (56 bits) | RC (5 bits) | Res | FLC | Infl |
32 bit Lockword:
| TID (24 bits) | RC (5 bits) | Res | FLC | Infl |

 TID = Thread ID excluding lowest 8 bits
  RC = Recursion Count
 Res = Reserved Bit
 FLC = Flatlock Contention Bit
Infl = Inflated Bit
```

The lockword format is largely defined in `runtime/oti/j9nonbuilder.h`. Defines are used to indicate which bit position has what meaning and to provide some useful bit masks.

The TID field takes advantage of how the lowest 8 bits in a Thread ID are always 0 so they don't need to be stored.

The RC or Recursion Count field is primarily used to track nested locking. One key point is the RC field is slightly different for reserved locks and unreserved locks. For reserved locks the RC field is equal to the nesting depth of the locks. This means that if the object was locked 3 times before being unlocked then the RC field will have a value of 3. If the object is not locked at all, the RC field will be 0. For unreserved locks the RC field is one less than the nesting depth. This means that if the object was locked 3 times without being unlocked, then the RC field will have a value of 2. If the object is not locked at all, the RC field will be 0 but the TID field must also be 0 for the object to be considered fully unlocked. If the same object gets locked over and over again without being unlocked, it is possible that the value to be stored in the RC field gets too big. To prevent the RC field from overflowing, the lock will be converted to an Inflated lock which can hold the information.

The reserved bit indicates whether the object has been reserved by a thread or not. 1 mean reserved and 0 means not reserved.

The FLC or flatlock contention bit is used to indicate that the lock is highly contended. This primarily means extra steps need to be taken when the lock is unlocked. It can't be set on locks that are unlocked.

If the inflated bit is set it will override the meaning of all the other bits in the lockword. The inflated bit being set means that the lockword actually contains a low tagged pointer to a `J9ObjectMonitor` data structure. Lock inflation is necessary for the cases where the required information can not be stored inside the lockword bits alone.

## Original Locking States <a name="original-states"></a>
There are 5 possible states the lockword can be in: Flat-Unlocked, Flat-Locked, Reserved-Unlocked, Reserved-Locked and Inflated. On x86 there is also a special New-ReserveOnce state.

```
| TID | RC (5 bits) | Res | FLC | Infl |
|   0 |           0 |   0 |   0 |    0 | Flat-Unlocked
| Set |        0-31 |   0 |   X |    0 | Flat-Locked
| Set |           0 |   1 |   0 |    0 | Reserved-Unlocked
| Set |        1-31 |   1 |   X |    0 | Reserved-Locked
|   X |           X |   X |   X |    1 | Inflated
|   0 |           0 |   1 |   0 |    0 | New-ReserveOnce (x86 only)

 TID = Thread ID excluding lowest 8 bits
  RC = Recursion Count
 Res = Reserved Bit
 FLC = Flatlock Contention Bit
Infl = Inflated Bit
```

### Flat-Unlocked
This is the initial state for new objects. In this state the object is unlocked so any thread is free to attempt to lock the object. It is possible that multiple threads may attempt to lock the object at the same time so an CAS operation is required to update the lockword and lock the object.

### Flat-Locked
This state means the object is locked and the thread that owns the lock does not have a reservation on the object. When the object is unlocked, it will go to the Flat-Unlocked state.

### Reserved-Unlocked
Reserved-Unlocked means while the object is unlocked, it is still reserved by the thread that previously locked the object. What this means is that if the thread that reserved the object tries to lock the object that thread does not have to use CAS operations to update the lockword. The tradeoff is that if a different thread attempts to lock the object this will trigger an expensive lock reservation cancellation process to convert the lock from being Reserved-Unlocked to Flat-Unlocked.

### Reserved-Locked
This state means the object is locked and the thread that owns the lock *does* have a reservation on the object. When the object is unlocked, it will go to the Reserved-Unlocked state. If another thread attempts to lock the object while it this state, it will also trigger a lock reservation cancellation that converts the lock to Flat-Locked.

### Inflated
It is possible for certain situations to occurs that require more information to be stored than would fit in the lockword. One such example is when the value of the RC field no longer fits. When this happens, the lock is "inflated". What this means is an additional data structure is allocated to hold the extra information and the lockword now points to this new structure. This also means the very slow paths are now taken to handle locking and unlocking of an object which may hurt performance. If a lockword no longer needs to be inflated, it will revert to the Flat-Unlocked state.

### New-ReserveOnce
This state is x86 only. It only appears if the `-XlockReservation` option is *not* set. When `-XlockReservation` is not set, aggressive lock reservation is disabled. This means objects can only be reserved once. If a lock reservation cancellation occurs and the object go to the Flat state, it will not be reserved again. To differentiate between objects that have and have not been cancelled, the New-ReserveOnce state was introduced. All objects have their lockword initialized to the New-ReserveOnce state. The first time the object is locked, whether in the VM or JIT'd code, it will be reserved. If the `-XlockReservation` option is set on x86, objects can be reserved again being cancelled. As a result, it is sufficient to initialize all lockwords to Flat-Unlocked.

## Original Lock Reservation <a name="original-reservation"></a>
Lock reservation is activated on a per class basis based on an allowlist. By default, there are two classes on the allowlist:
* `java/lang/StringBuffer`
* `java/util/Random`

Additional classes can be added via the `-Xjit:lockReserveClass=` option which accepts a regular expression.

### Reserving Locking in the JIT
Lock reservation can only occur inside JIT'd code. If it can be determined at compile time that the object being locked is from a class on the allowlist, the generated object locking code will attempt to reserve the object when it is locked. So when an object in the Flat-Unlocked state is locked it will transition to Reserved-Locked and the current thread will own the reservation. Since the object is not initially reserved, a CAS is required to update the lockword for this transition. Objects in the Flat-Locked state that get locked again will not be reserved. If the object is already in the Reserved-Unlocked or Reserved-Locked state and the current thread owns the reservation, then the reservation is maintained and a CAS is not required to update the lockword. However, if the current thread does not own the reservation, attempting to lock the object will trigger a lock reservation cancellation. Since lock reservation cancellation just converts a lock to a Flat state, it is possible for the lock to be reserved again the next time a reserving lock is attempted on the object. Inflated locks are not handled in JIT'd code so a call out to the VM is necessary.

### Non-Reserving Locking in the JIT
If the target object is not on the allowlist or it can not be confirmed at compile time, non-reserving locking code is generated. Objects in the Flat-Unlocked state simply transition to Flat-Locked when locked. Similarly, Flat-Locked objects stay as Flat-Locked if locked again in the nested lock case. Reservations made in other places are still preserved. This means that if an object in the Reserved-Unlocked or Reserved-Locked states gets locked by the reserving thread, they will still stay Reserved. Furthermore, CAS is still not needed to update the lockword in this case. As is probably expected, a non-reserving lock attempt on a reserved object by a thread that does not own the reservation will trigger a lock reservation cancellation. Inflated locks are not handled in this JIT code path either so there is still a call out to the VM.

### Locking in the VM
VM code handles things in a similar way to the non-reserving path in JIT'd code. Flat-Unlocked goes to Flat-Locked when locked. Flat-Locked is still Flat-Locked in the nested case. Reservations that already exist are preserved in the VM code paths. Reservation cancelations still occur if a thread that doesn't own the reservation attempts to lock a reserved object. VM code will manage the extra bookkeeping needed for handing Inflated locks.

### Unlocking
In the non-nested locking case if an unlock action is performed on an object in the Flat-Locked it will move to the Flat-Unlocked. In the nested case, the lock will remain in the Flat-Locked state but the record of the nesting depth is updated. For Reserved locks to process is very similar. Non-nested Reserved-Locked goes to Reserved-Unlocked and nested Reserved-Locked objects just have their nesting depth updated. This applies to both JIT's code paths and VM code.

## State Transition Details <a name="original-transitions"></a>
### Flat-Unlocked Transitions
#### Non-reserving Locking
A CAS is used to record the TID of the locking thread in the TID field. This transitions the lock to the Flat-Locked state.

#### Reserving Locking
In JIT'd code a CAS is used to record the TID of the locking thread in the TID field, set the RC field to 1 and set the Reserved bit to 1. This transitions the lock to the Reserved-Locked state. The VM code never performs a reserving lock.

#### Unlocking
An IllegalMonitorStateException exception is thrown since there is an attempt to unlock an object that is not locked.

### Flat-Locked Transitions
#### Locking, currently owns the lock
The RC field is incremented to track the nesting depth of the locking. Object remains in the Flat-Locked state. If the RC field will overflow, the lock will need to be inflated.

#### Locking, does not own the lock
The current thread waits for the thread that owns the lock to release the lock.

#### Unlocking, currently owns the lock
If the RC field is 0 this will fully unlock the object. The lockword will be set to all 0s to go to the Flat-Unlocked state.<br/>
If the RC field is 1 or more then the RC field is just decremented by one to keep track of the nesting depth.

#### Unlocking, does not own the lock
An IllegalMonitorStateException exception is thrown since there is an attempt to unlock an object that the current thread does not own.

### Reserved-Unlocked Transitions
#### Locking, currently owns the reservation
If the thread trying to lock the object also owns the reservation on the lock it is only necessary to increment the RC field from 0 to 1 to indicate that the object is now locked and in the Reserved-Locked state. The TID field already matches the thread attempting to lock the object.

#### Locking, does not own the reservation
If a thread that does not own the reservation on a reserved lock attempts to lock the object this will trigger a lock reservation cancellation. Lock reservation cancellation will remove the reservation on the lock to convert it to the Flat-Unlocked. At that point the current thread can attempt to lock the object again. If this occurs in JIT'd code, there will be a call out to the VM to handle the cancelation.

#### Unlocking
An IllegalMonitorStateException exception is thrown since there is an attempt to unlock an object that is not locked.

### Reserved-Locked Transitions
#### Locking, currently owns the lock
The thread that owns the lock is guaranteed to be the thread that owns the reservation. The RC field is incremented to track the nesting depth of the locking. Object remains in the Reserved-Locked state. If the RC field will overflow, the lock will need to be inflated.

#### Locking, does not own the lock
If the thread is not the one that owns the lock, then it does not own the reservation either. This means this lock attempt will trigger a lock reservation cancellation. Lock reservation cancellation will remove the reservation on the lock to convert it to the Flat-Locked. This thread will still need to wait for the original thread to unlock the object before this thread can lock the object. If this occurs in JIT'd code, there will be a call out to the VM to handle the cancelation.

#### Unlocking, currently owns the lock
The RC field is decremented by 1 to update information about lock nesting depth. If the RC field is now 0, the object is fully unlocked. One thing to note here is that the TID does not get cleared. The TID remains set to indicate which thread owns the reservation on the lock.

#### Unlocking, does not own the lock
An IllegalMonitorStateException exception is thrown since there is an attempt to unlock an object that the current thread does not own.

### Inflated
All locking and unlocking actions are handled in slow path VM code. If a lock no longer needs to be inflated, it reverts to Flat-Unlocked.

# Current Lockword Format <a name="current-format"></a>
## Current Lockword Bit Pattern <a name="current-bit-pattern"></a>
```
64 bit Lockword:
| TID (56 bits) | RC/LC (4 bits) | Lrn | Res | FLC | Infl |
32 bit Lockword:
| TID (24 bits) | RC/LC (4 bits) | Lrn | Res | FLC | Infl |

 TID = Thread ID excluding lowest 8 bits
  RC = Recursion Count
  LC = Learning Count
 Lrn = Learning Bit
 Res = Reserved Bit
 FLC = Flatlock Contention Bit
Infl = Inflated Bit

The 4 bit RC/LC field shares the 4 bits between the RC and LC in different ways depending on the lock state.

Non-Learning state case:
| TID | RC (4 bits) | Lrn | Res | FLC | Infl |

Learning state case:
| TID | RC (2 bits) | LC (2 bits) | Lrn | Res | FLC | Infl |

The Inflated state will override the other bits so there is no RC or LC field anymore.
```

Compared to the original format the RC field is now 1 bit smaller to make room for the new Learning bit. Also there is a new LC or Learning Count field that partially shares bits with the RC.

The TID field is largely handled the same way as before. It stores the TID (minus the lower 8 bits which are always 0) of the thread that currently owns the lock or the thread that reserved the lock. It can now also hold the TID of a thread that is trying to reserve the lock. This is connected to the new Learning state.

The RC or Recursion Count field is primarily used to track nested locking. Reserved locks and Flat locks are treated the same way as before. Locks in the Learning state handle RC in the same was as locks in the Reserved state. If a lock has been locked exactly 2 times with no unlocking actions in between and the lock is currently in the Learning state, then the RC will hold a value of 2. If incrementing the RC field would cause it to overflow, Reserved locks and Flat locks get converted to an Inflated lock which can handle to larger RC value. Locks in the Learning state have a smaller RC field so instead of transitioning to Inflated they first get converted to Flat which has a normal sized RC field. One thing to note is this will never happen under the default settings since a transition to Reserved or Flat should always happen before the RC field can overflow.

The LC or Learning Count is new. This field only appears for locks in the Learning state. It gets incremented whenever a lock is locked by the same thread. After it reaches a certain threshold the lock will transition from the Learning state to the Reserved state. The LC field is a saturating counter. It will not be incremented if this will cause an overflow. Under default settings, the object should always transition to Flat or Reserved before the LC saturates.

A set Learning bit is used to indicate that the lock is in the Learning state.

A set Reserved bit is used to indicate that the lock is reserved.

The FLC or flatlock contention bit when set is used to indicate that the lock is highly contended. This primarily means extra steps need to be taken when the lock is unlocked. It can't be set on locks that are unlocked.

If the inflated bit is set it will override the meaning of all the other bits in the lockword. The inflated bit being set means that the lockword actually contains a low tagged pointer to a J9ObjectMonitor data structure. Lock inflation is necessary for the cases where the required information can not be stored inside the lockword bits alone.

## Current Locking States <a name="current-states"></a>
There are 9 possible states the lockword can be in: New-PreLearning, New-AutoReserve, Flat-Unlocked, Flat-Locked, Reserved-Unlocked, Reserved-Locked, Learning-Unlocked, Learning-Locked and Inflated.

```
| TID |     RC (4 bits) | Lrn | Res | FLC | Infl |
|   0 |               0 |   1 |   0 |   0 |    0 | New-PreLearning
|   0 |               0 |   0 |   1 |   0 |    0 | New-AutoReserve

|   0 |               0 |   0 |   0 |   0 |    0 | Flat-Unlocked
| Set |            0-15 |   0 |   0 |   X |    0 | Flat-Locked

| Set |               0 |   0 |   1 |   0 |    0 | Reserved-Unlocked
| Set |            1-15 |   0 |   1 |   0 |    0 | Reserved-Locked

| TID | RC (2) | LC (2) | Lrn | Res | FLC | Infl |
| Set |      0 |    0-3 |   1 |   0 |   0 |    0 | Learning-Unlocked
| Set |    1-3 |    0-3 |   1 |   0 |   0 |    0 | Learning-Locked

| TID |     RC (4 bits) | Lrn | Res | FLC | Infl |
|   X |               X |   X |   X |   X |    1 | Inflated

 TID = Thread ID excluding lowest 8 bits
  RC = Recursion Count (number in parentheses is field width in bits)
  LC = Learning Count (number in parentheses is field width in bits)
 Lrn = Learning bit
 Res = Reserved bit
 FLC = Flatlock Contention bit
Infl = Inflated bit
```

### New-PreLearning
There are three possible initial lockwords when creating new objects. New-PreLearning is the default initial lockword. In this state, the object is considered unlocked and the first time the object is locked it will transition to Learning-Locked. CAS is required to update the lockword. There is no way to return to New-PreLearning after leaving.

### New-AutoReserve
This is the second possible initial lockword when creating new objects. When heuristics decide that an object should skip the Learning state and go straight to Reserved, this initial lockword is used. This object is also considered unlocked in this state and the first time the object is locked it will transition to Reserved-Locked. CAS is also required to perform this first lock and there is also no way to return to New-AutoReserve after leaving.

### Flat-Unlocked
This is both the third possible initial lockword for new objects and the unlocked state for locks in the Flat state. Heuristics may determine that an object should never be reserved. In those cases, the object will start in the Flat-Unlocked. CAS is required to edit the lockword in this state and when the object is locked it transitions to Flat-Locked. It is no longer possible to go from Flat-Unlocked to a Reserved state.

### Flat-Locked
This state is the same as before. This is for locked objects that are not in the Reserved or Learning state. When the object is fully unlocked it will go to the Flat-Unlocked state.

### Reserved-Unlocked
This state is also the same as before. The object is unlocked but the TID field still holds the TID of the thread that has reserved the lock. The thread that owns the reservation does not need to use CAS to acquire the lock and transition to Reserved-Locked. If a different thread tries to lock the object this will trigger a lock reservation cancellation.

### Reserved-Locked
This state is the same as before as well. When the object is unlocked it will go to the Reserved-Unlocked and the TID of the thread that reserved the object will remain in the TID field. Also as expected if a different thread tries to get the lock it will trigger a lock reservation cancellation.

### Learning-Unlocked
This is a new state for locks in the Learning state that are unlocked. When locked it normally transitions to the Learning-Locked state. However, it is also possible for the lock to go to a Reserved or Flat state as well. The Leaning state acts sort of like a trial reserved period. The TID of the thread that wants to reserve the object is recorded. If no other thread attempts to lock the object, the lock will transition to a Reserved state after being locked enough times. If another thread does attempt to lock the object, the lock transitions to a Flat state without the need for an expensive reservation cancellation. However, the tradeoff here is CAS always needs to be used to change the lockword when in the Learning state so it is ideal to get out of the Learning state as soon as possible.

### Learning-Locked
This is a new state for locks in the Learning state that are locked. When unlocked it transitions to the Learning-Unlocked state and the thread that is trying to reserve the lock still has its TID recorded in the TID field. Due to nested locking it is still possible to trigger a transition to Reserved-Locked. If another thread tries to lock the object it will get converted to Flat-Locked without a reservation cancellation. If the RC field overflows, the lock is converted to a Flat-Locked state which has a wider RC field. This is done to avoid going to the slow Inflated state. Similar to Learning-Unlocked, changes to the lockword always require a CAS operation.

### Inflated
The Inflated state is also the same as before. It is used to handle cases where more data needs to be stored then would fit in the lockword.

## Current Lock Reservation <a name="current-reservation"></a>
x86 and z have not been fully updated to use all the features of the new lockword format. As such, they effectively use the original system with a lockword that has a 4 bit RC instead of 5 bits. The Learning bit is always set to 0.

Under Power, lock reservation is now controlled by the `-XX:+GlobalLockReservation` option. Without the option, lockwords are always initialized to Flat-Unlocked and never reach any of the Reserved or Learning states. With the option set, lockwords may be initialized to New-PreLearning, New-AutoReserve or Flat-Unlocked. `-XX:-GlobalLockReservation` (note the minus sign) can be used to disable lock reservation. Lock reservation is disabled by default so this is only useful to override an enable option that occurs earlier in command line processing.

### Reservation Process
The purpose of the new Learning state is to try and reduce the number of lock reservation cancellations by acting as a halfway step towards reservation. If the same object is locked by the same thread enough times without another thread attempting to lock the object, then a prediction is made that it will be safe to reserve the object. The object will transition to a Reserved state. This information is tracked in the Learning state exclusive LC field. The first time a new object is locked it transitions from New-PreLearning to Learning-Locked but the LC will remain at 0. If the same thread locks the object again, even if it is a nested locking action, the LC is incremented. When the LC is equal to `reservedTransitionThreshold` (default value of 1), the next time the object is locked by the same thread it will go to Reserved-Locked instead of Learning-Locked. `reservedTransitionThreshold` can be set via the option `-XX:+GlobalLockReservation:reservedTransitionThreshold=#`. Due to the LC field only having 2 bits, values of `reservedTransitionThreshold` of 4 or higher will prevent transition from Learning to Reserved.

If a second thread attempts to lock an object in the Learning state, the expensive lock reservation cancellation is not required. This is because while in the learning state all changes to the lockword are done via CAS. The second thread simply uses a CAS to convert the lock from the Learning state to the Flat state and then tries to get the lock again. Once a lock has left the Learning state, it will never come back.

Another change relative to the original lock reservation scheme is an object can only be reserved once. If a thread attempts to lock an object in a Learning or Reserved state associated with a different thread, this will cause the object to transition to a Flat state. Once an object is in a Flat state it can never go to Learning or Reserved again. This is done due to predicting that once an object has been locked by multiple threads it will continue to be locked by multiple threads in the future. So to prevent costly lock reservation cancellations the object should never be reserved again.

### Lockword Initialization
Instead of using an allowlist to determine which classes are eligible for lock reservation, the determination is performed at runtime. Two new 16 bit counters were added to the J9Class. They are known as `reservedCounter` and `cancelCounter`. At a high level `reservedCounter` gets incremented went an object transitions to a reserved state and `cancelCounter` gets incremented when an object transitions to a flat state. In more detail:

* If a thread tries to access a lock that is currently in the Learning state for a different thread, then the `cancelCounter` for the J9Class of the object getting locked is incremented by one.
* If a thread tries to access a lock that is currently reserved by a different thread, then the `cancelCounter` for the J9Class of the object getting locked is incremented by one.
* If a thread tries to lock the same object enough times to transition it to the Reserved state, then the `reservedCounter` gets incremented by one.
* If a thread tries to lock an object in the New-AutoReserve it will immediate transition to the Reserved state so the `reservedCounter` also gets incremented by one.
* If incrementing a counter would cause an overflow, both counters are divided by two before the counter is incremented.

Due to how `reservedCounter` and `cancelCounter` are implemented it is possible for a race condition to occur when multiple threads attempt to update the same counter for the same J9Class. It was determined that this was an acceptable tradeoff in exchange for avoiding the cost of synchronizing access to the counters. The values of the counters do not need to be perfectly accurate to be usable for determining the initial lockword of an object. Also, this should never result in a functional issue as object locking will work regardless of which initial lockword is selected for the object.

The initial lockword for a new object is determined by comparing the `reservedCounter` and `cancelCounter` with the following four thresholds:

* `reservedAbsoluteThreshold` (Default: 10) - Minimum value of `reservedCounter` to initialize lockwords to New-AutoReserve.
* `minimumReservedRatio` (Default: 1024) - Minimum `reservedCounter` to `cancelCounter` ratio to initialize lockwords to New-AutoReserve.
* `cancelAbsoluteThreshold` (Default: 10) - `cancelCounter` must be greater than or equal `cancelAbsoluteThreshold` to initialize lockword to Flat-Unlocked.
* `minimumLearningRatio` (Default: 256) - Maximum `reservedCounter` to `cancelCounter` ratio to initial lockword to Flat-Unlocked.

1. If `reservedCounter` is >= `reservedAbsoluteThreshold` and `reservedCounter` is at least `minimumReservedRatio` times greater than `cancelCounter`, initial lockword to New-AutoReserve.
2. else if `cancelCounter` is >= `cancelAbsoluteThreshold` and `reservedCounter` is not at least `minimumLearningRatio` times greater than `cancelCounter`, initialize lockword to Flat-Unlocked.
3. else initialize lockword to New-PreLearning

In JIT'd code the values of the `reservedCounter` and `cancelCounter` are read at method compile time and the initial lockword is embedded into the generated code. The method would need to be recompiled to change the initial lockword. Other places that need to set an initial lockword will use the most recent values of `reservedCounter` and `cancelCounter`.

The default values can be changed via the following options:
* `-XX:+GlobalLockReservation:reservedAbsoluteThreshold=<value>`
* `-XX:+GlobalLockReservation:minimumReservedRatio=<value>`
* `-XX:+GlobalLockReservation:cancelAbsoluteThreshold=<value>`
* `-XX:+GlobalLockReservation:minimumLearningRatio=<value>`

Values of 65536 (0x10000) or higher are treated as 65536.

## State Transition Details <a name="current-transitions"></a>
Unlike the original lock reservation scheme, VM code will attempt to reserve locks as well. As a result, the VM and JIT'd code largely try to do the same thing. The exception is some cases can not be handled in JIT'd code so a call out to the VM is done instead.
### New-PreLearning Transitions
#### Locking
The TID of the thread locking the object is recorded in the TID field to both to say it owns the lock and is working towards reserving the lock. The RC field is converted to the 2 bit version and set to 1. A CAS operation is needed to save the new lockword and complete the transition to Learning-Locked. This case is not handled by JIT'd code so a call out to the VM is performed.

#### Unlocking
An IllegalMonitorStateException exception is thrown since there is an attempt to unlock an object that is not locked. This case is not handled by JIT'd code so a call out to the VM is performed.

### New-AutoReserve Transitions
#### Locking
The TID of the thread locking the object is recorded in the TID field to both to say it owns the lock and owns the reservation on the lock. The RC field is set to 1. A CAS operation is needed to save the new lockword and complete the transition to Reserved-Locked. This case is not handled by JIT'd code so a call out to the VM is performed.

#### Unlocking
An IllegalMonitorStateException exception is thrown since there is an attempt to unlock an object that is not locked. This case is not handled by JIT'd code so a call out to the VM is performed.

### Flat-Unlocked Transitions
#### Locking
The TID of the thread locking the object is recorded in the TID field to say it owns the lock. The RC field does not need to be updated. A CAS operation is needed to save the new lockword and complete the transition to Flat-Locked. Note that Flat-Unlocked always goes to Flat-Locked. Objects in the Flat state can not be reserved anymore.

#### Unlocking
An IllegalMonitorStateException exception is thrown since there is an attempt to unlock an object that is not locked. This case is not handled by JIT'd code so a call out to the VM is performed.

### Flat-Locked Transitions
#### Locking, currently owns the lock
The RC field is incremented to track the nesting depth of the locking. Object remains in the Flat-Locked state. If the RC field will overflow, the lock will need to be inflated.

#### Locking, does not own the lock
The current thread just waits for the thread that owns the lock to release the lock. A call out to the VM is done to handle the waiting.

#### Unlocking, currently owns the lock
If the RC field is 0 this will fully unlock the object. The lockword will be set to all 0s to go to the Flat-Unlocked state.<br/>
If the RC field is 1 or more then the RC field is just decremented by one to keep track of the nesting depth.

#### Unlocking, does not own the lock
An IllegalMonitorStateException exception is thrown since there is an attempt to unlock an object that the current thread does not own. This case is not handled by JIT'd code so a call out to the VM is performed.

### Reserved-Unlocked Transitions
#### Locking, currently owns the reservation
If the thread trying to lock the object also owns the reservation on the lock it is only necessary to increment the RC field from 0 to 1 to indicate that the object is now locked and in the Reserved-Locked state. The TID field already matches the thread attempting to lock the object.

#### Locking, does not own the reservation
If a thread that does not own the reservation on a reserved lock attempts to lock the object this will trigger a lock reservation cancellation. Lock reservation cancellation will remove the reservation on the lock to convert it to the Flat-Unlocked. At that point the current thread can attempt to lock the object again. If this occurs in JIT'd code, there will be a call out to the VM to handle the cancelation.

#### Unlocking
An IllegalMonitorStateException exception is thrown since there is an attempt to unlock an object that is not locked. This case is not handled by JIT'd code so a call out to the VM is performed.

### Reserved-Locked Transitions
#### Locking, currently owns the lock
The thread that owns the lock is guaranteed to be the thread that owns the reservation. The RC field is incremented to track the nesting depth of the locking. Object remains in the Reserved-Locked state. If the RC field will overflow, the lock will need to be inflated.

#### Locking, does not own the lock
If the thread is not the one that owns the lock it does not own the reservation either. This means this lock attempt will trigger a lock reservation cancellation. Lock reservation cancellation will remove the reservation on the lock to convert it to the Flat-Locked. This thread will still need to wait for the original thread to unlock the object before this thread can lock the object. If this occurs in JIT'd code, there will be a call out to the VM to handle the cancelation.

#### Unlocking, currently owns the lock
The RC field is decremented by 1 to update information about lock nesting depth. If the RC field is now 0, the object is fully unlocked. One thing to note here is that the TID does not get cleared. The TID remains set to indicate which thread owns the reservation on the lock.

#### Unlocking, does not own the lock
An IllegalMonitorStateException exception is thrown since there is an attempt to unlock an object that the current thread does not own. This case is not handled by JIT'd code so a call out to the VM is performed.

### Learning-Unlocked Transitions
Learning state is always handled in the VM code so JIT'd code always needs to call out.

#### Locking, same thread that first locked the object
If LC < reservedTransitionThreshold, a CAS is used to increment the RC field from 0 to 1 to indicate that the object is now locked and in the Learning-Locked state.<br/>
If LC >= reservedTransitionThreshold, a CAS is used to convert the RC field back to the 4 bit version and set it to 1. The Learning bit as also cleared while the Reserved bit is set.

#### Locking, not the same thread that first locked the object
A CAS is used to change the TID field to the TID of the new thread and all other bits are cleared. This changes the lock to Flat-Locked under the new thread. Note how a lock reservation is not needed for locks in the Learning state.

#### Unlocking
An IllegalMonitorStateException exception is thrown since there is an attempt to unlock an object that is not locked. This case is not handled by JIT'd code so a call out to the VM is performed.

### Learning-Locked Transitions
Learning state is always handled in the VM code so JIT'd code always needs to call out.

#### Locking, same thread that first locked the object
If LC < reservedTransitionThreshold, a CAS is used to increment the RC field to track nested locking. If the RC field will overflow, the lock is converted to Flat-Locked which has a wider RC field.<br/>
If LC >= reservedTransitionThreshold, a CAS is used to convert the RC field back to the 4 bit version and increment it by 1. The Learning bit as also cleared while the Reserved bit is set.

#### Locking, not the same thread that first locked the object
A CAS is used to convert the RC field back to the 4 bit version and clear the Learning bit. This converts the lock to Flat-Locked and then the current thread waits for the lock to be released.

#### Unlocking, currently owns the lock
The RC field is decremented by 1 to update information about lock nesting depth. If the RC field is now 0, the object is fully unlocked.

#### Unlocking, does not own the lock
An IllegalMonitorStateException exception is thrown since there is an attempt to unlock an object that the current thread does not own. This case is not handled by JIT'd code so a call out to the VM is performed.

### Inflated Transitions
All locking and unlocking actions are handled in slow path VM code. If a lock no longer needs to be inflated, it reverts to Flat-Unlocked.
