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

const char STATUS_400[] = {"400 Error"};
const char STATUS_404[] = {"404 Not Found"};
const char STATUS_408[] = {"408 Connection timeout"};
const char STATUS_501[] = {"501 Unsupported method"};
const char STATUS_505[] = {"505 Bad version number"};

const char STATUS_200[] = {"505 OK"};



// liso storage Path
extern char LISO_PATH[PATH_MAX];

// extension
const char pdf[] = {"400 Error"};
const char text[] = {"404 Not Found"};
const char STATUS_408[] = {"408 Connection timeout"};
const char STATUS_501[] = {"501 Unsupported method"};
const char STATUS_505[] = {"505 Bad version number"};

int add_header(Response *resp, char* name, char* value) {
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

	strncpy(resp->headers[resp->header_count].header_name, name, strlen(name));
	strncpy(resp->headers[resp->header_count].header_value, value, strlen(value));
	resp->header_count++;

	return LISO_SUCCESS;
}

int add_content_length(Response *resp, size_t size) {
	char buf[SIZE_STRING_BUF_SIZE];
	sprintf(buf, "%ld", size);

	return add_header(resp, CONTENT_LEN_HEADER, buf);
}

int add_time(Response *resp) {
	char buf[1000];
	time_t now = time(0);
	struct tm tm = *gmtime(&now);
	size_t dt_size = strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S %Z", &tm);

	return add_header(resp, DATE_HEADER, buf);
}

int add_last_modified(Response *resp, struct timespec *tv) {

	char buf[1000];
	time_t now = (time_t)tv->tv_sec;
	struct tm tm = *gmtime(&now);
	size_t dt_size = strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S %Z", &tm);

	return add_header(resp, LAST_MODIFIED_HEADER, buf);
}

inline get_mime_extension(char *path, Response *resp) {
	char *pch=strrchr(path,'.');

	char extension[10];
	strncpy(extension, pch, 10);

	for(char *i = TYPES[0]; i != NULL; i++) {
		if(strcmpi(extension, i, strlen(i))) {
			// matching MIME extension found

		}
	}
}

int generate_error_response() {
return 0;
}

int populate_basic_reponse(Response *resp) {

	assert(resp != NULL);

	resp->headers = malloc(sizeof(Http_header) * HEADER_COUNT_INCREMENT);
	if(resp->headers == NULL) {
		return LISO_MEM_FAIL;
	}

	resp->header_count = 0;
	resp->header_allocated = HEADER_COUNT_INCREMENT;

	// populate response line
	strncpy(resp->http_version, version, strlen(version));
	strncpy(resp->http_status_reason, STATUS_200, strlen(STATUS_200));

	// add server name
	add_header(resp, SERVER_HEADER, LISO_NAME);

	// add date
	add_time(resp);

	return LISO_SUCCESS;
}

Response* process_get(Request *req) {

	Response *resp = malloc(sizeof(Response));
	resp->header_count = 0;
	int error;

	populate_basic_response(&resp);

	if((error = load_uri(req, &resp)) != LISO_SUCCESS) {
		return generate_error_response(error);
	}

	return resp;
}

int check_uri(Request *req) {

	size_t buf_space = 4096;
	char path[PATH_MAX];

	snprintf(path, buf_space, "%s/%s", LISO_PATH, req->http_uri);

	struct stat sfile;
	int error = stat(path, &sfile);

	if(error == 0) {
		return LISO_SUCCESS;
	} else {
		return LISO_LOAD_FAILED;
	}

}

int load_uri(Request *req, Response *resp) {
	size_t buf_space = 4096;
	char path[PATH_MAX];

	snprintf(path, buf_space, "%s/%s", LISO_PATH, req->http_uri);

	struct stat sfile;
	int error = stat(path, &sfile);

	if(error != 0) {
		return LISO_LOAD_FAILED;
	}

	// last modified
	resp->message_len = sfile.st_size;
	resp->message = malloc(sfile.st_size);

	FILE *fp;
	fp = fopen(path, "r");

	size_t read_size = fread(resp->message, sizeof(char), sfile.st_size, fp);

	if(read_size != sfile.st_size) {
		return LISO_LOAD_FAILED;
	}

	fclose(fp);

	add_content_length(resp, sfile.st_size);
	add_last_modified(resp, &sfile.st_mtimespec);



}

Response* process_head(Request *req) {

	Response *resp = malloc(sizeof(Response));
	int error;

	populate_basic_response(&resp);

	error = check_uri(req);
	if(error != LISO_SUCCESS) {
		return generate_error(error);
	}

	return resp;
}

int process_post(int client_socket, Request *req, char *buf, int bufsize) {
	return 0;
}

