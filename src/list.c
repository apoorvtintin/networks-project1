/**
 * @file list.c
 * @author Apoorv Gupta <apoorvgu@andrew.cmu.edu>
 * 
 * @brief A simple list for liso that stores a list of clients
 * it has. The list is ordered with timeout making it easy to find
 * timed out clients
 * 
 * @version 0.1
 * @date 2021-10-02
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "list.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>

// constants
#define LIST_DEBUG 1
#define TIMEOUT (10)

// globals
static client *root = NULL;
static client *end = NULL;
extern FILE* fp;

/**
 * @brief Search a client in the list
 * 
 * @param socket socket to search for
 * @return ** client* pointer to client if found, NULL otherwise
 */
client* search_client(int socket) {
	assert(root != NULL);
	client *temp = root; 

	while(temp != NULL) {
		
		if(temp->sock == socket) {
			return temp;
		}
		temp = temp->next;
	}
	
	return NULL;
}

/**
 * @brief add a client to the list
 * 
 * @param c pointer to client to add to the list
 * @return ** void 
 */
void add_client(client *c) {
	assert(c != NULL);

	c->elapsed = time(NULL);
	if(root == NULL) {
		// list empty
		root = c;
		c->next = NULL;
		end = c;

	} else {
		// list has elements
		// iterate to last one
		end->next = c;
		c->next = NULL;
		end = c;
	}

}

/**
 * @brief delete a client form the list
 * 
 * @param c pointer to client to delete from list
 * @return ** void 
 */
void delete_client(client *c) {
	assert(c != NULL);
	assert(root != NULL);

	client *temp = root; 

	if(root == c) {
		if(end == c) {
			end = end->next;
		}
		root = root->next;

	} else {
		while(temp->next != NULL) {
			
			if(temp->next == c) {
				temp->next = c->next;

				if(end == c) {
					end = temp;
				}

				c->next = NULL;
				return;
			}

			temp = temp->next;
		}
	}

	c->next = NULL;
}

/**
 * @brief Check timeout for all clients in the list
 * 
 * @return ** client* a client which has timed out
 */
client* check_timeout() {
	
	client *temp = root; 

	time_t cur = time(NULL);

	while(temp != NULL) {
		
		if(cur - temp->elapsed > TIMEOUT) {
			delete_client(temp);
			return temp;
		} else {
			return NULL;
		}
		temp = temp->next;
	}

	return NULL;
}

/**
 * @brief Reinsert client in the list i.e. reset timeout
 * 
 * @param c client to reinsert
 * @return ** void 
 */
void reinsert_client(client *c) {
	delete_client(c);
	add_client(c);
}