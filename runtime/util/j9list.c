/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "j9user.h"
#include "j9protos.h"
#include "j9port.h"
#include "j9list.h"
#include "util_internal.h"
#include "util_api.h"

/*
 *	Standard dictionary using a singly linked list.
 *	The key is a (char *).  Data is an opaque (void *) never dereferenced.
 *	The key is copied to a buffer the list holds onto.
 */
j9list *list_new(J9PortLibrary *portLib)
{
	PORT_ACCESS_FROM_PORT(portLib);
	j9list *newList;
	newList = (j9list *) j9mem_allocate_memory(sizeof(j9list), OMRMEM_CATEGORY_VM);
	if(newList) {
		newList->first = NULL;
		newList->portLibrary = portLib;
	}
	return newList;
}


/*
 *	Kills the list and every item in it.  The data item is simply
 *	discarded.
 */
void list_kill(j9list *aList)
{
	PORT_ACCESS_FROM_PORT(aList->portLibrary);
	node *tmp2, *tmp = aList->first;

	while(tmp) {
		j9mem_free_memory(tmp->key);
		tmp2 = tmp->next;
		j9mem_free_memory(tmp);
		tmp = tmp2;
	}
	j9mem_free_memory(aList);
}


node *list_insert(j9list *aList, char *key, void *data)
{
	PORT_ACCESS_FROM_PORT(aList->portLibrary);
	node **tmp;

	tmp = &(aList->first);
	while(*tmp && ((*tmp)->next)) {
		tmp = &((*tmp)->next);
	}
	if(*tmp)
		tmp = &((*tmp)->next);
	*tmp = (node *)j9mem_allocate_memory(sizeof(node), OMRMEM_CATEGORY_VM);
	if(!*tmp)
		return NULL;
	(*tmp)->next = NULL;
	(*tmp)->key = (char *)j9mem_allocate_memory(strlen(key)+1, OMRMEM_CATEGORY_VM);
	if(!(*tmp)->key) {
		j9mem_free_memory(*tmp);
		*tmp = NULL; /* reset the next pointer to not point at unallocated memory */
		return NULL;
	}
	strcpy((*tmp)->key, key);
	(*tmp)->data = data;
	return *tmp;
}


void *list_remove(j9list *aList, node *aNode)
{
	PORT_ACCESS_FROM_PORT(aList->portLibrary);
	node *tmp, **last;
	void *data;

	tmp=aList->first;
	last = &(aList->first);
	while(tmp) {
		if(tmp == aNode) {
			j9mem_free_memory(tmp->key);
			data = tmp->data;
			*last = tmp->next;
			j9mem_free_memory(tmp);
			return data;
		} else {
			last = &(tmp->next);
			tmp = tmp->next;
		}
	}
	return NULL;
}


node *list_find(j9list *aList, char *key)
{
	node *tmp = aList->first;
	while(tmp) {
/*		printf("node: <%s> <%d>\n", tmp->key, (int) tmp->data); */
		if(!strcmp(key, tmp->key))
			return tmp;
		tmp = tmp->next;
	}
	return NULL;
}


node *list_no_case_find(j9list *aList, char *key)
{
	node *tmp = aList->first;
	while(tmp) {
/*		printf("node: <%s> <%d>\n", tmp->key, (int) tmp->data); */
		if(0 == j9_cmdla_stricmp(key, tmp->key))
			return tmp;
		tmp = tmp->next;
	}
	return NULL;
}



BOOLEAN list_isEmpty(j9list *aList)
{
	return aList->first ? FALSE : TRUE;
}



void *node_data(node *aNode)
{
	return aNode->data;
}



char *node_key(node *aNode)
{
	return aNode->key;
}


node *list_next(node *aNode)
{
	return aNode->next;
}



node *list_first(j9list *aList)
{
	return aList->first;
}


void list_dump(j9list *aList)
{
	node *tmpNode;
	PORT_ACCESS_FROM_PORT(aList->portLibrary);

	tmpNode = list_first(aList);
	while(tmpNode) {
		j9tty_printf( PORTLIB, "node (%s, 0x%08x)\n", (UDATA)node_key(tmpNode), (UDATA)node_data(tmpNode) );
		tmpNode = list_next(tmpNode);
	}
}




