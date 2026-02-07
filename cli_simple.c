#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include "cli_simple.h"

#if ENABLE_CLI_COLOR
#define VT100_BOLD   "\e[1m"
#define VT100_NORMAL "\e[m"
#else
#define VT100_BOLD
#define VT100_NORMAL
#endif

static uint8_t CliLineBuffer[CLI_LINE_MAX_LEN];
static uint16_t CliLineLen;
static uint8_t *CliArgv[CLI_CMD_MAX_ARGS];
static uint32_t CliArgc;
static uint8_t CliEdit;

static uint16_t CliCommandsCount;
static cli_history_t History;
static const char *Prompt;
static const cli_command_t *CliCommands;


static int dummy_available(void) { return 0;}
static int dummy_read(char *buf, int len) {(void)buf; (void)len; return 0;}
static int dummy_write(const char *buf, int len) {(void)buf; (void)len; return 0;}

static stdinout_t default_stdinout = {
    .available = dummy_available,
    .read = dummy_read,
    .write = dummy_write
};

static const stdinout_t *stdinout;

// =============================================================================
// CLI_GetLine
// =============================================================================
/*!
 *
 * Get internal line buffer.
 * This can be useful to bypass ReadLine if a given character is pressed
 *
 * \return       - uint8_t pointer to internal line buffer
 *
 */
// =============================================================================
uint8_t *CLI_GetLine(void)
{
    return CliLineBuffer;
}

// =============================================================================
// CLI_SkipSpaces
// =============================================================================
/*!
 *
 * Skips spaces until next character of a string.
 *
 * \param Str    - Pointer to some string
 * \param MaxLen - Input string max length
 *
 * \return       - uint8_t pointer to next character on string
 *                 or begin of string if no spaces were found
 *
 */
// =============================================================================
static uint8_t * CLI_SkipSpaces (uint8_t *Str, uint32_t MaxLen)
{
   uint8_t *Start;

   if(!Str)
   {
      return NULL;
   }

   Start = Str;

   while (*Start == ' ' || *Start == '\t')
   {
      if(Start == Str + MaxLen || *Start == '\0')
      {
         // No spaces found return string start
         return Str;
      }
      Start++;
   }

   return Start;
}

// =============================================================================
// GetCommand
// =============================================================================
/*!
 *
 * Searches the command on the command list
 *
 * \param - Buffer  String containing command name
 *
 * \return - cli_command_t*  Pointer to command structure
 *
 */
// =============================================================================
static cli_command_t * CLI_GetCommand(uint8_t *Buffer)
{
   if(!Buffer){
      return NULL;
   }

   for (uint8_t i = 0; i < CliCommandsCount; i++)
   {
      if(!strcmp(CliCommands[i].name, (const char*) Buffer))
      {
         return (cli_command_t *)&CliCommands[i];
      }
   }

   return NULL;
}

// =============================================================================
// CLI_GetArguments
// =============================================================================
/*!
 *
 * Splits a command line string into arguments
 *
 * \param[in] - Buffer  String containing command and arguments
 * \param[out] - Argv    Arguments output array
 *
 * \return - Number of arguments
 *
 */
// =============================================================================
static uint32_t CLI_GetArguments(uint8_t *Buffer, uint8_t **Argv)
{
    uint8_t *Start, *End, ArgvIndex;

    if(!Buffer)
    {
        return 0;
    }

    // Skip any spaces before command

    Start = CLI_SkipSpaces (Buffer, CLI_LINE_MAX_LEN);

    // Check for empty line

    if(*Start == '\0')
    {
        return 0;
    }

    ArgvIndex = 0;

    Argv[ArgvIndex++] = End = Start;

    do{
        if(*End == ' ' || *End == '\t')
        {
            // Split argument
            *End = '\0';
            End = CLI_SkipSpaces(End + 1, CLI_LINE_MAX_LEN);
            Argv[ArgvIndex++] = End;
            continue;
        }

        End++;
    }while(*End != '\0' && End != Start + CLI_LINE_MAX_LEN);

    return ArgvIndex;
}

// =============================================================================
// CLI_ReplaceLine
// =============================================================================
/*!
 *
 * Replace current line on console
 *
 * \param - new_line    Replacing line
 *
 * \return - Replacing line length
 *
 */
// =============================================================================
static void CLI_ReplaceLine(uint8_t *new_line) {
    int new_line_len;

    if(!new_line){
        return;
    }

    new_line_len = strlen((const char*)new_line);

    if(new_line_len > 0 && new_line_len < CLI_LINE_MAX_LEN){
        memcpy(CliLineBuffer, new_line, new_line_len);

      if(CliEdit){
         printf("\e[%uC", CliEdit);
         CliEdit = 0;
      }

      while(CliLineLen--){
         printf("\b \b");
      }

      CliLineLen = new_line_len;

      printf("%s", new_line);
    }
}

