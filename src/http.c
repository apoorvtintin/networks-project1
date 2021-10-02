/**
 * @file http.c
 * @author Apoorv Gupta <apoorvgu@andrew.cmu.edu>
 * @brief 
 * Contains implementation for the generating HTTP respnse by
 * LISO 
 * 
 * @version 0.1
 * @date 2021-09-24
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "liso.h"
#include "sys/stat.h"
#include <stdio.h>
#include <time.h>
#include "list.h"

// Globals
extern FILE* fp;
extern int freed;

// Constants
#define SIZE_STRING_BUF_SIZE 50
const int HEADER_COUNT_INCREMENT =  5;
char* ENVP[] = {
                    "CONTENT_LENGTH=",
                    "CONTENT_TYPE=",
                    "GATEWAY_INTERFACE=",
					"PATH_INFO=",
                    "QUERY_STRING=",
                    "REMOTE_ADDR=",
                    "REQUEST_METHOD=",
					"REQUEST_URI=",
                    "SCRIPT_NAME=",
                    "SERVER_PORT=",
                    "SERVER_PROTOCOL=",
                    "SERVER_SOFTWARE=",
                    "HTTP_ACCEPT=",
                    "HTTP_REFERER=",
                    "HTTP_ACCEPT_ENCODING=",
                    "HTTP_ACCEPT_LANGUAGE=",
                    "HTTP_ACCEPT_CHARSET=",
					"HTTP_HOST=",
                    "HTTP_COOKIE=",
                    "HTTP_USER_AGENT=",
                    "HTTP_CONNECTION=",
                    NULL
               };

// HTTP tokens
const char version[] = {"HTTP/1.1"};
const char GET[] = {"Get"};
const char HEAD[] = {"Head"};
const char POST[] = {"Post"};
const char SERVER_HEADER[] = {"Server"};
const char LISO_NAME[] = {"liso/1.0"};

// HEADER Types
const char MIME_HEADER[] = {"Content-Type"};
const char CONTENT_LEN_HEADER[] = {"Content-Length"};
const char DATE_HEADER[] = {"Date"};
const char LAST_MODIFIED_HEADER[] = {"Last-Modified"};
const char CONNECTION_HEADER[] = {"Connection"};
const char HOST_HEADER[] = {"Host"};
const char ACCEPT[] = {"Accept"};
const char REFERER[] = {"Referer"};
const char ACCEPT_ENCODING[] = {"Accept-Encoding"};
const char ACCEPT_LANGUAGE[] = {"Accept-Language"};
const char ACCEPT_CHARSET[] = {"Accept-Charset"};
const char COOKIE[] = {"Cookie"};

const char CLOSE[] = {"close"};
const char KEEP_ALIVE[] = {"keep-alive"};

const char Non_descript_data_MIME[] = "application/octet-stream";

const char* const MIME[] =  {"image/jpeg", 
							"image/gif", 
							"image/png", 
							"application/javascript",
							"application/json",
							"text/css",
							"text/html",
							"text/plain",
							0};

const char* const TYPES[] = {".jpeg",
							".gif",
							".png",
							".js",
							".json",
							".css",
							".html",
							".txt",
							0};

const int MIME_NUM_TYPES = 8;

// ERRORS
/**
 * We support five HTTP 1.1 error codes: 400, 404, 408, 501, and 505. 
 * 404 is for files not found; 408 is for connection timeouts; 
 * 501 is for unsupported methods; 505 is for bad version numbers. 
 * Everything else can be handled with 400.
 */

const char STATUS_400[] = {"400 Bad Request"};
const char STATUS_404[] = {"404 Not Found"};
const char STATUS_408[] = {"408 Connection timeout"};
const char STATUS_501[] = {"501 Unsupported method"};
const char STATUS_505[] = {"505 Bad version number"};

const char STATUS_200[] = {"200 OK"};

// liso storage Path
extern char LISO_PATH[PATH_MAX];

/**
 * @brief add header to the response
 * 
 * @param resp pointer to response structure
 * @param name name of the header
 * @param value value of the header
 * @return ** int LISO_SUCCESS on success, error otherwise 
 */
