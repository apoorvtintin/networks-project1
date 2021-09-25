/**
 * @file parse.c
 * @author Apoorv Gupta <apoorvgu@andrew.cmu.edu>
 * @brief Implementation of the parsing component of LISO
 * @version 0.1
 * @date 2021-09-08
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "parse.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "lisodebug.h"

/**
* Given a char buffer returns the parsed request headers
*/
Request * parse(char *buffer, int size, int socketFd) {

	//Differant states in the state machine
	enum {
		STATE_START = 0, STATE_CR, STATE_CRLF, STATE_CRLFCR, STATE_CRLFCRLF
	};

	int i = 0, state;
	size_t offset = 0;
	char ch;
	// char buf[8192];
	char* buf = malloc(size);
	memset(buf, 0, size);

	state = STATE_START;
	while (state != STATE_CRLFCRLF) {
		char expected = 0;

		if (i == size)
			break;

		ch = buffer[i++];
		buf[offset++] = ch;

		switch (state) {
		case STATE_START:
		case STATE_CRLF:
			expected = '\r';
			break;
		case STATE_CR:
		case STATE_CRLFCR:
			expected = '\n';
			break;
		default:
			state = STATE_START;
			continue;
		}

		if (ch == expected)
			state++;
		else
			state = STATE_START;

	}

    //Valid End State
	if (state == STATE_CRLFCRLF) {

		Request *request = (Request *) malloc(sizeof(Request));
        request->header_count=0;
		request->request_len=i;
        //TODO You will need to handle resizing this in parser.y
        request->headers = (Http_header *) malloc(sizeof(Http_header));

		assert(request->headers != NULL);
		set_parsing_options(buf, i, request);

		if (yyparse() == SUCCESS) {
			free(buf);
            return request;
		}

		yyrestart(NULL);
	}

    LISOPRINTF("Parsing Failed\n");
	free(buf);
	return NULL;
}

