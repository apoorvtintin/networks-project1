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
#include "liso.h"
#include "parse.h"
#include <netinet/tcp.h>
#include "list.h"
#include <stdbool.h>
#include <signal.h>

char LISO_PATH[1024];
extern const char CONTENT_LEN_HEADER[];

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
	LISOPRINTF("printing buffer of size %d\n", len);
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

int add_new_client(int client_sock) {
	client *c = malloc(sizeof(client));
	if(c == NULL) {
		return LISO_MEM_FAIL;
	}
	c->sock = client_sock;
	c->pipeline_flag = false;
	add_client(c);

	return LISO_SUCCESS;
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
	LISOPRINTF("Processing request \n");
	print_req_buf(buf, bufsize);

	int resp_size;
	char* resp_buf = generate_reply(req, buf, bufsize, &resp_size);

	// sending reply
	LISOPRINTF("Sending reply \n");
	print_req_buf(resp_buf, resp_size);
	
	if (send(client_socket, resp_buf, resp_size, 0) != resp_size)
	{
		fprintf(stderr, "Error sending to client.\n");
		perror("send recieved an error");
		free(resp_buf);
		return -1;
	}

	if(resp_buf != buf) { // TODO REMOVE WHEN IMPLEMENTING POST
		free(resp_buf);
	}
	return 0;
}

/**
 * @brief send a error 400 malformed request
 * 
 * @param client_socket socket to send response at
 * @return ** int 0 on success, nonzero otherwise
 */
int send_error_response(int client_socket, int error, Request *req) {

	int resp_size;
	char* bad_request = generate_error(error, &resp_size, req);

	LISOPRINTF(" error response for socket %d\n", client_socket);
	print_req_buf(bad_request, resp_size);
	if (send(client_socket, bad_request, resp_size, 0) < 0) {
		
		fprintf(stderr, "Error sending to client.\n");
		LISOPRINTF("sent bad request\n");
		free(bad_request);
		return -1;
	}
	free(bad_request);
	return 0;
}

void print_parse_req(Request *request) {
	//Print the parsed request for DEBUG
	LISOPRINTF("Http Method %s\n",request->http_method);
	LISOPRINTF("Http Version %s\n",request->http_version);
	LISOPRINTF("Http Uri %s\n",request->http_uri);
	LISOPRINTF("number of Request Headers %d\n", request->header_count);
	for(int index = 0;index < request->header_count;index++){
		LISOPRINTF("Request Header\n");
		LISOPRINTF("Header name %s Header Value %s\n",request->headers[index].header_name,request->headers[index].header_value);
	}
}

/**
 * @brief Handle a received packet
 * 
 * @param buf buffer containing the packet
 * @param bufsize size data in the buffer
 * @return ** int >= 0 on success negative otherwise 
 */
