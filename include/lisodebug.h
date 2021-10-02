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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Define LISODEBUG to enable debug messages for this liso server */
// #define LISODEBUG
#ifdef LISODEBUG
#include <stdio.h>
#define LISOPRINTF(...) fprintf(__VA_ARGS__)
#else
#define LISOPRINTF(...)
#endif

#endif // _LISODEBUG_H_