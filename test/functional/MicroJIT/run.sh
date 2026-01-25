#!/bin/bash

################################################################################
# Copyright (c) 2023, 2023 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
################################################################################

# Provide envars:
#  VERBOSE: Output the log to the terminal
#  MJIT_JAVA_BINARY: Full path to Java binary with MicroJIT enabled
#  MJIT_LOG_FILE: Output from JVM; look here for method signature from MicroJIT. Default: mjit.log
#  MJIT_INVOCATIONS: Number of times a method is run by the test driver. Default: 30000
#  MJIT_THRESHOLD: Compilation threshold. Default: 3
#  MJIT_DEBUG_ENABLED: Set to true for detailed output from OpenJ9
#  MJIT_NON_BREAKING_TEST: Set to true to test if MicroJIT changes have broken runs which do not use MicroJIT. Default: false
#  JUNIT_ARGS: List of JUnit arguments
#       See: https://junit.org/junit5/docs/current/user-guide/#running-tests-console-launcher
#       e.g. export JUNIT_ARGS=--disable-ansi-colors # for nice Jenkins output

# For each test defined in tests.json
# Execute TestRunner in JVM
# Result will be 0 if expected result is found, otherwise 1 (failure)
# Search $LOG_FILE for signature printed by MicroJIT
# If results == 0 and signature is found, then the function was JITed

# Trap Ctrl+C and call ctrl_c()
trap ctrl_c INT

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")

if [[ -z "$MJIT_JAVA_BINARY" ]]; then
    echo "MJIT_JAVA_BINARY required."
    exit 1
fi

JUNIT=${SCRIPTPATH}/lib/junit-platform-console-standalone-1.6.0.jar

MJIT_INVOCATIONS=${MJIT_INVOCATIONS:-30000}

MJIT_THRESHOLD=${MJIT_THRESHOLD:-3}

MJIT_LOG_FILE=${MJIT_LOG_FILE:-${SCRIPTPATH}/log_file}

MJIT_DEBUG_ENABLED=${MJIT_DEBUG_ENABLED:-false}

MJIT_NON_BREAKING_TEST=${MJIT_NON_BREAKING_TEST:-false}

VERBOSE=${VERBOSE:-false}

if [ ! -f "$MJIT_JAVA_BINARY" ]; then
    echo "$MJIT_JAVA_BINARY does not exist."
    exit 1
fi

function ctrl_c() {
    for pid_file in "${SCRIPTPATH}/.do/*/*/pid"; do
        kill -SIGINT $(cat "${pid_file}")
        rm "${pid_file}"
    done
    exit 127
}

function make_signature() {
    input="$*"
    j9_signature="${input/</(}"
    j9_signature="${j9_signature/>/)}"
    echo "+ (cold) ${j9_signature}"
}

function extract_classname() {
    echo "$*" | cut -d '.' -f 1
}

function extract_methodname() {
    echo "$*" | cut -d '.' -f 2
}

echo "Using JVM:"

eval ${MJIT_JAVA_BINARY} -version

printf "\nRunning tests (n=${MJIT_INVOCATIONS},threshold=${MJIT_THRESHOLD})...\n\n"

# Setup limit_file
rm -R .do || true
mkdir -p .do/pid

grep -v '^#\|^[[:space:]]*$' test-list.txt | while read -r line ; do
    signature="$(echo $line | cut -d':' -f1)"

    # Check if we passed test names
    run_it=false
    if [[ "_$*" == "_" ]]; then
        run_it=true
    else
        for regexes in "$@"; do
            if grep -E "${regexes}" < <( echo ${signature} ) &> /dev/null
            then
                run_it=true
                break
            fi
        done
    fi

    if [[ ${run_it} = true ]]; then

        # Run JVM with or without MicroJIT
        mjit_arg="-Xjit:disableAsyncCompilation,traceFull"
        if [[ "$MJIT_NON_BREAKING_TEST" = false ]]; then
            mjit_arg+=",verbose={compilePerformance},count=10000000,mjitEnabled=1,mjitCount=$MJIT_THRESHOLD"
        else
            mjit_arg+=",verbose,count=2"
        fi

        classname="$(extract_classname "${signature}")"
        fname="$(extract_methodname "${signature}")"
        this_rundir="${SCRIPTPATH}/.do/${classname}/${fname}"
        mkdir -p "${this_rundir}"

        make_signature "${signature}" > "${this_rundir}/limit"

        mjit_arg+=",log='${this_rundir}/"
        if [[ "$MJIT_NON_BREAKING_TEST" = false ]]; then
            mjit_arg+="mjit"
        else
            mjit_arg+="tr"
        fi
        mjit_arg+=".log',limitFile='${this_rundir}/limit'"

        printf "\
${MJIT_JAVA_BINARY} \\
    -XcompilationThreads1 \\
    ${mjit_arg} \\
    -jar $JUNIT \\
    --cp ${SCRIPTPATH}/build \\
    --disable-banner \\
    --disable-ansi-colors\