int add_header(Response *resp, const char* name, const char* value) {
	assert(name != NULL);
	assert(value != NULL);
	assert(resp != NULL);

	if(resp->header_allocated == resp->header_count) {
		// we need to allocate more headers
		resp->headers = realloc(resp->headers, sizeof(Http_header) * (resp->header_count + 5));
		if(resp->headers == NULL) {
			// memory allocation failed
			return LISO_MEM_FAIL;
		}

		// allocated 5 more headers in a batch
		resp->header_allocated += 5;
	}

	strncpy(resp->headers[resp->header_count].header_name, name, strlen(name) +1);
	strncpy(resp->headers[resp->header_count].header_value, value, strlen(value)+ 1);
	resp->header_count++;

	return LISO_SUCCESS;
}

/**
 * @brief Add the content lenght header
 * 
 * @param resp response to add to
 * @param size content lenght
 * @return ** int LISO_SUCCESS on success, error otherwise 
 */
int add_content_length(Response *resp, size_t size) {
	char buf[SIZE_STRING_BUF_SIZE];
	snprintf(buf, 20, "%ld", size);

	return add_header(resp, CONTENT_LEN_HEADER, buf);
}

/**
 * @brief Add the time header
 * 
 * @param resp response to add to
 * @return ** int LISO_SUCCESS on success, error otherwise 
 */
int add_time(Response *resp) {
	char buf[1000];
	time_t now = time(0);
	struct tm tm = *gmtime(&now);
	strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S %Z", &tm);

	return add_header(resp, DATE_HEADER, buf);
}

/**
 * @brief Add the last modified header
 * 
 * @param resp response to add to
 * @param tv time last modified
 * @return ** int  LISO_SUCCESS on success, error otherwise
 */
int add_last_modified(Response *resp, struct timespec *tv) {

	char buf[1000];
	time_t now = (time_t)tv->tv_sec;
	struct tm tm = *gmtime(&now);
	strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S %Z", &tm);

	return add_header(resp, LAST_MODIFIED_HEADER, buf);
}

/**
 * @brief Add the file type header
 * 
 * @param path path of the file
 * @param resp response to add to
 * @return ** int LISO_SUCCESS on success, error otherwise
 */
int add_mime_extension(char *path, Response *resp) {
	char *pch=strrchr(path,'.');

	char extension[10];
	strncpy(extension, pch, 10);
	LISOPRINTF(fp,"extension is %s", extension);

	for(int i = 0; TYPES[i] != NULL; i++) {
		if(strncasecmp(extension, TYPES[i], strlen(TYPES[i])) == 0) {
			// matching MIME extension found
			return add_header(resp, MIME_HEADER, MIME[i]);
		}
	}

	return add_header(resp, MIME_HEADER, Non_descript_data_MIME);
}

/**
 * @brief Get the header index object
 * 
 * @param header pointer to header array
 * @param header_name name of the header
 * @param header_count total headers in array
 * @return ** int index if found error otherwise
 */
int get_header_index(Http_header *header, const char* header_name, int header_count) {

	int index;
	for(index = 0; index < header_count; index++) {
		if(strncasecmp(header[index].header_name, header_name, strlen(header_name)) == 0) {
			LISOPRINTF(fp,"%s found content lenght \n", __func__);
			return index;
		}
	}

	return LISO_ERROR;
}

/**
 * @brief Get the header object
 * 
 * @param req request to get it from
 * @param name name of the ehader
 * @return ** char* pointer to header value, NULL otherwise
 */
char* get_header(Request *req, const char* name) {
	int index = get_header_index(req->headers, name, req->header_count);
	if(index != LISO_ERROR) {
		return req->headers[index].header_value;
	}
	return NULL;
}

/**
 * @brief Get the http env object created from the client request
 * 
 * @param env env array to fill
 * @param req request recieved
 * @param remote_address address of client
 * @param port port of client
 * @return ** int LISO_SUCCESS on success
 */
