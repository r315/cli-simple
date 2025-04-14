/**
 * @file stdinout.h
 * @brief This header defines a structure with function pointers
 * for printing operations. The idea is then to be shared between
 * different libraries/drivers
 */

#ifndef _stdinout_h_
#define _stdinout_h_

typedef struct stdinout_s {
    int (*available)(void);                 // Number of available characters in buffer
    int (*read)(char* str, int len);        // Read len chars from buffer, wait until all chars are obtained
    int (*write)(const char* str, int len); // Write len chars
}stdinout_t;

#endif