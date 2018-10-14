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

* OpenJ9 https://github.com/eclipse/openj9
* OMR https://github.com/eclipse/openj9-omr

OpenJ9 needs to be combined with code from OpenJDK to create a Java SDK.
The OpenJDK code is managed in separate github repos referred to as the 
"Extensions for OpenJDKx for OpenJ9".  Currently there are two 
extensions repos:

* JDK8 https://github.com/ibmruntimes/openj9-openjdk-jdk8
* JDK9 https://github.com/ibmruntimes/openj9-openjdk-jdk9

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
* Use Github releases to identify releases and link to the relevant
data.


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
Define the platforms that are supported by a release.  The set of 
platforms will depend on the availability of hardware at OpenJ9 and 
AdoptOpenJDK that can be used to validate the platform meets the quality 
bar.  The code base may support more platforms than can be validated by 
the quality.  Such platforms will not be officially included in the release.

## Performance
OpenJ9 should avoid regressing performance from release to release.
Each release should document the set of benchmarks and performance measures
that will be tracked for the release.  These measures should include the
acceptable noise margins that will be used when tracking the measure.
Regressions may be unavoidable at times due to changes required to be compliant
with the Java specifications, to close security holes, or to address the
need to update / modify the architecture.  Some regressions may simply be 
found too late in a release to address them.  
The project may decide to trade performance - whether throughput, startup 
or fooprint - for another important measure. 
Any regressions should be documented as part of the release plan.

Note, detecting regressions will be difficult in the open project 
given the limited hardware resources available.  Work is being done at 
AdoptOpenJDK to enable performance testing.

# Release Process

1. Create an issue to track the release and record the information required
above, including the SHAs and tags.  This information will mostly be duplicated
with the Eclipse release plan.
1. Pick a candidate OpenJ9 level to mark as the initial release candidate.  This
should be a level that has successfully passed the nightly testing builds at 
both OpenJ9 and AdoptOpenJDK. Given these builds may target a range of SHAs, a
candidate SHA should be taken from within that range.
1. Branch the `openj9` & `openj9-omr` repos at the specified level.  Branch names
should be of the form: `v#releaseNumber#-release`.  Immediately
tag the newly created branch with a tag of the following form: 
`openj9-#releaseNumber#-rc#candidatenumber#`.  For the `0.8.0` release, this would 
result in a `v0.8.0-release` branch with a `openj9-0.8.0-rc1` tag.  These branches 
are not intended as development branches and no continued support will be done on 
them.  They are merely points in time.
1. The Extensions repos should have been branched for each of the releases.
Update the Extensions branches to pull the tagged levels from the `openj9` 
& `openj9-omr` release branches.
1. Rebuild the tagged levels and ensure they pass the quality bar defined by 
AdoptOpenJDK. These builds need to have extra configure options which will
 identify them as release builds.  These options are`--with-milestone=fcs`
 (JDK8 - don't use the default option for an internal build) or
 `--without-version-pre --without-version-opt` (JDK9+ - don't set
 pre-release identifier or OPT field that contains a timestamp) added to the
 `./configure` command line.  These ensure that the correct version string
 is displayed in the `java -version` output.
1. Provide a window of time (a week?) for any stakeholders to highlight any 
stopship issues with the release candidate build.  If any are found, a 
determination can be made to either:
	* Apply a targetted fix to the release branch and retag, or
	* Rebase the release branch on the master branch if the 
	changes that have gone in between the initial RC tag and now are safe.
1. Retag the `-rcX` level as `openj9-#releaseNumber#`.  For the `0.8.0` release this 
will be `openj9-0.8.0`.
1. Create the [github release](https://help.github.com/articles/creating-releases/)
corresponding to the tagged level.  The release should link to the Eclipse Release 
document, the release issue, and the AdoptOpenJDK download links.
1. Open an Eclipse Bugzilla requesting the branch be marked `protected` to prevent 
commits after the release is complete.
1. Remove the `doc:releasenote` tag from items that were included in the release 
notes.

For a milestone release, the tag will be of the form: `openj9-0.8.0-m1`.

Note, use annotated tags (`git tag -a <tag name>`) as they are treated specially by
git compared to simple tags.

The `java -version` should now show the tagged level due to the `${java.vm.version}`
property being set to the tag.  Javacores will also display the tag if the build has
been tagged.