int get_http_env(char* env[], Request *req, char remote_address[], int port) {
	int count = 0;
	char* header;

	for(int i = 0; i < ENV_NUM -1; i++) {
		env[i] = malloc(ENV_SIZE);
		memset(env[i], 0, ENV_SIZE);
		assert(env[i] != NULL);
	}

	// content len
	header = get_header(req, CONTENT_LEN_HEADER);
	if(header != NULL) {
		snprintf(env[count], ENV_SIZE, "%s%s", ENVP[count], header);
	} else {
		snprintf(env[count], ENV_SIZE, "%s", ENVP[count]);
	}
	count++;

	// content type
	header = get_header(req, MIME_HEADER);
	if(header != NULL) {
		snprintf(env[count], ENV_SIZE, "%s%s", ENVP[count], header);
	} else {
		snprintf(env[count], ENV_SIZE, "%s", ENVP[count]);
	}
	count++;

	// "GATEWAY_INTERFACE=CGI/1.1"
	snprintf(env[count], ENV_SIZE, "%sCGI/1.1", ENVP[count]);
	count++;

	// PATH_INFO=
	char path_info[1024];
	memset(path_info, 0, 1024);
	header = strchr(req->http_uri, '?');
	if(header != NULL) {
		strncpy(path_info, req->http_uri+4, header- req->http_uri -4);
	} else {
		strncpy(path_info, req->http_uri, 1024);
	}
	snprintf(env[count], ENV_SIZE, "%s%s", ENVP[count], path_info);
	count++;

	// query string
	header = strchr(req->http_uri, '?');
	if(header != NULL) {
		snprintf(env[count], ENV_SIZE, "%s%s", ENVP[count], header);
	} else {
		snprintf(env[count], ENV_SIZE, "%s", ENVP[count]);
	}
	count++;


	// remote address
	snprintf(env[count], ENV_SIZE, "%s%s", ENVP[count], remote_address);
	count++;

	// http method
	snprintf(env[count], ENV_SIZE, "%s%s", ENVP[count], req->http_method);
	count++;

	// request uri
	snprintf(env[count], ENV_SIZE, "%s%s", ENVP[count], req->http_uri);
	count++;

	// script name
	char script_name[1024];
	memset(script_name, 0, 1024);
	header = strchr(req->http_uri, '?');
	if(header != NULL) {
		strncpy(script_name, req->http_uri, header - req->http_uri);
	} else {
		strncpy(script_name, req->http_uri, 1024);
	}
	snprintf(env[count], ENV_SIZE, "%s%s", ENVP[count], script_name);
	count++;

	// server_port
	snprintf(env[count], ENV_SIZE, "%s%d", ENVP[count], port);
	count++;

	// server_protocol
	snprintf(env[count], ENV_SIZE, "%s%s", ENVP[count], version);
	count++;

	// server_software
	snprintf(env[count], ENV_SIZE, "%s%s", ENVP[count], LISO_NAME);
	count++;

	// http accept
	header = get_header(req, ACCEPT);
	if(header != NULL) {
		snprintf(env[count], ENV_SIZE, "%s%s", ENVP[count], header);
	} else {
		snprintf(env[count], ENV_SIZE, "%s", ENVP[count]);
	}
	count++;

	// http REFERER
	header = get_header(req, REFERER);
	if(header != NULL) {
		snprintf(env[count], ENV_SIZE, "%s%s", ENVP[count], header);
	} else {
		snprintf(env[count], ENV_SIZE, "%s", ENVP[count]);
	}
	count++;

	// http ACCEPT_ENCODING
	header = get_header(req, ACCEPT_ENCODING);
	if(header != NULL) {
		snprintf(env[count], ENV_SIZE, "%s%s", ENVP[count], header);
	} else {
		snprintf(env[count], ENV_SIZE, "%s", ENVP[count]);
	}
	count++;

	// http ACCEPT_LANGUAGE
	header = get_header(req, ACCEPT_LANGUAGE);
	if(header != NULL) {
		snprintf(env[count], ENV_SIZE, "%s%s", ENVP[count], header);
	} else {
		snprintf(env[count], ENV_SIZE, "%s", ENVP[count]);
	}
	count++;

	// http ACCEPT_CHARSET
	header = get_header(req, ACCEPT_CHARSET);
	if(header != NULL) {
		snprintf(env[count], ENV_SIZE, "%s%s", ENVP[count], header);
	} else {
		snprintf(env[count], ENV_SIZE, "%s", ENVP[count]);
	}
	count++;

	// http COOKIE
	header = get_header(req, COOKIE);
	if(header != NULL) {
		snprintf(env[count], ENV_SIZE, "%s%s", ENVP[count], header);
	} else {
		snprintf(env[count], ENV_SIZE, "%s", ENVP[count]);
	}
	count++;

	// http accept
	header = get_header(req, CONNECTION_HEADER);
	if(header != NULL) {
		snprintf(env[count], ENV_SIZE, "%s%s", ENVP[count], header);
	} else {
		snprintf(env[count], ENV_SIZE, "%s", ENVP[count]);
	}
	count++;


	// http HOST_HEADER
	header = get_header(req, HOST_HEADER);
	if(header != NULL) {
		snprintf(env[count], ENV_SIZE, "%s%s", ENVP[count], header);
	} else {
		snprintf(env[count], ENV_SIZE, "%s", ENVP[count]);
	}
	count++;

	// free(env[count]);
	freed = count;
	env[count] = NULL;

	return LISO_SUCCESS;
}