// =============================================================================
// CLI_Prompt
// =============================================================================
/*!
 *
 * Prints cli prompt
 *
 * \param - None
 *
 * \return - void
 *
 */
// =============================================================================
void CLI_Prompt (void)
{
   printf(
      VT100_BOLD
      "%s "
      VT100_NORMAL
      ,Prompt);
}

// =============================================================================
// CLI_HistoryInit
// =============================================================================
/*!
 *
 * Prints registered commands alias
 *
 *
 * \param - void
 *
 *
 * \return - cli_result_t
 *
 */
// =============================================================================
static void CLI_HistoryInit(cli_history_t *Hist)
{
    uint16_t Index;

    if(!Hist){
        return;
    }

    Hist->head = 0;
    Hist->index = 0;
    Hist->size = 0;

    for(Index = 0; Index < CLI_HISTORY_SIZE; Index++){
        memset(Hist->history[Index], '\0', CLI_LINE_MAX_LEN);
    }
}

// =============================================================================
// CLI_HistoryDump
// =============================================================================
/*!
 *
 * Prints registered history list
 *
 *
 * \param - Hist    history structure
 *
 *
 * \return - void
 *
 */
// =============================================================================
static void CLI_HistoryDump(cli_history_t *Hist) {
    uint16_t Index;

    if(!Hist){
        return;
    }

    for (Index = 0; Index < CLI_HISTORY_SIZE; Index++)
    {
        printf("\n%c %u %s", (Index == Hist->head) ? '>' : ' ', Index, Hist->history[Index]);
    }

    putchar('\n');
    putchar('\n');
}

// =============================================================================
// CLI_HistoryAdd
// =============================================================================
/*!
 *
 * Adds entry to history
 *
 *
 * \param - Hist    history structure
 * \param - line    Line to be added
 *
 * \return - void
 *
 */
// =============================================================================
static void CLI_HistoryAdd(cli_history_t *Hist, uint8_t *line)
{
    uint16_t Index;

    if(!Hist || !line){
        return;
    }

    if (*line != '\n' && *line != '\r' && *line != '\0') {

        for(Index = 0; Index < CLI_LINE_MAX_LEN - 1; Index++)
        {
            if(line[Index] == '\0')
            {
                break;
            }

            Hist->history[Hist->head][Index] = line[Index];
        }

        Hist->history[Hist->head][Index] = '\0';

        Hist->head = (Hist->head + 1) % CLI_HISTORY_SIZE;

        Hist->index = Hist->head;

        if (Hist->size < CLI_HISTORY_SIZE)
        {
            Hist->size++;
        }
    }
}

// =============================================================================
// CLI_HistoryGet
// =============================================================================
/*!
 *
 * Returns history entry relative to current one
 *
 *
 * \param - Hist    history structure
 * \param - Dir     Direction -1 previous entry, 1 next entry, 0 current entry
 *
 * \return - void   selected entry
 *
 */
// =============================================================================
static uint8_t *CLI_HistoryGet(cli_history_t *Hist, int8_t Dir)
{
    uint16_t CurIndex;

    if(!Hist){
        return NULL;
    }

    CurIndex = Hist->index;

    if(Dir == -1)
    {
        if (Hist->size == CLI_HISTORY_SIZE)
        {
            // History is full, wrap arround is allowed
            if (--CurIndex > CLI_HISTORY_SIZE)
            {
                CurIndex = CLI_HISTORY_SIZE - 1;
            }

            // Stop going back if we are back on current entry
            if (CurIndex != Hist->head)
            {
                Hist->index = CurIndex;
            }
        }
        else if(Hist->index > 0)
        {
            Hist->index--;
        }
    }
    else if (Dir == 1)
    {
        if (CurIndex != Hist->head) {
            CurIndex = (CurIndex + 1) % CLI_HISTORY_SIZE;
        }

        if(CurIndex == Hist->head){
            // Clear current line to avoid duplicating history navigation
            memset(Hist->history[CurIndex], '\0', CLI_LINE_MAX_LEN);
        }

        Hist->index = CurIndex;
    }

    return Hist->history[Hist->index];
}

// =============================================================================
// CLI_History
// =============================================================================
/*!
 *
 * Wrapper for CLI_HistoryDump
 *
 *
 * \param - void
 *
 *
 * \return - cli_result_t
 *
 */
// =============================================================================
int CLI_History(void)
{
    CLI_HistoryDump(&History);
    return CLI_OK;
}

// =============================================================================
// CLI_Init
// =============================================================================
/*!
 *
 * Initialize command line interface
 *
 *
 * \param - Prompt
 *
 * \return - void
 *
 */
