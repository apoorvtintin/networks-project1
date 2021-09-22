#include "liso.h"
#include "sys/stat.h"
#include <stdio.h>
#include <time.h>

#define SIZE_STRING_BUF_SIZE 50

// 
const int HEADER_COUNT_INCREMENT =  5;

// bad request response // DEPRECATED TODO CONSTRUCT
const char bad_request[] = {"HTTP/1.1 400 Bad Request\r\n\r\n"};

// HTTP tokens
const char version[] = {"HTTP/1.1"};
const char GET[] = {"Get"};
const char HEAD[] = {"Head"};
const char POST[] = {"Post"};
const char SERVER_HEADER[] = {"Server"};
const char LISO_NAME[] = {"liso/1.0"};

// MIME Types
const char MIME_HEADER[] = {"Content-Type"};
const char CONTENT_LEN_HEADER[] = {"Content-Length"};
const char DATE_HEADER[] = {"Date"};
const char LAST_MODIFIED_HEADER[] = {"Last-Modified"};
const char CONNECTION_HEADER[] = {"Conenction"};

const char CLOSE[] = {"close"};

/**
 * JPEG: image/jpeg
 * GIF: image/gif
 * PNG: image/png
 * JavaScript: application/javascript
 * JSON: application/json
 * CSS: text/css
 * HTML: text/html
 * Plain TXT: text/plain
 * Non-descript data: application/octet-stream
 */
// const char JPEG_MIME[] = "image/jpeg";
// const char GIF_MIME[] = "image/gif";
// const char PNG_MIME[] = "image/png";
// const char JavaScript_MIME[] =  "application/javascript";
// const char JSON_MIME[] = "application/json";
// const char CSS_MIME[] = "text/css";
// const char HTML_MIME[] = "text/html";
// const char Plain_TXT_MIME[] = "text/plain";
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

const char* const TYPES[] = {"jpeg",
							"gif",
							"png",
							"js",
							"json",
							"css",
							"html",
							"txt",
							0};

const int MIME_NUM_TYPES = 8;

// const char JPEG[] = "jpeg";
// const char GIF[] = "gif";
// const char PNG[] = "png";
// const char JavaScript[] =  "js";
// const char JSON[] = "json";
// const char CSS[] = "css";
// const char HTML[] = "html";
// const char Plain_TXT[] = "txt";

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

int add_content_length(Response *resp, size_t size) {
	char buf[SIZE_STRING_BUF_SIZE];
	snprintf(buf, 20, "%ld", size);

	return add_header(resp, CONTENT_LEN_HEADER, buf);
}

int add_time(Response *resp) {
	char buf[1000];
	time_t now = time(0);
	struct tm tm = *gmtime(&now);
	strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S %Z", &tm);

	return add_header(resp, DATE_HEADER, buf);
}

int add_last_modified(Response *resp, struct timespec *tv) {

	char buf[1000];
	time_t now = (time_t)tv->tv_sec;
	struct tm tm = *gmtime(&now);
	strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S %Z", &tm);

	return add_header(resp, LAST_MODIFIED_HEADER, buf);
}

int add_mime_extension(char *path, Response *resp) {
	char *pch=strrchr(path,'.');

	char extension[10];
	strncpy(extension, pch, 10);

	for(int i = 0; TYPES[i] != NULL; i++) {
		if(strncasecmp(extension, TYPES[i], strlen(TYPES[i])) == 0) {
			// matching MIME extension found
			return add_header(resp, MIME_HEADER, MIME[i]);
			
		}
	}

	return add_header(resp, MIME_HEADER, Non_descript_data_MIME);
}