/**
 * @brief generate error respnse
 * 
 * @param req request which this would be reqply to
 * @param resp repsonse to be filled
 * @param error error for the repsonse
 * @return ** int LISO_SUCCESS on success, error otherwise
 */
int generate_error_response(Request *req, Response *resp, int error) {

	assert(resp != NULL);

	if(resp->headers != NULL) {
		free(resp->headers);
	}

	resp->headers = malloc(sizeof(Http_header) * HEADER_COUNT_INCREMENT);

	if(resp->headers == NULL) {
		return LISO_MEM_FAIL;
	}

	resp->header_count = 0;
	resp->header_allocated = HEADER_COUNT_INCREMENT;

	// populate response line
	strncpy(resp->http_version, version, strlen(version) + 1);
	LISOPRINTF(fp,"Length of:+%s+ is %ld", version, strlen(version) + 1);

	// add connection header
	if((req != NULL && get_conn_header(req) == LISO_CLOSE_CONN) || error == LISO_TIMEOUT) {
		add_header(resp, CONNECTION_HEADER, CLOSE);
	} else {
		add_header(resp, CONNECTION_HEADER, KEEP_ALIVE);
	}
	// add server name
	add_header(resp, SERVER_HEADER, LISO_NAME);
	add_content_length(resp, 0);
	// add date
	add_time(resp);

	add_header(resp, MIME_HEADER, "text/html");

	switch (error)
	{
	case LISO_LOAD_FAILED:
		strncpy(resp->http_status_reason, STATUS_404, strlen(STATUS_404) +1);
		LISOPRINTF(fp,"Length of:+%s+ is %ld", STATUS_404, strlen(STATUS_404) +1);
		break;
	case LISO_UNSUPPORTED_METHOD:
		strncpy(resp->http_status_reason, STATUS_501, strlen(STATUS_501) +1);
		break;
	case LISO_TIMEOUT:
		strncpy(resp->http_status_reason, STATUS_408, strlen(STATUS_408) +1);
		break;
	case LISO_BAD_VERSION_NUMBER:
		strncpy(resp->http_status_reason, STATUS_505, strlen(STATUS_505) +1);
		break;
	case LISO_BAD_REQUEST:
		strncpy(resp->http_status_reason, STATUS_400, strlen(STATUS_400) +1);
		break;
	case LISO_MEM_FAIL:
		strncpy(resp->http_status_reason, STATUS_400, strlen(STATUS_400) +1);
		break;
	default:
		strncpy(resp->http_status_reason, STATUS_400, strlen(STATUS_400) +1);
		break;
	}

	return LISO_SUCCESS;
}

/**
 * @brief Populate a basic barebones HTTP response
 * 
 * @param resp repsonse to fill
 * @return ** int LISO_SUCCESS on success, error otherwise
 */
int populate_basic_response(Response *resp) {

	assert(resp != NULL);

	resp->headers = malloc(sizeof(Http_header) * HEADER_COUNT_INCREMENT);
	if(resp->headers == NULL) {
		return LISO_MEM_FAIL;
	}

	resp->header_count = 0;
	resp->header_allocated = HEADER_COUNT_INCREMENT;

	// populate response line
	strncpy(resp->http_version, version, strlen(version) +1);
	strncpy(resp->http_status_reason, STATUS_200, strlen(STATUS_200) +1);

	// add server name
	add_header(resp, SERVER_HEADER, LISO_NAME);

	// add date
	add_time(resp);

	return LISO_SUCCESS;
}

