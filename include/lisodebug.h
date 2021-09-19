/**
 * @file lisodebug.h
 * @author Apoorv Gupta <apoorvgu@andrew.cmu.edu>
 * @brief Logging infrastructure for LISO server
 * @version 0.1
 * @date 2021-09-08
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#ifndef _LISODEBUG_H_
#define _LISODEBUG_H_

/* Define LISODEBUG to enable debug messages for this liso server */
//#define LISODEBUG
#ifdef LISODEBUG
#include <stdio.h>
#define LISOPRINTF(...) printf(__VA_ARGS__)
int yydebug = STDOUT_FILENO;
#else
#define LISOPRINTF(...)
#endif

#endif // _LISODEBUG_H_