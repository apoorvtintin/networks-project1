/**
 * @file cgi.c
 * @author Apoorv Gupta (apoorvgu@andrew.cmu.edu)
 * @brief 
 * This file contains the implementation for CGI in LISO, the LISO deamon
 * forks and runs the cgi script. The CGI script asynchronously returns the 
 * response to LISO which is then sent back to the client.
 * 
 * Initial implementation taken from 15441 P1 CP3 starter code
 * 
 * @version 0.1
 * @date 2021-10-02
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "liso.h"
#include <stdbool.h>

/**************** BEGIN GLOBALS ***************/
extern char cgi_script[BUF_SIZE];
extern FILE* fp;
int freed = -1;
/**************** BEGIN GLOBALS ***************/

/**************** BEGIN CONSTANTS ***************/

#define ARG_NUM 2
#define BUF_SIZE 4096

/**************** END CONSTANTS ***************/

/**************** BEGIN UTILITY FUNCTIONS ***************/
/* error messages stolen from: http://linux.die.net/man/2/execve */
/**
 * @brief Display error encoutered while execve
 * 
 * @return ** void 
 */
void execve_error_handler()
{
    switch (errno)
    {
        case E2BIG:
            fprintf(stderr, "The total number of bytes in the environment \
(envp) and argument list (argv) is too large.\n");
            return;
        case EACCES:
            fprintf(stderr, "Execute permission is denied for the file or a \
script or ELF interpreter.\n");
            return;
        case EFAULT:
            fprintf(stderr, "filename points outside your accessible address \
space.\n");
            return;
        case EINVAL:
            fprintf(stderr, "An ELF executable had more than one PT_INTERP \
segment (i.e., tried to name more than one \
interpreter).\n");
            return;
        case EIO:
            fprintf(stderr, "An I/O error occurred.\n");
            return;
        case EISDIR:
            fprintf(stderr, "An ELF interpreter was a directory.\n");
            return;
        case ELIBBAD:
            fprintf(stderr, "An ELF interpreter was not in a recognised \
format.\n");
            return;
        case ELOOP:
            fprintf(stderr, "Too many symbolic links were encountered in \
resolving filename or the name of a script \
or ELF interpreter.\n");
            return;
        case EMFILE:
            fprintf(stderr, "The process has the maximum number of files \
open.\n");
            return;
        case ENAMETOOLONG:
            fprintf(stderr, "filename is too long.\n");
            return;
        case ENFILE:
            fprintf(stderr, "The system limit on the total number of open \
files has been reached.\n");
            return;
        case ENOENT:
            fprintf(stderr, "The file filename or a script or ELF interpreter \
does not exist, or a shared library needed for \
file or interpreter cannot be found.\n");
            return;
        case ENOEXEC:
            fprintf(stderr, "An executable is not in a recognised format, is \
for the wrong architecture, or has some other \
format error that means it cannot be \
executed.\n");
            return;
        case ENOMEM:
            fprintf(stderr, "Insufficient kernel memory was available.\n");
            return;
        case ENOTDIR:
            fprintf(stderr, "A component of the path prefix of filename or a \
script or ELF interpreter is not a directory.\n");
            return;
        case EPERM:
            fprintf(stderr, "The file system is mounted nosuid, the user is \
not the superuser, and the file has an SUID or \
SGID bit set.\n");
            return;
        case ETXTBSY:
            fprintf(stderr, "Executable was open for writing by one or more \
processes.\n");
            return;
        default:
            fprintf(stderr, "Unkown error occurred with execve().\n");
            return;
    }
}
/**************** END UTILITY FUNCTIONS ***************/


/**
 * @brief Start processing a CGI request
 * 
 * @param req the request recieved
 * @param c the client that sent the request
 * @return ** int fd which will have the script response or LISO_ERROR otherwise
 */
