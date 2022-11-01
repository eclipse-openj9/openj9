<!--
Copyright (c) 2021, 2021 Josh Soref

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

# check-spelling/check-spelling configuration

File | Purpose | Format | Info
-|-|-|-
[allow/](allow/*.txt) | Add words to the dictionary | one word per line (only letters and `'`s allowed) | [allow](https://github.com/check-spelling/check-spelling/wiki/Configuration#allow)
[reject.txt](reject.txt) | Remove words from the dictionary (after allow) | grep pattern matching whole dictionary words | [reject](https://github.com/check-spelling/check-spelling/wiki/Configuration-Examples%3A-reject)
[excludes.txt](excludes.txt) | Files to ignore entirely | perl regular expression | [excludes](https://github.com/check-spelling/check-spelling/wiki/Configuration-Examples%3A-excludes)
[only.txt](only.txt) | Only check matching files (applied after excludes) | perl regular expression | [only](https://github.com/check-spelling/check-spelling/wiki/Configuration-Examples%3A-only)
[patterns.txt](patterns.txt) | Patterns to ignore from checked lines | perl regular expression (order matters, first match wins) | [patterns](https://github.com/check-spelling/check-spelling/wiki/Configuration-Examples%3A-patterns)
[expect.txt](expect.txt) | Expected words that aren't in the dictionary | one word per line (sorted, alphabetically) | [expect](https://github.com/check-spelling/check-spelling/wiki/Configuration#expect)
[advice.txt](advice.txt) | Supplement for GitHub comment when unrecognized words are found | GitHub Markdown | [advice](https://github.com/check-spelling/check-spelling/wiki/Configuration-Examples%3A-advice)

Note: you can replace any of these files with a directory by the same name (minus the suffix)
and then include multiple files inside that directory (with that suffix) to merge multiple files together.

---

## Workflow Matrix

In order to enable check-spelling to run in a timely manner, it has been split into a matrix:

Directory | Description
-|-
[../spelling](../spelling) | common content
[../spelling-runtime](../spelling-runtime) | configuration for the [../../../runtime](runtime) directory
[../spelling-test](../spelling-test) | configuration for the [../../../test](test) directory
[../spelling-other](../spelling-other) | configuration for all [../../../](other) directories

Each specific directory should at least share the common `allow` configuration (using symlinks) and will
have its own `expect` configuration. They can have their own additional `allow` configuration entries.

The files each part of the matrix checks are controlled by specific
`only.txt` and `excludes.txt` files.

Since the [advice.md](advice.md) file is specific to the individual directories,
it's possible to provide distinct advice for each one.

Initially they may be symlinks to the common advice.

Ideally, contributors won't see any of this, since, ideally they won't
make typos :smile:.

As long as all unrecognized words are limited to a single directory (`runtime`, `test`, or everything else), there should only be a single comment.