// =============================================================================
void CLI_Init (const char *prompt, const stdinout_t *io)
{
   memset (CliLineBuffer, 0x0, sizeof (CliLineBuffer));
   CliLineLen = 0;
   CliEdit = 0;

   stdinout = io ? io : &default_stdinout;

   Prompt = (prompt == NULL) ? "cli>" : prompt;

   setvbuf(stdout, NULL, _IONBF, 0); // make stdout non-buffered

   CLI_HistoryInit(&History);
}

// =============================================================================
// CLI_RegisterCommand
// =============================================================================
/*!
 *
 * Registers commands that can be executed by cli
 *
 *
 * \param - Commands    List of commands
 * \param - Count       Number of commands in list
 *
 * \return - void
 *
 */
// =============================================================================
void CLI_RegisterCommand (const cli_command_t *Commands, uint8_t Count)
{
   if(Commands)
   {
      CliCommands = Commands;
      CliCommandsCount = Count;
   }
}

// =============================================================================
// CLI_Commands
// =============================================================================
/*!
 *
 * Prints registered commands alias
 *
 *
 * \param - void
 *
 *
 * \return - cli_result_t
 *
 */
// =============================================================================
int CLI_Commands (void)
{
   for (int i = 0; i < CliCommandsCount; i++)
   {
      printf("\n %s", CliCommands[i].name);
   }

   putchar('\n');
   putchar('\n');

   return CLI_OK;
}
// =============================================================================
// CLI_ProcessLine
// =============================================================================
/*!
 *
 * command line command by spliting it into arguments and
 * executing the corresponding command
 *
 * \param[in] line - pointer to command line,
 *
 * \return - cli_result_t
 *
 */
// =============================================================================
cli_result_t CLI_ProcessLine (uint8_t *line)
{
   cli_result_t Res;

   CliArgc = CLI_GetArguments(line, CliArgv);

   if(CliArgc > CLI_CMD_MAX_ARGS){
      Res = CLI_BAD_PARAM;
   }else{
      const cli_command_t *Cmd = CLI_GetCommand(CliArgv[0]);
      if(Cmd != NULL){
         Res = Cmd->exec(CliArgc, (char**)CliArgv);
      }else{
         // Distinguish invalid command and empty line
         Res = (CliArgc) ? CLI_CMD_NOT_FOUND : CLI_OK;
      }
   }

   switch(Res){
      case CLI_CMD_NOT_FOUND:
         puts("command not found");
         break;

      case CLI_BAD_PARAM:
         puts("Invalid parameter");
         break;

      case CLI_MISSING_ARGS:
         puts("Missing parameter");
         break;

      case CLI_OK_LF:
         putchar('\n');
         break;

      default:
         break;
   }

   return Res;
}

// =============================================================================
// CLI_HandleLine
// =============================================================================
/*!
 *
 * Handle command line entered by user and adds it to history
 *
 * \param - None
 *
 * \return - cli_result_t
 *
 */
// =============================================================================
cli_result_t CLI_HandleLine (void)
{
   CLI_HistoryAdd(&History, CliLineBuffer);

   cli_result_t Res = CLI_ProcessLine(CliLineBuffer);

   // Parse splits initial line, it must be cleared in all its length
   memset (CliLineBuffer, '\0', CLI_LINE_MAX_LEN);
   CliLineLen = 0;
   CliEdit = 0;

   // Also clear parameters pointers
   memset (CliArgv, '\0', sizeof(CliArgv));
   CliArgc = 0;

   CLI_Prompt ();

   return Res;
}

// =============================================================================
// CLI_ReadLine
// =============================================================================
/*!
 *
 * Read input line by user, this call is not blocking
 *
 *
 * \param - void
 *
 * \return - cli_result_t
 *
 */
