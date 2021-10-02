/**
 * @file liso.h
 * @author Apoorv Gupta <apoorvgu@andrew.cmu.edu>
 * 
 * @brief Main header file LISO, all public functions are declared here 
 * 
 * @version 0.1
 * @date 2021-10-02
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#ifndef _LISO_H_
#define _LISO_H_

#include "lisodebug.h"
#include "parse.h"
#include <assert.h>
#include <strings.h>
#include "list.h"
#include <arpa/inet.h>

// MACROS

#define BAD_REQUEST_SIZE 28			// size of a bad request error 400
#define PATH_MAX 4096

#define ENV_SIZE 1024
#define ENV_NUM 23

enum liso_errors {
	LISO_ERROR = -1,
	LISO_SUCCESS = 0,
	LISO_LOAD_FAILED = 1,
	LISO_MEM_FAIL = 2,
	LISO_UNSUPPORTED_METHOD = 3,
	LISO_TIMEOUT =4,
	LISO_BAD_VERSION_NUMBER=5,
	LISO_BAD_REQUEST =6,
	LISO_CLOSE_CONN =7,
	LISO_CGI_START = 8,
	LISO_CGI_END =9,
};

char* generate_reply(Request *req, char *buf, int bufsize, int *resp_size);
char* generate_error(int error, int *resp_size, Request *req);
int get_full_request_len(Request *req);
int get_conn_header(Request *req);
int sanity_check(Request *req);
int start_process_cgi(Request *req, client *c);
int wrap_process_cgi(client *cgi_client);
int get_http_env(char* env[], Request *req, char remote_address[], int port); 
void print_parse_req(Request *request);
void print_req_buf(char *buf, int len);
int get_header_index(Http_header *header, const char* header_name, int header_count);

#endif // _LISO_H_