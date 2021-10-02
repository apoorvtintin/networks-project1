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
 * Liso runs as deamon and has a lockfile which allows only one deamon to run
 * at a time. CGI is also supported and can be accessed with /cgi/ in URI
 * 
 * @version 0.1
 * @date 2021-09-08
 * 
 */

#include <fcntl.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
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

// GLOBALS
char LISO_PATH[1024];
extern const char CONTENT_LEN_HEADER[];
FILE *fp;
char lock_file[1024];
char cgi_script[BUF_SIZE];


/**
 * @brief Print buffer
 * 
 * @param buf buffer to be printed
 * @param len length of the buffer
 * @return ** void 
 */
void print_req_buf(char *buf, int len)
{
	if (buf == NULL)
	{
		return;
	}
	LISOPRINTF(fp, "printing buffer of size %d\n", len);
#ifdef LISODEBUG
	fwrite(buf, 1, len, fp);
#endif
	LISOPRINTF(fp, "\n");
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
 * @brief Initialize and add a new client to the client list
 * 
 * @param client_sock socket of the client
 * @param pV4Addr sockaddr structure of the client
 * @return ** int 
 */
int add_new_client(int client_sock, struct sockaddr_in *pV4Addr)
{
	client *c = malloc(sizeof(client));
	if (c == NULL)
	{
		return LISO_MEM_FAIL;
	}
	c->sock = client_sock;
	c->pipeline_flag = false;
	c->cgi_host = NULL;

	struct in_addr ipAddr = pV4Addr->sin_addr;

	inet_ntop(AF_INET, &ipAddr, c->remote_address, INET_ADDRSTRLEN);
	c->port = ntohs(pV4Addr->sin_port);
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
int generate_and_send_reply(int client_socket, Request *req, char *buf, int bufsize)
{
	LISOPRINTF(fp, "Processing request \n");
	print_req_buf(buf, bufsize);

	int resp_size;
	char *resp_buf = generate_reply(req, buf, bufsize, &resp_size);

	// sending reply
	LISOPRINTF(fp, "Sending reply \n");
	print_req_buf(resp_buf, resp_size);

	if (send(client_socket, resp_buf, resp_size, 0) != resp_size)
	{
		fprintf(stderr, "Error sending to client.\n");
		perror("send recieved an error");
		free(resp_buf);
		return -1;
	}

	if (resp_buf != buf)
	{ // TODO REMOVE WHEN IMPLEMENTING POST
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
int send_error_response(int client_socket, int error, Request *req)
{

	int resp_size;
	char *bad_request = generate_error(error, &resp_size, req);

	LISOPRINTF(fp, " error response for socket %d\n", client_socket);
	print_req_buf(bad_request, resp_size);
	if (send(client_socket, bad_request, resp_size, 0) < 0)
	{

		fprintf(stderr, "Error sending to client.\n");
		LISOPRINTF(fp, "sent bad request\n");
		free(bad_request);
		return -1;
	}
	free(bad_request);
	return 0;
}

/**
 * @brief [DEBUG] print the parsed request from client
 * 
 * @param request parsed request from client
 * @return ** void 
 */
void print_parse_req(Request *request)
{
	//Print the parsed request for DEBUG
	LISOPRINTF(fp, "Http Method %s\n", request->http_method);
	LISOPRINTF(fp, "Http Version %s\n", request->http_version);
	LISOPRINTF(fp, "Http Uri %s\n", request->http_uri);
	LISOPRINTF(fp, "number of Request Headers %d\n", request->header_count);
	for (int index = 0; index < request->header_count; index++)
	{
		LISOPRINTF(fp, "Request Header\n");
		LISOPRINTF(fp, "Header name %s Header Value %s\n", request->headers[index].header_name, request->headers[index].header_value);
	}
}

/**
 * @brief Handle a received packet
 * 
 * @param buf buffer containing the packet
 * @param bufsize size data in the buffer
 * @return ** int >= 0 on success negative otherwise 
 */
int handle_rx(int client_socket, int *pipefd)
{

	client *c = search_client(client_socket);
	assert(c != NULL);

	if (c->cgi_host != NULL)
	{
		// this is a cgi response from script
		wrap_process_cgi(c);
		return LISO_CGI_END;
	}

	char *buf;
	int alloc_count = 1;
	int read_count = 0;
	int readret;
	*pipefd = -1;

	buf = malloc(BUF_SIZE);
	if (buf == NULL)
	{
		return LISO_MEM_FAIL;
	}

	// read everything in OS socket buffer
	do
	{
		readret = recv(client_socket, buf + read_count, BUF_SIZE, 0);
		read_count += readret;

		if (readret >= BUF_SIZE)
		{
			// reallocate and try to read everything in the socket
			alloc_count++;
			LISOPRINTF(fp, "reallocating buffer to size %d \n", BUF_SIZE * alloc_count);
			buf = realloc(buf, BUF_SIZE * alloc_count);
			assert(buf != NULL);
		}

	} while (readret >= BUF_SIZE); // no more data in the socket

	if (read_count <= 0)
	{
		free(buf);
		return LISO_CLOSE_CONN;
	}

	// begin processing buffer
	int cur_to_end_size = read_count;
	bool pipelined;
	char *cur_buf = buf;
	int conn_close = LISO_SUCCESS;
	LISOPRINTF(fp, "Printing whole request(s) \n");
	print_req_buf(buf, read_count);

	int req_counter = 0;
	do
	{ // respond to all pipelined requests in buffer
		req_counter++;
		pipelined = false;
		assert(cur_to_end_size > 0);
		// we have all the data that the buffer had
		//Parse the buffer to the parse function.
		Request *req = parse(cur_buf, cur_to_end_size, 0);

		// handle data from client
		if (req == NULL)
		{
			// request is malformed
			LISOPRINTF(fp, "request is malformed\n");
			send_error_response(client_socket, LISO_BAD_REQUEST, req);
			conn_close = LISO_SUCCESS;
			break;
		}

		int error = sanity_check(req);
		int rlen = get_full_request_len(req);

		// get content len of request
		int clen = 0;
		int index = get_header_index(req->headers, CONTENT_LEN_HEADER, req->header_count);

		if (index != LISO_ERROR)
		{
			clen = atoi(req->headers[index].header_value);
		}

		req->message = malloc(clen + 1);
		memcpy(req->message, cur_buf + rlen, clen);
		req->message[clen] = '\0';
		req->message_len = clen;

		// process request
		if (error != LISO_SUCCESS)
		{
			send_error_response(client_socket, error, req);
		}
		else
		{

			LISOPRINTF(fp, "request is good and will be parsed\n");
			if (req->is_cgi)
			{
				// request is dynamic uri
				*pipefd = start_process_cgi(req, c);
				conn_close = LISO_CGI_START;
				free(req->headers);
				free(req->message);
				free(req);
				break;
				// wrap_process_cgi(*pipefd, client_socket);
			}
			else
			{
				generate_and_send_reply(client_socket, req, buf, rlen);
			}
		}

		if (get_conn_header(req) == LISO_CLOSE_CONN)
		{
			LISOPRINTF(fp, "GOt connection close for req number %d", req_counter);
			conn_close = LISO_CLOSE_CONN;
			free(req->headers);
			free(req->message);
			free(req);

			break;
		}

		// check if requests are pipelined

		if (rlen < cur_to_end_size)
		{
			// we have pipelined requests handle them
			pipelined = true;
			cur_to_end_size = cur_to_end_size - rlen;
			cur_buf = cur_buf + rlen;
			LISOPRINTF(fp, "Processing another pipelined request cur_to_end_size %d rlen %d, parse len %d\n", cur_to_end_size, rlen, req->request_len);
		}

		free(req->headers);
		free(req->message);
		free(req);

		LISOPRINTF(fp, "Processed %d pipelined requests", req_counter);
	} while (pipelined == true);

	reinsert_client(c);
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
int initialize_listen_socket(int listen_port, struct sockaddr_in *addr)
{

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

	int yes = 1;
	setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	setsockopt(listen_sock, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(int));

	/* servers bind sockets to ports---notify the OS they accept connections */
	if (bind(listen_sock, (struct sockaddr *)addr, sizeof(struct sockaddr_in)))
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
 * internal signal handler
 */
void signal_handler(int sig)
{
	switch (sig)
	{
	case SIGHUP:
		/* rehash the server */
		break;
	case SIGTERM:
		killpg(getpid(), SIGTERM);
		/* finalize and shutdown the server */
		exit(EXIT_SUCCESS);
		break;
	default:
		break;
		/* unhandled signal */
	}
}

/** 
 * internal function daemonizing the process
 */
int daemonize(char *lock_file)
{
	/* drop to having init() as parent */
	int i, lfp, pid = fork();
	char str[256] = {0};
	if (pid < 0)
		exit(EXIT_FAILURE);
	if (pid > 0)
		exit(EXIT_SUCCESS);

	setsid();

	for (i = getdtablesize(); i >= 0; i--)
		close(i);

	i = open("/dev/null", O_RDWR);
	dup(i); /* stdout */
	dup(i); /* stderr */
	umask(027);

	lfp = open(lock_file, O_RDWR | O_CREAT, 0640);

	if (lfp < 0)
		exit(EXIT_FAILURE); /* can not open */

	if (lockf(lfp, F_TLOCK, 0) < 0)
		exit(EXIT_SUCCESS); /* can not lock */

	/* only first instance continues */
	sprintf(str, "%d\n", getpid());
	write(lfp, str, strlen(str)); /* record pid to lockfile */

	signal(SIGCHLD, SIG_IGN); /* child terminate signal */

	signal(SIGHUP, signal_handler);	 /* hangup signal */
	signal(SIGTERM, signal_handler); /* software termination signal from kill */

	return EXIT_SUCCESS;
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
int main(int argc, char *argv[])
{
	// declarations
	int listen_sock, client_sock;
	socklen_t cli_size;
	struct sockaddr_in addr, cli_addr;
	char buf[BUF_SIZE];

	// get listen port from command line arguments
	if (argc < 6)
	{
		fprintf(stderr, "Invalid arguments.\n");
		fprintf(stderr, "Usage ./lisod <HTTP port> <log file> <lock file> <www folder> <CGI script path>\n");
		return -1;
	}
	int listen_port = atoi(argv[1]);

	strncpy(LISO_PATH, argv[4], strlen(argv[4]) + 1);

	daemonize(argv[3]);

	// initialize logfile
	fp = fopen(argv[2], "a+");
	if (fp == NULL)
	{

		perror("logfile open failed\n");
	}
	fflush(stdout);
	assert(fp != NULL);
	setvbuf(fp, (char *)NULL, _IONBF, 0);
	LISOPRINTF(fp, "started in logfile in %s\n", argv[2]);
	strncpy(cgi_script, argv[5], BUF_SIZE);

	// install sigpipe handler
	sigaction(SIGPIPE, &(struct sigaction){SIG_IGN}, NULL);

	if ((listen_sock = initialize_listen_socket(listen_port, &addr)) < 0)
	{
		fprintf(stderr, "Initialize of listen socket failed.\n");
		return -1;
	}

	LISOPRINTF(fp, "lisopath given is %s\n", LISO_PATH);

	// initialize file descriptor set for select
	fd_set master_set; // master file descriptor list
	fd_set tenp_set;   // temp file descriptor list for select()
	int fdrange;	   // maximum file descriptor number
	FD_ZERO(&master_set);
	FD_ZERO(&tenp_set);
	// add it to the master set and set range
	FD_SET(listen_sock, &master_set);
	fdrange = listen_sock;

	// Main Server Loop
	while (1)
	{

		// set up select time out
		struct timeval timeout;
		timeout.tv_sec = 10;
		// copy master set to temporary set
		tenp_set = master_set;

		if (select(fdrange + 1, &tenp_set, NULL, NULL, &timeout) == -1)
		{
			// select failed
			fprintf(stderr, "select call failed\n");
			close_socket(listen_sock);
			fprintf(stderr, "Error on select.\n");
			return EXIT_FAILURE;
		}

		// iterate over all to see which one has new data
		for (int i = 0; i < fdrange + 1; i++)
		{
			if (FD_ISSET(i, &tenp_set))
			{
				// some activity, check it out

				if (i == listen_sock)
				{
					// we have a new connection handle it'
					cli_size = sizeof(cli_addr);
					if ((client_sock = accept(listen_sock, (struct sockaddr *)&cli_addr,
											  &cli_size)) == -1)
					{
						close(listen_sock);
						fprintf(stderr, "Error accepting connection.\n");
						return EXIT_FAILURE;
					}
					else
					{
						// successfully accepted a new connection, add it to
						// and master set
						LISOPRINTF(fp, "accepted a new connection\n");
						add_new_client(client_sock, (struct sockaddr_in *)&cli_addr);

						if (client_sock > fdrange)
						{
							fdrange = client_sock;
						}

						FD_SET(client_sock, &master_set);
					}
				}
				else
				{

					int pipe_fd;
					int rx_ret = handle_rx(i, &pipe_fd);
					// new data from an existing client
					if (rx_ret == LISO_CLOSE_CONN)
					{
						// client connection closed
						client *c = search_client(i);
						close_socket(i);
						FD_CLR(i, &master_set);
						delete_client(c);
						free(c);
						LISOPRINTF(fp, "closed connection %d\n", i);
					}
					else if (rx_ret == LISO_CGI_START)
					{

						// we have a cgi script running add
						// pipe to select FDs
						if (pipe_fd > fdrange)
						{
							fdrange = pipe_fd;
						}

						FD_SET(pipe_fd, &master_set);
					}
					else if (rx_ret == LISO_CGI_END)
					{
						// cgi script ended remove from select
						FD_CLR(i, &master_set);
					}

					memset(buf, 0, BUF_SIZE);
				}
			}
		}

		// check for timed out sockets
		LISOPRINTF(fp, "going to check for timeouts\n");
		client *timeout_client;
		while ((timeout_client = check_timeout()) != NULL)
		{
			send_error_response(timeout_client->sock, LISO_TIMEOUT, NULL);
			close_socket(timeout_client->sock);
			FD_CLR(timeout_client->sock, &master_set);
			LISOPRINTF(fp, "timeeout for socket %d\n", timeout_client->sock);
			free(timeout_client);
		}
	}

	close_socket(listen_sock);
	return EXIT_SUCCESS;
}
