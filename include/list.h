/**
 * @file list.h
 * @author  Apoorv Gupta <apoorvgu@andrew.cmu.edu>
 * @brief declaration for the List utility for LISO
 * @version 0.1
 * @date 2021-10-02
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#ifndef _LIST_H_
#define _LIST_H_

#include <time.h>
#include <netinet/in.h>

#define BUF_SIZE 4096				// size of Liso Buffer 

typedef struct node {
	int sock;
	time_t elapsed;
	char *buf;
	int buf_left;
	int alloc_num;
	int pipeline_flag;
	char remote_address[INET_ADDRSTRLEN];
	int port;

	int is_pipe;
	struct node *cgi_host;
	struct node *next;
} client;

client* search_client(int socket);
void add_client(client *c);
void delete_client(client *c);
client* check_timeout();
void reinsert_client(client *c);

#endif // _LIST_H_