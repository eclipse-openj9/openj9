/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

if (!binding.hasVariable('extra_git_options')) extra_git_options = null
if (!binding.hasVariable('build_discarder_logs')) build_discarder_logs = 0
if (!binding.hasVariable('build_discarder_artifacts')) build_discarder_artifacts = 0
if (!binding.hasVariable('triggers')) triggers = null
if (!binding.hasVariable('github_project')) github_project = null
if (!binding.hasVariable('parameters')) parameters = null
if (!binding.hasVariable('quiet_period')) quiet_period = 0


pipelineJob(job_name){
    if (quiet_period > 0){
        quietPeriod(quiet_period)
    }
    description(job_description)
    definition{
        cpsScm{
            scm{
                git{
                    remote{
                        url(repository_url)
                    }
                    branch(repository_branch)
                    if (extra_git_options){
                        extensions{
                            cloneOption{
                                depth(extra_git_options.depth)
                                reference(extra_git_options.reference_repo)
                                noTags(extra_git_options.no_tags)
                                shallow(extra_git_options.shallow)
                                timeout(extra_git_options.git_timeout)
                            }
                        }
                    }
                }
            }
            scriptPath(pipeline_script_path)
            lightweight(true)
        }
    }
    if (build_discarder_logs || build_discarder_artifacts){
        logRotator{
            if (build_discarder_logs){
                numToKeep(build_discarder_logs)
            }
            if (build_discarder_artifacts){
                artifactNumToKeep(build_discarder_artifacts)
            }
        }
    }
    if (github_project){
        properties {
            githubProjectUrl(github_project)
        }
    }
    if (parameters){
        parameters {
            parameters.boolean_parameters.each{ boolean_parameter ->
                booleanParam{
                    name(boolean_parameter.key)
                    defaultValue(boolean_parameter.value)
                    description(parameter_descriptions.get(boolean_parameter.key))
                }
            }
            parameters.string_parameters.each { string_parameter ->
                stringParam{
                    name(string_parameter.key)
                    defaultValue(string_parameter.value)
                    description(parameter_descriptions.get(string_parameter.key))
                    trim(true)
                }
            }
            parameters.choice_parameters.each{ choice_parameter ->
                choiceParam{
                    name(choice_parameter.key)
                    choices(choice_parameter.value)
                    description(parameter_descriptions.get(choice_parameter.key))
                }
            }
        }
    }
    if (triggers){
        triggers{
            if (triggers.cron){
                cron(triggers.cron)
            }
            if (triggers.pull_request_builder){
                githubPullRequest {
                    admins(triggers.pull_request_builder.admin_list)
                    blackListCommitAuthor(triggers.pull_request_builder.blacklist)
                    cron(triggers.pull_request_builder.cron)
                    triggerPhrase(triggers.pull_request_builder.trigger_phrase)
                    onlyTriggerPhrase()
                    useGitHubHooks()

                    extensions {
                        commitStatus {
                            context(triggers.pull_request_builder.context)
                            triggeredStatus(triggers.pull_request_builder.triggered_status)
                            startedStatus(triggers.pull_request_builder.started_status)
                        }
                    }
                }
            }
        }
    }
}