int start_process_cgi(Request *req, client *c) {
    LISOPRINTF(fp, "inside %s", __func__);
    print_parse_req(req);
    /*************** BEGIN VARIABLE DECLARATIONS **************/
    pid_t pid;
    int stdin_pipe[2];
    int stdout_pipe[2];

    /*************** END VARIABLE DECLARATIONS **************/

	// create environment variables
	char* env[ENV_NUM];
    get_http_env(env, req, c->remote_address, c->port);

    char* arg[ARG_NUM];
    arg[0] = cgi_script;
    arg[1] = NULL;

    /*************** BEGIN PIPE **************/
    /* 0 can be read from, 1 can be written to */
    if (pipe(stdin_pipe) < 0)
    {
        fprintf(stderr, "Error piping for stdin.\n");
        return LISO_ERROR;
    }

    if (pipe(stdout_pipe) < 0)
    {
        fprintf(stderr, "Error piping for stdout.\n");
        return LISO_ERROR;
    }
    /*************** END PIPE **************/

    /*************** BEGIN FORK **************/
    int ppid = getpid();
    pid = fork();
    /* not good */
    if (pid < 0)
    {
        fprintf(stderr, "Something really bad happened when fork()ing.\n");
        return LISO_ERROR;
    }

    /* child, setup environment, execve */
    if (pid == 0)
    {
        /*************** BEGIN EXECVE ****************/
        close(stdout_pipe[0]);
        close(stdin_pipe[1]);
        dup2(stdout_pipe[1], fileno(stdout));
        dup2(stdin_pipe[0], fileno(stdin));
        setpgid(getpid(), ppid);
        /* you should probably do something with stderr */

        /* pretty much no matter what, if it returns bad things happened... */
        if (execve(cgi_script, arg, env))
        {
            execve_error_handler();
            fprintf(fp, "Error executing execve syscall with filename %s\n", cgi_script);
            exit(-1);
        }
        /*************** END EXECVE ****************/ 
    }

    if (pid > 0)
    {
        // free envp
        fprintf(fp, "Parent: Heading to select() loop.\n");
        close(stdout_pipe[1]);
        close(stdin_pipe[0]);

        if (write(stdin_pipe[1], req->message, req->message_len) < 0)
        {
            fprintf(stderr, "Error writing to spawned CGI program.\n");
            return EXIT_FAILURE;
        }

        close(stdin_pipe[1]); /* finished writing to spawn */
		close(stdin_pipe[1]);

        client *cgi_client = malloc(sizeof(client));
        cgi_client->sock = stdout_pipe[0];
        cgi_client->cgi_host = c;
        cgi_client->pipeline_flag = false;

        add_client(cgi_client);

        for(int i = 0; i < ENV_NUM -1; i++) {
            if(freed != i)
                free(env[i]);
        }

		return stdout_pipe[0];
	}

    return LISO_ERROR;
}

/**
 * @brief Wrap up the CGI response started earlier and send the response back
 * to the client
 * 
 * @param cgi_client the pipefd of CGI script
 * @return ** int LISO_SUCCESS on success negative otherwise
 */
int wrap_process_cgi(client *cgi_client) {

    assert(cgi_client != NULL);
    assert(cgi_client->cgi_host != NULL);

	int readret;
	char *resp_buf = malloc(BUF_SIZE);
    assert(resp_buf != NULL);
    int resp_size = 0;
    int incr = 2;

	/* you want to be looping with select() telling you when to read */
	while((readret = read(cgi_client->sock, resp_buf, BUF_SIZE-1)) > 0)
	{
        if(readret == BUF_SIZE-1) {
            resp_buf = realloc(resp_buf, BUF_SIZE* incr);
            assert(resp_buf != NULL);
        }
        resp_size+=readret;
        resp_buf[resp_size] = '\0'; /* nul-terminate string */
		LISOPRINTF(fp, "Got from CGI: %s\n", resp_buf);
	}

    LISOPRINTF(fp,"printing output of cgi script\n");
    print_req_buf(resp_buf, resp_size);

	close(cgi_client->sock);

	if (readret == 0)
	{
		LISOPRINTF(fp, "CGI spawned process returned with EOF as expected.\n");
	}

	if (send(cgi_client->cgi_host->sock, resp_buf, resp_size, 0) != resp_size)
	{
		fprintf(stderr, "Error sending to client.\n");
		perror("send recieved an error");
		free(resp_buf);
		return -1;
	}

    cgi_client->cgi_host = NULL;
    delete_client(cgi_client);
    free(cgi_client);

    free(resp_buf);
    return LISO_SUCCESS;
}