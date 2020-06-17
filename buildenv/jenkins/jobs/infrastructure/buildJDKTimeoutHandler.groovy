/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

timestamps {
    timeout(time: 15, unit: 'MINUTES') {
        node('worker') {
            echo "Build JDK Timeout Handler"
            try {
                stage('Clone repo') {
                    checkout changelog: false, poll: false,
                        scm: [$class: 'GitSCM',
                            branches: [[name: '*/openj9']],
                            doGenerateSubmoduleConfigurations: false,
                            extensions: [[$class: 'CloneOption',
                                        depth: 1,
                                        noTags: true,
                                        reference: '/home/jenkins/openjdk_cache',
                                        shallow: true]],
                            userRemoteConfigs: [[url: 'https://github.com/ibmruntimes/openj9-openjdk-jdk.git']]]
                }
                stage('Download and extract jtreg jar') {
                    sh 'wget https://ci.adoptopenjdk.net/view/Dependencies/job/jtreg/lastSuccessfulBuild/artifact/jtreg-4.2.0-tip.tar.gz'
                    sh 'tar xf jtreg-4.2.0-tip.tar.gz jtreg/lib/jtreg.jar'
                }
                stage('Build openj9jtregtimeouthandler.jar') {
                    sh 'javac -d . -cp jtreg/lib/jtreg.jar closed/test/jtreg-ext/jtreg/openj9/CoreDumpTimeoutHandler.java'
                    sh 'jar -cf openj9jtregtimeouthandler.jar jtreg/openj9/CoreDumpTimeoutHandler.class'
                    sh 'sha256sum openj9jtregtimeouthandler.jar > openj9jtregtimeouthandler.jar.sha256sum.txt'
                    archiveArtifacts artifacts: 'openj9jtregtimeouthandler.jar', fingerprint: false, onlyIfSuccessful: true
                    archiveArtifacts artifacts: 'openj9jtregtimeouthandler.jar.sha256sum.txt', fingerprint: false, onlyIfSuccessful: true
                }
            } finally {
                cleanWs()
            }
        }
    }
}
