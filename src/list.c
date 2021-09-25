#include "list.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>

#define LIST_DEBUG 1
#define TIMEOUT (10)

client *root = NULL;
client *end = NULL;

bool detect_rep(int socket) {
#ifdef LIST_DEBUG
	client *temp = root; 

	while(temp != NULL) {
		
		if(temp->sock == socket) {
			return true;
		}
		temp = temp->next;
	}
#endif
	return false;
}

int count_nodes() {
	client *temp = root; 
	int count = 0;

	while(temp != NULL) {
		temp = temp->next;
		count++;
	}

	return count;
}

client* search_client(int socket) {
	// printf("searching clieet sock %d\n", socket);
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

void add_client(client *c) {
	assert(c != NULL);
	assert(detect_rep(c->sock) == false);
	// int prev_c = count_nodes();
	// printf("adding clieet sock %d\n", c->sock);
	c->elapsed = time(NULL);
	if(root == NULL) {
		// list empty
		root = c;
		c->next = NULL;
		end = c;

	} else {
		// list has elements
		// iterate to last one
			// client *temp = root; 
			// root = c;
			// c->next = temp;
		end->next = c;
		c->next = NULL;
		end = c;
	}

	// assert(prev_c == count_nodes() -1);
}

void delete_client(client *c) {
	// printf("deleting clieet sock %d\n", c->sock);
	assert(c != NULL);
	assert(root != NULL);

	// int prev_c = count_nodes();

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
	// assert(prev_c == count_nodes() +1);
}

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

void reinsert_client(client *c) {
	delete_client(c);
	add_client(c);
}