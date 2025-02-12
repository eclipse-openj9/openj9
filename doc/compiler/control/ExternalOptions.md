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

# Overview

In the context of the JIT, an "External Option" is an option that is not
specified as part of the `-Xjit` or `-Xaot` JVM argument. These options need to
be processed using either `FIND_AND_CONSUME_VMARG` or `FIND_ARG_IN_VMARGS`. The
main difference between the two is the former "consumes" the argument. For all
intents and purposes, this can be thought of as the JVM acknowledging the
argument as a valid argument. This is especially relevant when one specifies
`-XX:-IgnoreUnrecognizedXXColonOptions`, which will cause the JVM to terminate
with an error if there are any unconsumed arguments.

# Adding an External Option

There are two relevant structures: the `J9::ExternalOptions` enum and the
`J9::Options::_externalOptionsMetadata` table; these need to be in sync with
each other.

To add a new External Option:
1. Add a new entry to the end of the `J9::ExternalOptions` enum
2. Add a new entry to the end of the `J9::Options::_externalOptionsMetadata`
   array; specify the option string, the way it should be matched, a `-1` for
   the `_argIndex` (this will be updated at runtime), and whether or not the
   option should be consumed by the JIT.
3. Add a case to the switch in `J9::OptionsPostRestore::iterateOverExternalOptions`