" > "${this_rundir}/cmd"
            # Dependant tests are a json array or a single test
        echo $line | cut -d ':' -f2 | cut -d '[' -f2 | cut -d ']' -f1 | tr -s '[[:space:]]' | tr ',' '\n' | while read -r dependency ; do
            make_signature "${dependency}" >> "${this_rundir}/limit"

            printf " \\
    --select-class='$(extract_classname "${dependency}")' \\
    --include-tag='$(extract_methodname "${dependency}")'\
" >> "${this_rundir}/cmd"
        done

        printf " \\
    --select-class='$(extract_classname "${signature}")' \\
    --include-tag='$(extract_methodname "${signature}")'\
" >> "${this_rundir}/cmd"

        printf "\
#!/bin/bash
export MJIT_INVOCATIONS=${MJIT_INVOCATIONS}
touch ${SCRIPTPATH}/.do/pid/\$$

gdb --args " > "${this_rundir}/debug.sh"
cat "${this_rundir}/cmd" >> "${this_rundir}/debug.sh"

        echo "\
#!/bin/bash
export MJIT_INVOCATIONS=${MJIT_INVOCATIONS}
touch ${SCRIPTPATH}/.do/pid/\$$

pushd '${this_rundir}' &> /dev/null
echo 'Running $signature ...'

TEST_PASSED=true
TEST_FAILURE_KIND=''
if ! source cmd 2> run.err > run.log; then
    TEST_PASSED=false
    TEST_FAILURE_KIND+=': JUnit or Java failure '
fi

if [[ "$MJIT_NON_BREAKING_TEST" = false ]]; then
    if ! grep -q 'MicroJIT Compiled $j9_signature Successfully!' mjit.log*; then
        TEST_PASSED=false
        TEST_FAILURE_KIND+=': Not compiled by MicroJIT '
    fi
else
    if ! grep -q '$j9_signature' tr.log*; then
        TEST_PASSED=false
        TEST_FAILURE_KIND+=': Not compiled by TRJIT '
    fi
fi

echo \"
TEST_NAME=\\\"$signature\\\"
TEST_PASSED=\\\"\${TEST_PASSED}\\\"
TEST_FAILURE_KIND=\\\"\${TEST_FAILURE_KIND}\\\"
\" > result_env

popd &> /dev/null
echo 'Running $signature ... Done'

rm ${SCRIPTPATH}/.do/pid/\$$
exit \$?
" > "${this_rundir}/test.sh"

        chmod +x "${this_rundir}/debug.sh"
        chmod +x "${this_rundir}/test.sh"
    fi
done

if [[ ${MJIT_DEBUG_ENABLED} = true ]] ; then
    for tests in .do/*/*/debug.sh; do
        test_name="$(echo "${tests}" | sed 's+.do/++g' | sed 's+/debug.sh++g' | sed 's+/+.+g')"
        printf "Running using gdb "${test_name}" Y/n [n]:"
        read answer
        case $answer in
            Y|y|yes)
                ${tests}
                ;;
            *)
                printf "\e[1ARunning using gdb $test_name Y/n [n]: ... skipping\n"
            ;;
        esac
    done
else
    echo .do/*/*/test.sh | xargs -n1 | xargs -n1 -P$(nproc --all) -I{} /bin/bash '{}'
    echo "
====================== Parsing Results ========================
"
    TESTS_FAILED=0

    for results in  $(echo .do/*/*/result_env | xargs -n1 | sort -u) ; do
        source ${results}

        line='. . . . . . . . . . . . . . . . . . . . . . . . .'
        printf "%s%s" $TEST_NAME "${line:${#TEST_NAME}}"
        if [[ ${TEST_PASSED} = true ]]; then
            printf "\033[0;32m \u2714 \033[0m\n"  "${TEST_FAILURE_KIND}"
        else
            printf "\033[0;31m \u2717 %s\033[0m\n" "${TEST_FAILURE_KIND}"
            # Don't print the JUnit container stuff
            if [[ ${VERBOSE} = true ]] ; then
                head -n -15 ${lines}/run.log
            fi
            TESTS_FAILED=$(( TESTS_FAILED + 1 ))
        fi
    done

    if (( TESTS_FAILED == 0 )); then
        if [[ "$MJIT_NON_BREAKING_TEST" = false ]]; then
            printf "Finished. All tests passed with MicroJIT!"
        else
            printf "Finished. All tests passed with TRJIT!"
        fi
    elif (( TESTS_FAILED == 1 )); then
        printf "Finished with [${TESTS_FAILED}] failure."
    else
        printf "Finished with [${TESTS_FAILED}] failures."
    fi
    printf "\n\n"

    exit ${TESTS_FAILED}
fi
