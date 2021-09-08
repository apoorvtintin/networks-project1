/******************************************************************************
* echo_server.c                                                               *
*                                                                             *
* Description: This file contains the C source code for an echo server.  The  *
*              server runs on a hard-coded port and simply write back anything*
*              sent to it by connected clients.  It does not support          *
*              concurrent clients.                                            *
*                                                                             *
* Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                         *
*          Wolf Richter <wolf@cs.cmu.edu>                                     *
*                                                                             *
*******************************************************************************/

#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "parse.h"

#define LISTEN_PORT 15441
#define BUF_SIZE 4096

const char bad_request[] = {"HTTP/1.1 400 Bad Request\r\n\r\n"};
#define BAD_REQUEST_SIZE 28
int yydebug = STDOUT_FILENO;

int close_socket(int sock)
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}

int handle_rx(char *buf, int bufsize) {
	int index;
	//Parse the buffer to the parse function. You will need to pass the socket fd and the buffer would need to
	//be read from that fd
	Request *request = parse(buf,bufsize,0);

	if(request == NULL) {
		// parsing failed
		return -1;
	}
	//Just printing everything
	printf("Http Method %s\n",request->http_method);
	printf("Http Version %s\n",request->http_version);
	printf("Http Uri %s\n",request->http_uri);
	printf("number of Request Headers %d\n", request->header_count);
	for(index = 0;index < request->header_count;index++){
		printf("Request Header\n");
		printf("Header name %s Header Value %s\n",request->headers[index].header_name,request->headers[index].header_value);
	}
	free(request->headers);
	free(request);
	return 0;
}



int main(int argc, char* argv[])
{
    int listen_sock, client_sock;
    ssize_t readret;
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr;
    char buf[BUF_SIZE];
	int yes=1;

	if(argc < 2) {
		fprintf(stderr, "Failed creating socket.\n");
		return -1;
	}
	int listen_port = atoi(argv[1]);

    fprintf(stdout, "----- Echo Server -----\n");
    
    /* all networked programs must create a socket */
    if ((listen_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "Failed creating socket.\n");
        return EXIT_FAILURE;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(listen_port);
    addr.sin_addr.s_addr = INADDR_ANY;

	setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(listen_sock, (struct sockaddr *) &addr, sizeof(addr)))
    {
        close_socket(listen_sock);
        fprintf(stderr, "Failed binding socket.\n");
        return EXIT_FAILURE;
    }

	fd_set master_set;	// master file descriptor list
    fd_set tenp_set;	// temp file descriptor list for select()
	int fdrange;		// maximum file descriptor number

	FD_ZERO(&master_set);
	FD_ZERO(&tenp_set);

    if (listen(listen_sock, 5))
    {
        close_socket(listen_sock);
        fprintf(stderr, "Error listening on socket.\n");
        return EXIT_FAILURE;
    }

	// add it to the master set and set range
	FD_SET(listen_sock, &master_set);
	fdrange = listen_sock;

	while(1) {

		tenp_set = master_set;

		if(select(fdrange+1, &tenp_set, NULL, NULL, NULL) == -1) {
			// select failed
			printf("select call failed\n");
			close_socket(listen_sock);
			fprintf(stderr, "Error on select.\n");
			return EXIT_FAILURE;
		}

		// iterate over all to see which one has new data
		for(int i = 0; i < fdrange + 1; i++) {
			if (FD_ISSET(i, &tenp_set)) {
				// some activity, check it out
				
				if(i == listen_sock) {
					// we have a new connection handle it'
					cli_size = sizeof(cli_addr);
					if ((client_sock = accept(listen_sock, (struct sockaddr *) &cli_addr,
												&cli_size)) == -1)
					{
						close(listen_sock);
						fprintf(stderr, "Error accepting connection.\n");
						return EXIT_FAILURE;
					} else {
						// successfully accepted a new connection, add it to
						// and master set
						printf("accepted a new connection\n");
						if(client_sock > fdrange) {
							fdrange = client_sock;
						}

						FD_SET(client_sock, &master_set);
					}
				} else  {
					// new data from an existing client
					readret = 0;

					if((readret = recv(i, buf, BUF_SIZE, 0)) >= 1)
					{
						printf("recieved data\n");
						write(STDOUT_FILENO, buf, readret);
						printf("\n");
						// handle data from client
						if(handle_rx(buf, readret) < 0) {
							// request is malformed
							if (send(i, bad_request, BAD_REQUEST_SIZE, 0) < 0) {
								
								close_socket(i);
								close_socket(listen_sock);
								fprintf(stderr, "Error sending to client.\n");
								return EXIT_FAILURE;
							}
							printf("sent bad request\n");
						} else {
							if (send(i, buf, readret, 0) != readret)
							{
								close_socket(i);
								close_socket(listen_sock);
								fprintf(stderr, "Error sending to client.\n");
								return EXIT_FAILURE;
							}
							printf("echoing request back\n");
							
						}
					} else {
						// client connection closed
						close_socket(i);
						FD_CLR(i, &master_set);
					}

					memset(buf, 0, BUF_SIZE);
				}
			}
		}
	}

	close_socket(listen_sock);
	return EXIT_SUCCESS;
}
