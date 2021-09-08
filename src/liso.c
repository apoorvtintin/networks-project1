/**
 * @file liso.c
 * @author Apoorv Gupta <apoorvgu@andrew.cmu.edu>
 * 
 * @brief Implementation of HTTP parsing from scratch used by the LISO server.
 * 
 * The LISO server uses YACC and LEX to parse HTTP requests, LEX is used to 
 * tokenise the HTTP request and YACC is used for conditional parsing of 
 * the HTTP request and populate internal LISO request fields with Requests
 * and headers. LISO replies to invalid malformed requests appropriately.
 * 
 * @version 0.1
 * @date 2021-09-08
 * 
 */

#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "parse.h"
#include "lisodebug.h"

// MACROS
#define BUF_SIZE 4096				// size of Liso Buffer 
#define BAD_REQUEST_SIZE 28			// size of a bad request error 400

// bad request response
const char bad_request[] = {"HTTP/1.1 400 Bad Request\r\n\r\n"};

/**
 * @brief Print buffer
 * 
 * @param buf buffer to be printed
 * @param len length of the buffer
 * @return ** void 
 */
void print_req_buf(char *buf, int len) {
	if(buf == NULL) {
		return;
	}
	LISOPRINTF("recieved data\n");
#ifdef LISODEBUG
	write(STDOUT_FILENO, buf, len);
#endif
	LISOPRINTF("\n");
}

/**
 * @brief Helper Function to close socket
 * 
 * @param sock socket to be closed
 * @return ** int 0 on success nonzero otherwise
 */
int close_socket(int sock)
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}

/**
 * @brief Handle a received packet
 * 
 * @param buf buffer containing the packet
 * @param bufsize size data in the buffer
 * @return ** int >= 0 on success negative otherwise 
 */
Request* handle_rx(char *buf, int bufsize) {

	if(buf == NULL || bufsize <= 0)  {
		// invalid arguments
		return NULL;
	}

	int index;
	//Parse the buffer to the parse function.
	Request *request = parse(buf,bufsize,0);

	if(request == NULL) {
		// parsing failed
		return NULL;
	}

	//Print the parsed request for DEBUG
	LISOPRINTF("Http Method %s\n",request->http_method);
	LISOPRINTF("Http Version %s\n",request->http_version);
	LISOPRINTF("Http Uri %s\n",request->http_uri);
	LISOPRINTF("number of Request Headers %d\n", request->header_count);
	for(index = 0;index < request->header_count;index++){
		LISOPRINTF("Request Header\n");
		LISOPRINTF("Header name %s Header Value %s\n",request->headers[index].header_name,request->headers[index].header_value);
	}

	free(request->headers);
	free(request);

	// SUCCESS
	return request;
}

/**
 * @brief Initialie the listen socket for accepting incoming client connections
 * 
 * @param listen_port port to listen on
 * @param addr struct sockaddr_in to be associated with listen_socket
 * @return ** int 0 in success, nonzero otherwise
 */
int initialize_listen_socket(int listen_port, struct sockaddr_in *addr) {

	int listen_sock;

    /* create our listen socket */
    if ((listen_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "Failed creating socket.\n");
        return -1;
    }

    addr->sin_family = AF_INET;
    addr->sin_port = htons(listen_port);
    addr->sin_addr.s_addr = INADDR_ANY;

	int yes=1;
	setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(listen_sock, (struct sockaddr*)addr, sizeof(struct sockaddr_in)))
    {
        close_socket(listen_sock);
        fprintf(stderr, "Failed binding socket.\n");
        return -1;
    }

    if (listen(listen_sock, 1000))
    {
        close_socket(listen_sock);
        fprintf(stderr, "Error listening on socket.\n");
        return -1;
    }

	return listen_sock;
}

/**
 * @brief generate reply for the request recieved and send appropriate 
 * response
 * 
 * @param client_socket socket of the client that sent the request
 * @param req Populated request with parsed values
 * @param buf reauest buffer
 * @param bufsize reauest buffer size
 * @return ** int 0 on success, nonzero otherwise 
 */
int generate_and_send_reply(int client_socket, Request *req, char *buf, int bufsize) {
	LISOPRINTF("echoing request back\n");
	print_req_buf(buf, bufsize);
	if (send(client_socket, buf, bufsize, 0) != bufsize)
	{
		fprintf(stderr, "Error sending to client.\n");
		return -1;
	}
	return 0;
}

/**
 * @brief send a error 400 malformed request
 * 
 * @param client_socket socket to send response at
 * @return ** int 0 on success, nonzero otherwise
 */
int send_bad_request_response(int client_socket) {
	if (send(client_socket, bad_request, BAD_REQUEST_SIZE, 0) < 0) {
		
		fprintf(stderr, "Error sending to client.\n");
		LISOPRINTF("sent bad request\n");
		return -1;
	}
	return 0;
}

/**
 * @brief Main driver function for LISO
 * 
 * This function initializes the liso server and starts it up
 * 
 * @param argc number of command line arguments
 * @param argv command line arguments
 * @return ** int 
 */
int main(int argc, char* argv[])
{
	// declarations
    int listen_sock, client_sock;
    ssize_t readret;
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr;
    char buf[BUF_SIZE];
	Request *req;


	// get listen port from command line arguments
	if(argc < 2) {
		fprintf(stderr, "Invalid arguments.\n");
		fprintf(stderr, "Usage ./lisod <port>\n");
		return -1;
	}
	int listen_port = atoi(argv[1]);

	if((listen_sock = initialize_listen_socket(listen_port, &addr)) < 0) {
		fprintf(stderr, "Initialize of listen socket failed.\n");
		return -1;
	}

	// initialize file descriptor set for select
	fd_set master_set;	// master file descriptor list
    fd_set tenp_set;	// temp file descriptor list for select()
	int fdrange;		// maximum file descriptor number
	FD_ZERO(&master_set);
	FD_ZERO(&tenp_set);
	// add it to the master set and set range
	FD_SET(listen_sock, &master_set);
	fdrange = listen_sock;

	// Main Server Loop
	while(1) {

		// copy master set to temporary set
		tenp_set = master_set;

		if(select(fdrange+1, &tenp_set, NULL, NULL, NULL) == -1) {
			// select failed
			fprintf(stderr,"select call failed\n");
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
						LISOPRINTF("accepted a new connection\n");
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
						// handle data from client
						if((req = handle_rx(buf, readret)) == NULL) {
							// request is malformed
							send_bad_request_response(i);
						} else {
							generate_and_send_reply(i, req, buf, readret);
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
