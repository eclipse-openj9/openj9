<!--
Copyright (c) 2018, 2018 IBM Corp. and others

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

# Eclipse OpenJ9 Release Process

The OpenJ9 JVM is composed of two code repositories:

* OpenJ9 [https://github.com/eclipse/openj9]()
* OMR [https://github.com/eclipse/openj9-omr]()

OpenJ9 needs to be combined with code from OpenJDK to create a Java SDK.
The OpenJDK code is managed in separate github repos referred to as the 
"OpenJDK extensions for OpenJ9 for JDK X".  Currently there are two 
extensions repos:

* JDK8 [https://github.com/ibmruntimes/openj9-openjdk-jdk8]()
* JDK9 [https://github.com/ibmruntimes/openj9-openjdk-jdk9]()

OpenJ9 binaries are built by the AdoptOpenJDK community.  

An OpenJ9 release should:

* Use a single code base to support the various JDK-levels.
* Track the upstream OpenJDK releases.  
* Use a consistent set of tags across OpenJ9 & extensions repos to 
ensure that releases can be rebuilt or branched later.
* OpenJ9 releases should be high quality and pass a common quality bar. 
* Communicate the tags for a release to AdoptOpenJDK to ensure that
binaries at the correct levels have been created. 
* Clearly define the supported platforms
* Should not regress performance from release to release.


## Release cadence
OpenJ9 should aim to release quarterly to match the OpenJDK long term 
support (LTS) release cadence.  OpenJDK 8 is released quarterly and 
other LTS releases will likely follow similar release cadences.  There 
should be monthly milestone builds as the project builds towards the 
next release.

## Quality bar
Work with the AdoptOpenJDK community to define a common quality bar for 
any OpenJDK-based JDK.  The aim is to define a set of tests, both 
functional and stress, and 3rd party applications that should pass for 
every release.

## Platform support
Define the platforms that are supported by this release.  The set of 
platforms will depend on the availability of hardware at OpenJ9 and 
AdoptOpenJDK that can be used to validate the platform meets the quality 
bar.  The code base may support more platforms than can be validated by 
the quality.  Such platforms will not be officially included in the release.

## Performance
OpenJ9 should avoid regressing performance from release to release.
This is harder to validate in the open project given the limitted 
hardware resources available.  Work is being done at AdoptOpenJDK to 
enable performance testing.

# Release Process

1. Create an issue to track the release and record the information required
above, including the SHAs and tags.  This information will mostly be duplicated
with the Eclipse release plan.
1. Ask committers - via Slack and mailing list -  to refrain from committing any
further changes until an "all clear" signal is broadcast.  Disable the promotion
from the `openj9-omr` `master` branch to the `openj9` branch.
1. Pick a candidate OpenJ9 level to mark as the initial release candidate.  This
should be a level that has successfully passed the nightly testing builds at 
AdoptOpenJDK.
1. Tag the `openj9` & `openj9-omr` repos at the specified level with a tag of the
following form: `openj9-#releaseNumber#RC#candidatenumber#`.  For the `0.8` 
release, this would result in a `openj9-0.8RC1` tag.
1. Tag the Extensions repos at the levels that passed the AdoptOpenJDK builds with
the same tag.
1. Sound the "all-clear". The OpenJ9 `master` branch and `openj9-omr` promotion 
can be reopened for pull requests at this point.
1. Rebuild the tagged levels and ensure they pass the quality bar defined by 
AdoptOpenJDK.
1. Provide a window of time (a week?) for any stakeholders to highlight any 
stopship issues with the release candidate build.  If any are found, a 
determination can be made to either:
	* Branch from the the `RCx` tag and apply a targetted fix, or
	* Create the next release candidate build from the master stream if the 
	changes that have gone in between the initial RC tag and now are safe.
1. Retag the `RCx` level as `openj9-#releaseNumber#`.  For the `0.8` release this 
will be `openj9-0.8`.

The `java -version` should now show the tagged level.

