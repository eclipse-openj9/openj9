<!--
Copyright (c) 2018, 2019 IBM Corp. and others

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

# Eclipse OpenJ9 Issue Labels

OpenJ9 has defined a number of different labels for managing issues.
Many of the labels are of the form `{category}:{subcategory}`.
The labels are defined below along with notes about their intended usage.

Priority
---
Describe the priority of the particular issue.

* `pri:high`
* `pri:medium`
* `pri:low`


JDK
---
Indicate if this issue is specific for a given JDK level, such as JDK8.

* `jdk8`
* `jdk9`
* `jdk10`
* `jdk11`
* `jdk12`


Component
---
Indicate the likely component for this issue.  Segregating by component
helps to keep the list of issues to review managable and to guide
contributors for a particular area to the relevant items.

* `comp:vm`
* `comp:jit`
* `comp:test`
* `comp:doc`
* `comp:gc`
* `comp:build`
* `comp:port`
* `comp:infra`


Dependencies
---
OpenJ9 builds with several other projects: 'Eclipse OMR' & an 'OpenJDK
extensions for OpenJ9' repo for each JDK level.  These labels help to
identify dependent changes to ensure they are merged together or in the
correct order:

* `depends:omr`
* `depends:openjdk(8, 9, 10, 11, 12)`
* `depends:EclipseCQ`: Indicate a change needs a Contributor Questionnaire

OpenJDK projects
---
Labels for issues related to OpenJDK projects and JEPs:

* `project:panama`
* `project:valhalla`
* `JEP`

Performance
---
Labels for issues that affect performance or footprint.

* `perf`
* `footprint`

External documentation
---
Label to indicate that a PR or issue requires external documentation (new feature / change in
  behavior / limitation).

* `doc:externals`

**Note:** When you use this label, you must open an [issue](https://github.com/eclipse/openj9-docs/issues/new?template=new-documentation-change.md) in the OpenJ9 documentation repo.

End User issues
---
To indicate an issue has been raised by an end user or affects the User-experience rather than being a developer-centric issue.

* `userRaised`
* `svtRaised`: Indicate an issue was raised for an AdoptOpenJDK 3rd party issue or the OpenJ9 system verification test (svt).

