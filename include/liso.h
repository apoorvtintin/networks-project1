#ifndef _LISO_H_
#define _LISO_H_

#include "lisodebug.h"
#include "parse.h"

// MACROS
#define BUF_SIZE 4096				// size of Liso Buffer 
#define BAD_REQUEST_SIZE 28			// size of a bad request error 400
#define PATH_MAX 4096

enum liso_errors {
	LISO_ERROR = -1,
	LISO_SUCCESS = 0,
	LISO_LOAD_FAILED = 1,
	LISO_MEM_FAIL = 2,
};

#endif // _LISO_H_