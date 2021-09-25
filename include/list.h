#ifndef _LIST_H_
#define _LIST_H_

#include <time.h>

#define BUF_SIZE 4096				// size of Liso Buffer 

typedef struct node {
	int sock;
	time_t elapsed;
	char buf[BUF_SIZE];
	int pipeline_flag;
	struct node *next;
} client;

client* search_client(int socket);
void add_client(client *c);
void delete_client(client *c);
client* check_timeout();
void reinsert_client(client *c);

#endif // _LIST_H_