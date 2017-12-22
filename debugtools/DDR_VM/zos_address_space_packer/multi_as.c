/*******************************************************************************
 * Copyright (c) 2009, 2014 IBM Corp. and others
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


#define _XOPEN_SOURCE_EXTENDED 1
#include <unistd.h>
#include <spawn.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <assert.h>
#include <string.h>

/*
 * Test prog to pack multiple VMs into an address space
 *
 * This involves messing with environment variables that affect the spawn() call
 * and can't easily be done from the USS command line (it's easy enough to put Java
 * into the address space of the shell, seemingly impossible to put two Javas into
 * the address space of the shell).
 */

extern char **environ;

struct ToSpawn
{
	char * line;
	char * execname;
	char ** argv;
	pid_t pid;
	struct ToSpawn * next;
};

#define MAX_BUFFER 1024
#define MAX_ARGS 100

static struct ToSpawn * parseInputFile(FILE * inputFile)
{
	char buffer[MAX_BUFFER];
	struct ToSpawn * head = NULL;
	struct ToSpawn * tail = NULL;

	while (NULL != fgets(buffer,MAX_BUFFER,inputFile)) {
		struct ToSpawn * allocated;
		char * p;
		char * localArgv[MAX_ARGS];
		unsigned int i;
		unsigned int argContext = 0;

		if (buffer[0] == '\0') {
			continue;
		}

		/* Strip out any funny characters */
		p = buffer;

		while(*p) {
			if (*p == '\n' || *p == '\r') {
				*p = ' ';
			}
			p++;
		}

		allocated =  (struct toSpawn *)malloc(sizeof (struct ToSpawn));
		assert(allocated != NULL);

		memset(allocated, 0x00, sizeof(struct ToSpawn));

		allocated->line = strdup(buffer);
		assert(allocated->line != NULL);

		p = strtok(buffer," ");

		allocated->execname = strdup(p);
		assert(allocated->execname != NULL);

		while ( (p = strtok(NULL," ")) != NULL) {
			localArgv[argContext] = strdup(p);
			assert(localArgv[argContext] != NULL);
			argContext++;
		}

		allocated->argv = (char **) malloc((argContext + 2) * sizeof(char**));
		assert(allocated->argv != NULL);
		
		allocated->argv[0] = allocated->execname;
		for (i=0;i!=argContext;i++) {
			allocated->argv[i + 1] = localArgv[i];
		}
		allocated->argv[argContext + 1] = NULL;

		if (NULL == head) {
			head = allocated;
			tail = head;
		} else {
			tail->next = allocated;
			tail = allocated;
		}
	}

	return head;
}

static void spawnAndWait (struct ToSpawn * spawnList){
	struct ToSpawn * current = spawnList;

	while(current != NULL) {
		#define SP_FD_NUM  3
		#define MAX_ARGS 100
		int    sp_fd_cnt;
		int    sp_fd_map[SP_FD_NUM];
		struct inheritance sp_inherit;
		pid_t resultPid;


		/* Map file descriptors for child.  This enables the child to use  */
		/* the literal values (0, 1, 2) as file descriptors.               */
		sp_fd_cnt    = SP_FD_NUM;
		sp_fd_map[0] = 0;
		sp_fd_map[1] = 1;
		sp_fd_map[2] = 2;

		memset(&sp_inherit, 0x00, sizeof(sp_inherit));

	
		fprintf(stderr,"Spawning %s\n",current->line);

		fprintf(stderr,"execname is %s\n",current->execname);


		{
			int i=0;
			for(i=0;current->argv[i] != NULL;i++) {
				fprintf(stderr,"arg[%d] = %p, %s, %d\n",i,current->argv[i],current->argv[i],strlen(current->argv[i]));
			}
		}


		/* Spawn child process */
		{
			resultPid = spawn(current->execname,
								sp_fd_cnt, sp_fd_map, &sp_inherit, current->argv, environ);
		}
		if (resultPid < 0) {
			perror("spawn()");
			exit(1);
		}

		fprintf(stderr,"Started PID was %d\n",resultPid);
		current->pid = resultPid;

		current = current->next;
	}

	current = spawnList;

	while(current != NULL) {
		int childExitCode;
		pid_t waitResult;

		waitResult = waitpid(current->pid,&childExitCode,0);

		if (waitResult != current->pid) {
			perror("waitpid()");
		}
		current =  current->next;
	}
}

/*
 * One argument expected - list of commands to run
 */
int main(int argc, char * argv[])
{
	FILE * inputFile;
	struct ToSpawn * spawnChain = NULL;

	if (argc != 2) {
		fprintf(stderr,"Expected 1 argument, got %d\n",argc-1);
		return 1;
	}

	inputFile = fopen(argv[1],"r");

	if (NULL == inputFile) {
		perror("Opening input file");
		return 2;
	}

	/* Set the environment variable to push spawns into the same AS*/
	putenv("_BPX_SHAREAS=MUST");

	spawnChain = parseInputFile(inputFile);
	
	fclose(inputFile);

	spawnAndWait(spawnChain);

	/*Deliberately end without freeing up*/
}
