#ifndef CLI_SIMPLE_H
#define CLI_SIMPLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <string.h>
#include "stdinout.h"

#define CLI_LINE_MAX_LEN    64
#define CLI_HISTORY_SIZE    4
#define CLI_CMD_MAX_ARGS    8

#define CLI_IS_PARM(_i, _P) !strcmp((const char*)argv[_i], (const char*)_P)
#define CLI_GET_INT_PARM(_i, _P) CLI_Ia2i(argv[_i], (int32_t*)&_P)
#define CLI_GET_HINT_PARM(_i, _P) CLI_Ha2i(argv[_i], (uint32_t*)&_P)

typedef enum cli_result_e
{
   CLI_OK = 0,
   CLI_OK_LF,
   CLI_LINE_READ,
   CLI_CMD_NOT_FOUND,
   CLI_BAD_PARAM,
   CLI_MISSING_ARGS,
   CLI_ERROR,
   CLI_EXIT
}cli_result_t;

typedef int (*cli_func) (int argc, char **argv);

typedef struct cli_command_s
{
   const char *name;
   cli_func exec;
}cli_command_t;

typedef struct cli_history_s
{
   uint16_t head;       // Index of free entry
   uint16_t index;      // Current history entry being viewed
   uint16_t size;
   uint8_t history[CLI_HISTORY_SIZE][CLI_LINE_MAX_LEN];
}cli_history_t;

void CLI_Init (const char *prompt, stdinout_t *io);
cli_result_t CLI_ReadLine (void);
cli_result_t CLI_HandleLine (void);
cli_result_t CLI_ProcessLine (uint8_t *line);
void CLI_RegisterCommand (const cli_command_t *Commands, uint8_t Count);
int CLI_Commands(void);
int CLI_History(void);
int CLI_Run(void *);
void CLI_Clear(void);
uint8_t *CLI_GetLine(void);
void CLI_Prompt (void);
uint8_t CLI_Ia2i(char *str, int32_t *value);
uint8_t CLI_Ha2i(char *str, uint32_t *value);

#ifdef __cplusplus
}
#endif

#endif