/**
 * @file parse.h
 * @author Apoorv Gupta <apoorvgu@andrew.cmu.edu>
 * @brief Header for parsing component of LISO
 * @version 0.1
 * @date 2021-09-08
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef _PARSE_H_
#define _PARSE_H_

#define SUCCESS 0

//Header field
typedef struct
{
	char header_name[4096];
	char header_value[4096];
} Http_header;

//HTTP Request Header
typedef struct
{
	char http_version[50];
	char http_method[50];
	char http_uri[4096];
	Http_header *headers;
	int header_count;

	char* message;
	int message_len;
	int request_len;
	int is_cgi;
} Request;

typedef struct {
	char http_version[50];
	char http_status_reason[4096];
	Http_header *headers;
	int header_count;
	int header_allocated;
	int message_len;
	char *message;

	int error;
} Response;

Request* parse(char *buffer, int size,int socketFd);

// functions decalred in parser.y
int yyparse();
void set_parsing_options(char *buf, size_t i, Request *request);
void yyrestart ( FILE *input_file  );

#endif