/**
 * @brief Try to load the URI received in the request
 * 
 * @param req request received
 * @param resp repsonse to be sent back to client
 * @return ** int LISO_SUCCESS on success, error otherwise
 */
int load_uri(Request *req, Response *resp) {
	assert(req != NULL);
	assert(resp != NULL);
	size_t buf_space = 4096;
	char path[PATH_MAX];

	snprintf(path, buf_space, "%s%s", LISO_PATH, req->http_uri);
	LISOPRINTF(fp," %s http req uri %s\n", __func__, req->http_uri );

	struct stat sfile;
	int error = stat(path, &sfile);

	if(error != 0) {
		LISOPRINTF(fp," liso path is, %s\n", LISO_PATH );
		LISOPRINTF(fp," failed stat returned error for path, %s errno is \n", path );
		perror("error returned");
		return LISO_LOAD_FAILED;
	}

	LISOPRINTF(fp," the name of file is %s and lenght is %ld", path, sfile.st_size);

	resp->message_len = sfile.st_size;
	resp->message = malloc(sfile.st_size);

	FILE *fp;
	fp = fopen(path, "r");

	size_t read_size = fread(resp->message, sizeof(char), sfile.st_size, fp);

	if(read_size != sfile.st_size) {
		LISOPRINTF(fp," failed read size mismatch for file on path for path, %s file size was %ld size read was %ld\n", path , sfile.st_size, read_size);
		return LISO_LOAD_FAILED;
	}

	fclose(fp);

	add_mime_extension(path, resp);
	add_content_length(resp, sfile.st_size);
	add_last_modified(resp, &sfile.st_mtim);

	return LISO_SUCCESS;
}

/**
 * @brief Get the full request len
 * 
 * @param req request
 * @return ** int total_len of the request
 */
int get_full_request_len(Request *req) {

	int total_len = 0;
	assert(req != NULL);

	int index = get_header_index(req->headers, CONTENT_LEN_HEADER, req->header_count);

	if(index != LISO_ERROR) {
		total_len += atoi(req->headers[index].header_value);
	}

	total_len+=req->request_len;

	return total_len;
}

/**
 * @brief Get the conn header of the request
 * 
 * @param req request to search in
 * @return ** int LISO_CLOSE_CONN if connection header was close, LISO_SUCCESS 
 * otherwise
 */
int get_conn_header(Request *req) {
	int res = LISO_SUCCESS;
	int index = get_header_index(req->headers, CONNECTION_HEADER, req->header_count);

	if(index != LISO_ERROR) {
		if(strncasecmp(req->headers[index].header_value, CLOSE, strlen(CLOSE)) == 0)  {
			res = LISO_CLOSE_CONN;
		}
	}

	return res;
}

/**
 * @brief Process a get rHTTP equest
 * 
 * @param req parsed request to process
 * @return ** Response* response for the request
 */
Response* process_get(Request *req) {
	LISOPRINTF(fp," %s http req uri %s\n", __func__, req->http_uri );
	Response *resp = malloc(sizeof(Response));
	memset(resp, 0,  sizeof(Response));

	assert(resp != NULL);

	int error;

	populate_basic_response(resp);

	LISOPRINTF(fp," %s http req uri %s\n", __func__, req->http_uri );
	error = load_uri(req, resp);
	if(error != LISO_SUCCESS) {
		int err_err = generate_error_response(req, resp, error);
		assert(err_err == LISO_SUCCESS);
	}

	// add connection header
	if(get_conn_header(req) == LISO_CLOSE_CONN) {
		add_header(resp, CONNECTION_HEADER, CLOSE);
	} else {
		add_header(resp, CONNECTION_HEADER, KEEP_ALIVE);
	}

	return resp;
}

/**
 * @brief Process a head HTTP equest
 * 
 * @param req parsed request to process
 * @return ** Response* response for the request
 */
Response* process_head(Request *req) {

	Response *resp = process_get(req);

	if(resp->message_len != 0) {
		free(resp->message);
		resp->message = NULL;
		resp->message_len = 0;
	}

	return resp;
}

