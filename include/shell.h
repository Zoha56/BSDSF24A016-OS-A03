#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_LEN 512
#define MAXARGS 10
#define ARGLEN 30
#define PROMPT "FCIT> "
#define HISTORY_SIZE 20
#define MAX_JOBS 64
#define MAX_BLOCK_LINES 64

// Function prototypes
char** tokenize(char* cmdline);
int execute(char** arglist);
int execute_command_line(char* cmdline);
void execute_chained_commands(char* cmdline);
int handle_if_block(char* first_line);
int handle_builtin(char** arglist);
void add_to_history(const char* cmd);
void print_history(void);
const char* get_history_cmd(int index);
void add_job(pid_t pid, const char* cmd);
void remove_job(pid_t pid);
void reap_zombies(void);
void print_jobs(void);

// Feature 8: Shell Variables
void set_shell_var(const char* name, const char* value);
const char* get_shell_var(const char* name);
void print_shell_vars(void);
int try_variable_assignment(const char* token);
char* expand_variables_in_str(const char* str);

#endif // SHELL_H