// =============================================================================
cli_result_t CLI_ReadLine (void)
{
   static uint8_t EscSeq = 0;

   if (stdinout->available())
   {
      uint8_t Data;

      stdinout->read((char*)&Data, 1);

      if (EscSeq == 1)
      {
         if (Data == '[')
         {
            EscSeq++;
            return CLI_OK;
         }
         EscSeq = 0;
      }
      else if (EscSeq == 2)
      {
         switch (Data)
         {
            case 'A':
               //puts ("UP");
               CLI_ReplaceLine(CLI_HistoryGet(&History, -1));
               break;

            case 'B':
               //puts ("DOWN");
               CLI_ReplaceLine(CLI_HistoryGet(&History, 1));
               break;

            case 'C':
               //puts ("RIGTH");
               if(CliEdit > 0){
                  printf("\e[1C");
                  CliEdit--;
               }
               break;
            case 'D':
               //puts ("LEFT");
               if(CliEdit < CliLineLen){
                  printf("\e[1D");
                  CliEdit++;
               }
               break;
            default:
               break;
         }
         EscSeq = 0;
         return CLI_OK;
      }

      switch (Data)
      {
         case 0:
            break;

         case '\e':
         {
            EscSeq++;
            break;
         }

         case 0x7F:
         case '\b':
         {
            if (CliLineLen > 0)
            {
               if(!CliEdit){
                  putchar('\b');
                  putchar(' ');
                  putchar('\b');
               }else{
                  uint8_t offset = CliLineLen - CliEdit;

                  if((CliLineLen - CliEdit) == 0){
                     // We are at prompt end, dont allow backspace
                     break;
                  }

                  // move cursor one character to left
                  putchar('\b');

                  // Move and print remaning string
                  for(uint8_t i = 0; i < CliEdit; i++){
                     CliLineBuffer[offset + i - 1] = CliLineBuffer[offset + i];
                     putchar (CliLineBuffer[offset + i]);
                  }

                  // Erase last character
                  putchar(' ');
                  // Move cursor back to edit position
                  printf("\e[%uD", CliEdit + 1);
               }
               CliLineLen--;
            }
            break;
         }

         case '\n':
         case '\r':
         {
            CliLineBuffer[CliLineLen] = '\0';
            puts ("");
            return CLI_LINE_READ;
         }

         default:
         {
            if (CliLineLen < sizeof (CliLineBuffer))
            {
               if(!CliEdit){
                  CliLineBuffer[CliLineLen++] = Data;
                  putchar (Data);
               }else{
                  uint8_t offset = CliLineLen - CliEdit;

                  // Move and print remaning string in buffer
                  for(uint8_t i = 0; i < CliEdit; i++){
                     CliLineBuffer[CliLineLen - i] = CliLineBuffer[CliLineLen - 1 - i];
                  }
                  // Insert entered character and increment current line len
                  CliLineBuffer[offset] = Data;

                  // Print remanig string with new character already inserted
                  for(uint8_t i = 0; i < CliEdit + 1; i++){
                     putchar(CliLineBuffer[offset + i]);
                  }

                  // Move cursor back to edit position
                  printf("\e[%uD", CliEdit);

                  CliLineLen++;
               }
            }
            break;
         }
      }
   }
   return CLI_OK;
}

// =============================================================================
// CLI_Run ()
// =============================================================================
/*!
 *
 * Continuously processes cli, this is intended to be used by a thread from OS
 *
 * \param func - Function pointer for thread yield or other processes execution.
 *
 * \return - exit status
 *
 */
// =============================================================================
int CLI_Run(void (*func)(void))
{
    CLI_Prompt ();

    while(1)
    {
        if(CLI_ReadLine() == CLI_LINE_READ){
            if(CLI_HandleLine() == CLI_EXIT){
                break;
            }
        }

        if(func){
            func();
        }
    }

    return 0;
}

void CLI_Clear(void)
{
    printf("\e[2J\r");
}


/**
 * Try to parse a string representing a integer to integer value
 *
 * \param  str - pointer to input string
 * \param  value - pointer to output value
 * \return number of converted digits
 * */
uint8_t CLI_Ia2i(char *str, int32_t *value) {
    int val = 0;
    char c;
    uint8_t s = 0;

    if(str == NULL){
        return 0;
    }

    if(*str == '\0'){
        return 0;
    }

    c = *str;

    if (c == '-') {
        s = (1 << 7); // Set signal flag
        str++;
        c = *str;
    }

    do{
        if (c > '/' && c < ':') {
            c -= '0';
            val = val * 10 + c;
            s++;
        }
        else {
            return 0;
        }
        c = *(++str);
    }while (c != ' ' && c != '\n' && c != '\r' && c != '\0');

    // check signal flag
    *value = (s & (1 << 7)) ? -val : val;

    return s & 0x7F;
}


/**
 * Try to parse a string representing a hex number to integer value
 *
 * \param  str	pointer to input string
 * \param  value  pointer to output value
 * \return 1 if success, 0 if failed
 * */
uint8_t CLI_Ha2i(char *str, uint32_t *value) {
    uint32_t val = 0;
    char c;

    if(str == NULL){
        return 0;
    }

    if(*str == '\0'){
        return 0;
    }

    c = *str;

    do {
        val <<= 4;
        if (c > '`' && c < 'g') {
            c -= 'W';
        }
        else if ((c > '@' && c < 'G')) {
            c -= '7';
        }
        else if (c > '/' && c < ':') {
            c -= '0';
        }
        else {
            return 0;
        }

        val |= c;
        c = *(++str);

    } while (c != '\0' && c != ' ' && c != '\n' && c != '\r');

    *value = val;
    return 1;
}