int generate_error_response(Response *resp, int error) {

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
	LISOPRINTF("Length of:+%s+ is %ld", version, strlen(version) + 1);

	add_header(resp, CONNECTION_HEADER, CLOSE);
	// add server name
	add_header(resp, SERVER_HEADER, LISO_NAME);

	// add date
	add_time(resp);

	add_header(resp, MIME_HEADER, "text/html");

	switch (error)
	{
	case LISO_LOAD_FAILED:
		strncpy(resp->http_status_reason, STATUS_404, strlen(STATUS_404) +1);
		LISOPRINTF("Length of:+%s+ is %ld", STATUS_404, strlen(STATUS_404) +1);
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

int load_uri(Request *req, Response *resp) {
	assert(req != NULL);
	assert(resp != NULL);
	size_t buf_space = 4096;
	char path[PATH_MAX];

	snprintf(path, buf_space, "%s%s", LISO_PATH, req->http_uri);
	LISOPRINTF(" %s http req uri %s\n", __func__, req->http_uri );

	struct stat sfile;
	int error = stat(path, &sfile);

	if(error != 0) {
		LISOPRINTF(" liso path is, %s\n", LISO_PATH );
		LISOPRINTF(" failed stat returned error for path, %s errno is \n", path );
		perror("error returned");
		return LISO_LOAD_FAILED;
	}

	LISOPRINTF(" the name of file is %s and lenght is %ld", path, sfile.st_size);

	resp->message_len = sfile.st_size;
	resp->message = malloc(sfile.st_size);

	FILE *fp;
	fp = fopen(path, "r");

	size_t read_size = fread(resp->message, sizeof(char), sfile.st_size, fp);

	if(read_size != sfile.st_size) {
		LISOPRINTF(" failed read size mismatch for file on path for path, %s file size was %ld size read was %ld\n", path , sfile.st_size, read_size);
		return LISO_LOAD_FAILED;
	}

	fclose(fp);

	
	add_content_length(resp, sfile.st_size);
	add_last_modified(resp, &sfile.st_mtim);

	return LISO_SUCCESS;
}

Response* process_get(Request *req) {
	LISOPRINTF(" %s http req uri %s\n", __func__, req->http_uri );
	Response *resp = malloc(sizeof(Response));
	memset(resp, 0,  sizeof(Response));

	assert(resp != NULL);

	int error;

	populate_basic_response(resp);

	LISOPRINTF(" %s http req uri %s\n", __func__, req->http_uri );
	error = load_uri(req, resp);
	if(error != LISO_SUCCESS) {
		int err_err = generate_error_response(resp, error);
		assert(err_err == LISO_SUCCESS);
	}

	return resp;
}

Response* process_head(Request *req) {

	Response *resp = process_get(req);

	if(resp->message_len != 0) {
		free(resp->message);
		resp->message = NULL;
		resp->message_len = 0;
	}

	return resp;
}

int process_post(int client_socket, Request *req, char *buf, int bufsize) {
	return 0;
}

Response* process_error(int error) {
	Response *resp = malloc(sizeof(Response));

	assert(resp != NULL);

	int err = generate_error_response(resp, error);
	assert(err == LISO_SUCCESS);
	return resp;
}

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
	}

	free(resp->headers);
	free(resp);

	*bufsize = count;
	return resp_buf;
}

char* generate_error(int error, int *resp_size) {
	Response *resp = process_error(error);

	// convert to char array and return char* buffer
	return convert_response_to_byte_stream(resp, resp_size);
}

char* generate_reply(Request *req, char *buf, int bufsize, int *resp_size) {
	LISOPRINTF("inside %s\n", __func__);
	LISOPRINTF(" %s http req uri %s\n", __func__, req->http_uri );
	Response *resp;

	if(strncasecmp(req->http_method, GET, strlen(GET)) == 0) {
		LISOPRINTF("%s Processing Get \n", __func__);
		resp = process_get(req);
	} else if(strncasecmp(req->http_method, HEAD, strlen(HEAD) == 0)) {
		LISOPRINTF("%s Processing HEAD \n", __func__);
		resp = process_head(req);
	} else if (strncasecmp(req->http_method, POST, strlen(POST)) == 0) {
		LISOPRINTF("%s Processing Post \n", __func__);
		// special case for now
		*resp_size = bufsize;
		return buf;
	} else {
		// invalid request
		resp = process_error(LISO_UNSUPPORTED_METHOD);
	}

	// convert to char array and return char* buffer
	return convert_response_to_byte_stream(resp, resp_size);

}