int handle_rx(int client_socket) {

	client *c = search_client(client_socket);
	assert(c != NULL);

	char* buf = malloc(BUF_SIZE);
	if(buf == NULL) {
		return LISO_MEM_FAIL;
	}

	int alloc_count = 1;
	int read_count = 0;
	int readret;

	do {
		readret = recv(client_socket, buf + read_count, BUF_SIZE, 0);
		read_count += readret;

		if(readret >= BUF_SIZE) {
			// reallocate and try to read everything in the socket
			alloc_count++;
			LISOPRINTF("reallocating buffer to size %d \n", BUF_SIZE * alloc_count);
			buf = realloc(buf, BUF_SIZE * alloc_count);
			assert(buf != NULL);
		}

	} while(readret >= BUF_SIZE); // no more data in the socket

	if(read_count <= 0) {
		free(buf);
		return LISO_CLOSE_CONN;
	}

	int cur_to_end_size = read_count;
	bool pipelined;
	char* cur_buf = buf;
	int conn_close = LISO_SUCCESS;
	LISOPRINTF("Printing whole request(s) \n");
	print_req_buf(buf, read_count);

	int req_counter = 0;
	do { // respond to all pipelined requests in buffer
		req_counter++;
		pipelined = false;
		assert(cur_to_end_size > 0);
		// we have all the data that the buffer had 
		//Parse the buffer to the parse function.
		Request *req = parse(cur_buf,cur_to_end_size,0);

		// handle data from client
		if(req == NULL) {
			// request is malformed
			LISOPRINTF("request is malformed\n");
			send_error_response(client_socket, LISO_BAD_REQUEST, req);
			conn_close = LISO_SUCCESS;
			break;
		}

		int error = sanity_check(req);
		int rlen = get_full_request_len(req);

		if(error != LISO_SUCCESS) {
			send_error_response(client_socket, error, req);
		} else {

			LISOPRINTF("request is good and will be parsed\n");
			generate_and_send_reply(client_socket, req, buf, rlen);
		}

		if(get_conn_header(req) == LISO_CLOSE_CONN) {
			LISOPRINTF("GOt connection close for req number %d", req_counter);
			conn_close = LISO_CLOSE_CONN;
			free(req->headers);
			free(req);
			break;
		}

		// check if requests are pipelined

		if(rlen < cur_to_end_size) {
			// we have pipelined requests handle them
			pipelined = true;
			cur_to_end_size = cur_to_end_size - rlen;
			cur_buf = cur_buf + rlen;
			LISOPRINTF("Processing another pipelined request cur_to_end_size %d rlen %d, parse len %d\n" ,cur_to_end_size, rlen, req->request_len);
		}

		free(req->headers);
		free(req);

		LISOPRINTF("Processed %d pipelined requests", req_counter);
	} while(pipelined == true);

	// SUCCESS
	free(buf);
	return conn_close;
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
	setsockopt(listen_sock, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(int));

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

// int init_liso_storage() {
// 	// initialise LISO path
// 	if(readlink("/proc/self/exe", LISO_PATH, PATH_MAX)) {
// 		perror("readlink");
// 		return LISO_LOAD_FAILED;
// 	} else {
// 		LISOPRINTF("absolute path of liso is %s", LISO_PATH);
// 		snprintf(LISO_PATH + strlen(LISO_PATH), PATH_MAX - strlen(LISO_PATH), "/%s/", "static_site");
// 		LISOPRINTF("final storage path is %s")
// 	}

// 	return LISO_SUCCESS;
// }

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
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr;
    char buf[BUF_SIZE];

	// install sigpipe handler
	sigaction(SIGPIPE, &(struct sigaction){SIG_IGN}, NULL);

	// get listen port from command line arguments
	if(argc < 6) {
		fprintf(stderr, "Invalid arguments.\n");
		fprintf(stderr, "Usage ./lisod <HTTP port> <log file> <lock file> <www folder> <CGI script path>\n");
		return -1;
	}
	int listen_port = atoi(argv[1]);

	if((listen_sock = initialize_listen_socket(listen_port, &addr)) < 0) {
		fprintf(stderr, "Initialize of listen socket failed.\n");
		return -1;
	}

	strncpy(LISO_PATH, argv[4], strlen(argv[4]) +1);
	LISOPRINTF("lisopath given is %s\n", LISO_PATH);
	// LISOPRINTF("argv 0 %s\n", argv[0]);
	// LISOPRINTF("argv 1 %s\n", argv[1]);
	// LISOPRINTF("argv 2 %s\n", argv[2]);
	// LISOPRINTF("argv 3 %s\n", argv[3]);
	// LISOPRINTF("argv 4 %s\n", argv[4]);
	// LISOPRINTF("argv 5 %s\n", argv[5]);
	// LISOPRINTF("argv 6 %s\n", argv[6]);
	// if(init_liso_storage() != LISO_SUCCESS) {
	// 	// storage init fail
	// 	LISOPRINTF("Liso storage init failed");
	// 	return -1;
	// }

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

		// set up select time out
		struct timeval timeout;
		timeout.tv_sec = 10;
		// copy master set to temporary set
		tenp_set = master_set;

		if(select(fdrange+1, &tenp_set, NULL, NULL, &timeout) == -1) {
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

						add_new_client(client_sock);

						if(client_sock > fdrange) {
							fdrange = client_sock;
						}

						FD_SET(client_sock, &master_set);
					}
				} else  {
					// new data from an existing client
					if(handle_rx(i) == LISO_CLOSE_CONN) {
						// client connection closed
						close_socket(i);
						FD_CLR(i, &master_set);
						client *c = search_client(i);
						delete_client(c);
						free(c);
						LISOPRINTF("closed connection %d\n", i);
					}

					memset(buf, 0, BUF_SIZE);
				}
			}
		}

		// check for timed out sockets
		LISOPRINTF("going to check for timeouts\n");
		client *timeout_client;
		while((timeout_client = check_timeout()) != NULL) {
			send_error_response(timeout_client->sock, LISO_TIMEOUT, NULL);
			close_socket(timeout_client->sock);
			FD_CLR(timeout_client->sock, &master_set);
			LISOPRINTF("timeeout for socket %d\n", timeout_client->sock);
			free(timeout_client);
		}

	}

	close_socket(listen_sock);
	return EXIT_SUCCESS;
}
