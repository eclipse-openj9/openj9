/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

/*
 * Please note:
 *
 * Changing this file will require a Jenkins admin to approve the DSL script.
 * To avoid this approval each time, under "Configure Global Security"
 * uncheck "Enable script security for Job DSL scripts". This has been done
 * on the Eclipse OpenJ9 Jenkins instance.
 */

if (!binding.hasVariable('SDK_VERSION')) SDK_VERSION = ''
if (!binding.hasVariable('PLATFORM')) PLATFORM = ''
if (!binding.hasVariable('VARIABLE_FILE_DEFAULT')) VARIABLE_FILE_DEFAULT = ''
if (!binding.hasVariable('VENDOR_REPO_DEFAULT')) VENDOR_REPO_DEFAULT = ''
if (!binding.hasVariable('VENDOR_BRANCH_DEFAULT')) VENDOR_BRANCH_DEFAULT = ''
if (!binding.hasVariable('VENDOR_CREDENTIALS_ID_DEFAULT')) VENDOR_CREDENTIALS_ID_DEFAULT = ''
if (!binding.hasVariable('DISCARDER_NUM_BUILDS')) DISCARDER_NUM_BUILDS = '1'
if (!binding.hasVariable('SCM_REPO')) SCM_REPO = 'https://github.com/eclipse/openj9.git'
if (SCM_BRANCH ==~ /origin\/pr\/[0-9]+\/merge/) {
    SCM_BRANCH = 'master'
}

pipelineScript = 'buildenv/jenkins/jobs/pipelines/Pipeline-Initialize.groovy'

pipelineJob("$JOB_NAME") {
    description('<h3>THIS IS AN AUTOMATICALLY GENERATED JOB DO NOT MODIFY, IT WILL BE OVERWRITTEN.</h3><p>This job is defined in Pipeline_Template.groovy in the openj9 repo, if you wish to change it modify that</p>')
    definition {
        cpsScm {
            scm {
                git {
                    remote {
                        url(SCM_REPO)
                    }
                    branch(SCM_BRANCH)
                    extensions {
                        cloneOptions {
                            reference('$HOME/openjdk_cache')
                        }
                    }
                }
            }
            scriptPath(pipelineScript)
            lightweight(true)
        }
    }
    logRotator {
        numToKeep(DISCARDER_NUM_BUILDS.toInteger())
    }
    parameters {
        choiceParam('SDK_VERSION', ["${SDK_VERSION}"])
        choiceParam('PLATFORM', ["${PLATFORM}"])
        stringParam('OPENJDK_REPO')
        stringParam('OPENJDK_BRANCH')
        stringParam('OPENJDK_SHA')
        stringParam('OPENJ9_REPO')
        stringParam('OPENJ9_BRANCH')
        stringParam('OPENJ9_SHA')
        stringParam('OMR_REPO')
        stringParam('OMR_BRANCH')
        stringParam('OMR_SHA')
        stringParam('ADOPTOPENJDK_REPO')
        stringParam('ADOPTOPENJDK_BRANCH')
        stringParam('VARIABLE_FILE', VARIABLE_FILE_DEFAULT)
        stringParam('VENDOR_REPO', VENDOR_REPO_DEFAULT)
        stringParam('VENDOR_BRANCH', VENDOR_BRANCH_DEFAULT)
        stringParam('VENDOR_CREDENTIALS_ID', VENDOR_CREDENTIALS_ID_DEFAULT)
        stringParam('SETUP_LABEL')
        stringParam('BUILD_IDENTIFIER')
        stringParam('ghprbGhRepository')
        stringParam('ghprbActualCommit')
        stringParam('ghprbPullId')
        stringParam('ghprbCommentBody')
        stringParam('ghprbTargetBranch')
        stringParam('EXTRA_GETSOURCE_OPTIONS')
        stringParam('EXTRA_CONFIGURE_OPTIONS')
        stringParam('EXTRA_MAKE_OPTIONS')
        stringParam('OPENJDK_CLONE_DIR')
        stringParam('PERSONAL_BUILD')
        stringParam('CUSTOM_DESCRIPTION')
        stringParam('SCM_REPO', SCM_REPO)
        stringParam('SCM_BRANCH', SCM_BRANCH)
        stringParam('SCM_REFSPEC')
        booleanParam('ARCHIVE_JAVADOC', false)

        if (jobType == 'pipeline') {
            stringParam('TESTS_TARGETS')
            stringParam('BUILD_NODE')
            stringParam('TEST_NODE')
            stringParam('SLACK_CHANNEL')
            stringParam('RESTART_TIMEOUT')
            stringParam('RESTART_TIMEOUT_UNITS')
            choiceParam('AUTOMATIC_GENERATION', ['true', 'false'])
            choiceParam('JOB_TYPE', ['pipeline'])
        } else if (jobType == 'build') {
            stringParam('NODE')
            choiceParam('JOB_TYPE', ['build'])
        }
    }
}