/**
 * @brief Process a post HTTP equest
 * 
 * @param req parsed request to process
 * @return ** Response* response for the request
 */
int process_post(int client_socket, Request *req, char *buf, int bufsize) {
	return 0;
}

/**
 * @brief Process an error and send resposne
 * 
 * @param error error encountered
 * @param req parsed request to process
 * @return ** Response* response for the request
 */
Response* process_error(int error, Request *req) {
	Response *resp = malloc(sizeof(Response));
	resp->headers = NULL;

	assert(resp != NULL);

	int err = generate_error_response(req, resp, error);
	assert(err == LISO_SUCCESS);
	return resp;
}

/**
 * @brief Convert the respnse structure in to byte stream for
 * transmission
 * 
 * @param resp reponse to be translated
 * @param bufsize [out] bufsize of the bytestream
 * @return ** char* pointer to byte stream to be sent
 */
char* convert_response_to_byte_stream(Response *resp, int *bufsize) {
	
	int cur_bufsize = BUF_SIZE;
	char* resp_buf = malloc(cur_bufsize);
	memset(resp_buf, 0, cur_bufsize);
	int count = 0;

	// response line
	// HTTP1.1 response line
	count += snprintf(resp_buf + count, cur_bufsize - count, 
	"%s %s\r\n", resp->http_version, resp->http_status_reason);

	// headers
	for(int i = 0 ; i < resp->header_count; i++) {
		// header_name: header_value \r\n
		count += snprintf(resp_buf + count, cur_bufsize - count, 
		"%s: %s\r\n", resp->headers[i].header_name, resp->headers[i].header_value);
	}

	// last CRLF
	count += snprintf(resp_buf + count, cur_bufsize - count, "\r\n");

	// message
	if(resp->message_len > 0) {
		if(resp->message_len + count > cur_bufsize) {
			resp_buf = realloc(resp_buf, resp->message_len + count + 10);
			cur_bufsize = resp->message_len + count + 10;
		}

		memcpy(resp_buf + count, resp->message, resp->message_len);
		count += resp->message_len;

		free(resp->message);
	}

	free(resp->headers);
	free(resp);

	*bufsize = count;
	return resp_buf;
}

/**
 * @brief generate error
 * 
 * @param error error for the response
 * @param resp_size [out] response size
 * @param req request to respond to
 * @return ** char* byte stream for the response
 */
char* generate_error(int error, int *resp_size, Request *req) {
	Response *resp = process_error(error, req);

	// convert to char array and return char* buffer
	return convert_response_to_byte_stream(resp, resp_size);
}

char* generate_reply(Request *req, char *buf, int bufsize, int *resp_size) {
	LISOPRINTF(fp,"inside %s\n", __func__);
	LISOPRINTF(fp," %s http req uri %s\n", __func__, req->http_uri );
	Response *resp;

	if(strncasecmp(req->http_method, GET, strlen(GET)) == 0) {
		LISOPRINTF(fp,"%s Processing Get \n", __func__);
		resp = process_get(req);
	} else if(strncasecmp(req->http_method, HEAD, strlen(HEAD)) == 0) {
		LISOPRINTF(fp,"%s Processing HEAD \n", __func__);
		resp = process_head(req);
	} else if (strncasecmp(req->http_method, POST, strlen(POST)) == 0) {
		LISOPRINTF(fp,"%s Processing Post \n", __func__);
		// special case for now
		*resp_size = bufsize;
		return buf;
	} else {
		// invalid request
		resp = process_error(LISO_UNSUPPORTED_METHOD, req);
	}

	// convert to char array and return char* buffer
	return convert_response_to_byte_stream(resp, resp_size);

}

/**
 * @brief perform a sanity check on the parsed request
 * 
 * @param req request to be checked
 * @return ** int LISO_SUCCESS on success, error otherwise
 */
int sanity_check(Request *req) {
	// check version
	if(strncasecmp(req->http_version, version, strlen(version + 1)) != 0) {
		LISOPRINTF(fp,"req method rx %s matching with %s failed \n", req->http_method, version);
		return LISO_BAD_VERSION_NUMBER;
	}

	return LISO_SUCCESS;
}