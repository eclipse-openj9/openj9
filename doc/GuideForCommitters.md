<!--
Copyright (c) 2019, 2019 IBM Corp. and others

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


# Eclipse OpenJ9 Committer Guide

This document outlines some general rules for Eclipse OpenJ9 committers to
follow when reviewing and merging pull requests and issues. It also provides
a checklist of items that committers must ensure have been completed prior to
merging a pull request.


## General Guidelines

* Committers should not merge their own pull requests.

* If a pull request modifies the [Contribution Guidelines](https://github.com/eclipse/openj9/blob/master/CONTRIBUTING.md),
request the author to post a detailed summary of the changes on the
`openj9-dev@eclipse.org` mailing list after the pull request is merged.

* Do not merge a pull request if you are not one of the Assignees.

* If you will be one of the primary committers, add yourself to the set of
Assignees. Being the primary committer does not necessarily mean you have to
review the pull request in detail. You may delegate a deeper technical review
to a developer with expertise in a particular area by `@mentioning` them. Even
if you delegate a technical review you should look through the requested changes
anyway as you will ultimately be responsible for merging them.

* Do not merge a pull request if its title is prefixed with `WIP:` (indicating
it's a work-in-progress).

* If a review or feedback was requested by the author by `@mentioning` particular
developers in a pull request, do not merge until all have provided the input
requested.

* Do not merge a pull request until all validation checks and continuous
integration builds have passed. If there are circumstances where you believe
this rule should be violated (for example, a known test failure unrelated to
this change) then the justification must be documented in the pull request
prior to merging.

* If a pull request is to revert a previous commit, ensure there is a reasonable
explanation in the pull request from the author of the rationale along with a
description of the problem if the commit is not reverted. Additionally, committers 
should open an issue that links the revert commit to the original PR and tags the 
original author to ensure that the cause of the revert is addressed and not lost.

* Regardless of the simplicity of the pull request, explicitly Approve the
changes in the pull request.


## Pre-Merge Checklist

* Ensure the pull request adheres to all the Eclipse OpenJ9 [Contribution Guidelines](https://github.com/eclipse/openj9/blob/master/CONTRIBUTING.md).

* Ensure pull requests and issues are annotated with descriptive metadata by
attaching GitHub labels. The current set of labels can be found [here](https://github.com/eclipse/openj9/labels).

* Be sure to validate the commit title and message on **each** commit in the PR (not
just the pull request message) and ensure they describe the contents of the commit.

* Ensure the commits in the pull request are squashed into only as few commits as
is logically required to represent the changes contributed (in other words, superfluous
commits are discouraged without justification). This is difficult to quantify
objectively, but the committer should verify that each commit contains distinct
changes that should not otherwise be logically squashed with other commits in the
same pull request.

* When commits are pushed to a pull request, TravisCI builds launch automatically 
to build the changes on x86 Linux (with and without the linter). However, as this only 
builds the changes, you must initiate pull request builds sufficient to cover
all affected architectures and language levels prior to merging. To launch a pull
request build, see [Triggering PR Builds](https://github.com/eclipse/openj9/tree/master/buildenv/jenkins).

   If testing is only warranted on a subset of platforms (for example, only files
built on x86 are modified) then pull request testing can be limited to only those
platforms. However, a comment from the committer providing justification is
mandatory.

   If newer commits have been added (either as truly new commits or a rebase of existing
commits), PR builds must be triggered again.

   If any builds fail because of infrastructure issues, they must be restarted.

* Ensure `[ci skip]` is used for documentation only changes; any changes to code **must**
be tested using the appropriate PR builds.

* If the code change(s) necessitate change(s) to the [OpenJ9 Documentation](https://www.eclipse.org/openj9/docs/),
first add the `depends:doc` label to the OpenJ9 PR, and then ensure the contributer 
has opened an associated PR in the [openj9-docs](https://github.com/eclipse/openj9-docs) 
repository. An OpenJ9 PR that requires documentation changes should not be merged 
until the associated `openj9-docs` PR is also approved and ready to be merged.
