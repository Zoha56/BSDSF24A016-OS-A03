#include "shell.h"

int execute(char* arglist[]) {
    int status;
    int cpid = fork();

    switch (cpid) {
        case -1:
            perror("fork failed");
            exit(1);
        case 0: // Child process
            execvp(arglist[0], arglist);
            perror("Command not found"); // This line runs only if execvp fails
            exit(1);
        default: // Parent process
            waitpid(cpid, &status, 0);
            if (WIFEXITED(status)) {
                return WEXITSTATUS(status);
            }
            return 1;
    }
}

// Helper: Split a single command segment into argv, input_file (<), and output_file (>)
static int parse_segment(char* seg, char** argv, char** infile, char** outfile) {
    *infile = NULL;
    *outfile = NULL;
    int argc = 0;
    char* p = seg;

    while (*p != '\0') {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;

        if (*p == '<' || *p == '>') {
            char op = *p++;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\0' || *p == '<' || *p == '>') {
                fprintf(stderr, "Syntax error near unexpected token `%c'\n", op);
                return -1;
            }
            char* start = p;
            while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '<' && *p != '>') {
                p++;
            }
            int len = p - start;
            char* fname = (char*)malloc(len + 1);
            strncpy(fname, start, len);
            fname[len] = '\0';

            if (op == '<') {
                free(*infile);
                *infile = fname;
            } else {
                free(*outfile);
                *outfile = fname;
            }
        } else {
            char quote = 0;
            char buf[MAX_LEN];
            int bpos = 0;

            while (*p != '\0') {
                if (quote) {
                    if (*p == quote) {
                        quote = 0;
                        p++;
                    } else {
                        buf[bpos++] = *p++;
                    }
                } else {
                    if (*p == '"' || *p == '\'') {
                        quote = *p++;
                    } else if (*p == ' ' || *p == '\t' || *p == '<' || *p == '>') {
                        break;
                    } else {
                        buf[bpos++] = *p++;
                    }
                }
            }
            buf[bpos] = '\0';
            if (argc < MAXARGS) {
                argv[argc++] = strdup(buf);
            }
        }
    }
    argv[argc] = NULL;
    return argc;
}

static void free_segment(char** argv, char* infile, char* outfile) {
    for (int i = 0; argv[i] != NULL; i++) {
        free(argv[i]);
    }
    free(infile);
    free(outfile);
}

static void apply_redirections(const char* infile, const char* outfile) {
    if (infile != NULL) {
        int fd_in = open(infile, O_RDONLY);
        if (fd_in < 0) {
            perror(infile);
            exit(1);
        }
        if (dup2(fd_in, STDIN_FILENO) < 0) {
            perror("dup2 input");
            close(fd_in);
            exit(1);
        }
        close(fd_in);
    }

    if (outfile != NULL) {
        int fd_out = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd_out < 0) {
            perror(outfile);
            exit(1);
        }
        if (dup2(fd_out, STDOUT_FILENO) < 0) {
            perror("dup2 output");
            close(fd_out);
            exit(1);
        }
        close(fd_out);
    }
}

int execute_command_line(char* cmdline) {
    if (cmdline == NULL) return 0;

    // Trim leading whitespace
    while (*cmdline == ' ' || *cmdline == '\t') cmdline++;
    if (*cmdline == '\0' || *cmdline == '#') return 0;

    // Save clean command string for background job display
    char job_cmd[MAX_LEN];
    strncpy(job_cmd, cmdline, MAX_LEN - 1);
    job_cmd[MAX_LEN - 1] = '\0';

    // Check for trailing '&' (background execution)
    int background = 0;
    int len = strlen(cmdline);
    while (len > 0 && (cmdline[len - 1] == ' ' || cmdline[len - 1] == '\t')) {
        cmdline[--len] = '\0';
    }
    if (len > 0 && cmdline[len - 1] == '&') {
        background = 1;
        cmdline[--len] = '\0';
        while (len > 0 && (cmdline[len - 1] == ' ' || cmdline[len - 1] == '\t')) {
            cmdline[--len] = '\0';
        }
    }
    if (cmdline[0] == '\0') return 0;

    // Check if there is a pipe '|' outside quotes
    char* pipe_pos = NULL;
    char quote = 0;
    for (char* p = cmdline; *p != '\0'; p++) {
        if (quote) {
            if (*p == quote) quote = 0;
        } else {
            if (*p == '"' || *p == '\'') quote = *p;
            else if (*p == '|') {
                pipe_pos = p;
                break;
            }
        }
    }

    // Case 1: Pipe command (left | right)
    if (pipe_pos != NULL) {
        *pipe_pos = '\0';
        char* left_str = cmdline;
        char* right_str = pipe_pos + 1;

        char* left_argv[MAXARGS + 1];
        char* left_in = NULL;
        char* left_out = NULL;

        char* right_argv[MAXARGS + 1];
        char* right_in = NULL;
        char* right_out = NULL;

        int l_argc = parse_segment(left_str, left_argv, &left_in, &left_out);
        int r_argc = parse_segment(right_str, right_argv, &right_in, &right_out);

        if (l_argc <= 0 || r_argc <= 0) {
            if (l_argc >= 0) free_segment(left_argv, left_in, left_out);
            if (r_argc >= 0) free_segment(right_argv, right_in, right_out);
            fprintf(stderr, "Invalid null command in pipe\n");
            return 1;
        }

        int pipefd[2];
        if (pipe(pipefd) < 0) {
            perror("pipe");
            free_segment(left_argv, left_in, left_out);
            free_segment(right_argv, right_in, right_out);
            return 1;
        }

        // Left child (writer)
        pid_t pid1 = fork();
        if (pid1 < 0) {
            perror("fork");
            close(pipefd[0]);
            close(pipefd[1]);
            free_segment(left_argv, left_in, left_out);
            free_segment(right_argv, right_in, right_out);
            return 1;
        }
        if (pid1 == 0) {
            close(pipefd[0]);
            if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
                perror("dup2");
                exit(1);
            }
            close(pipefd[1]);
            apply_redirections(left_in, left_out);
            if (handle_builtin(left_argv)) {
                exit(0);
            }
            execvp(left_argv[0], left_argv);
            perror("Command not found");
            exit(1);
        }

        // Right child (reader)
        pid_t pid2 = fork();
        if (pid2 < 0) {
            perror("fork");
            close(pipefd[0]);
            close(pipefd[1]);
            if (!background) waitpid(pid1, NULL, 0);
            free_segment(left_argv, left_in, left_out);
            free_segment(right_argv, right_in, right_out);
            return 1;
        }
        if (pid2 == 0) {
            close(pipefd[1]);
            if (dup2(pipefd[0], STDIN_FILENO) < 0) {
                perror("dup2");
                exit(1);
            }
            close(pipefd[0]);
            apply_redirections(right_in, right_out);
            if (handle_builtin(right_argv)) {
                exit(0);
            }
            execvp(right_argv[0], right_argv);
            perror("Command not found");
            exit(1);
        }

        close(pipefd[0]);
        close(pipefd[1]);

        int ret_status = 0;
        if (background) {
            add_job(pid2, job_cmd);
        } else {
            int st2;
            waitpid(pid1, NULL, 0);
            waitpid(pid2, &st2, 0);
            if (WIFEXITED(st2)) ret_status = WEXITSTATUS(st2);
            else ret_status = 1;
        }

        free_segment(left_argv, left_in, left_out);
        free_segment(right_argv, right_in, right_out);
        return ret_status;
    }

    // Case 2: Single command (with optional <, >, or &)
    char* argv[MAXARGS + 1];
    char* infile = NULL;
    char* outfile = NULL;

    int argc = parse_segment(cmdline, argv, &infile, &outfile);
    if (argc <= 0) {
        if (argc == 0) free_segment(argv, infile, outfile);
        return 0;
    }

    // Foreground command without redirection: preserve exact original flow
    if (!background && infile == NULL && outfile == NULL) {
        int ret_status = 0;
        if (!handle_builtin(argv)) {
            ret_status = execute(argv);
        }
        free_segment(argv, infile, outfile);
        return ret_status;
    }

    // Background command or command with redirection
    pid_t cpid = fork();
    if (cpid < 0) {
        perror("fork failed");
        free_segment(argv, infile, outfile);
        return 1;
    }
    if (cpid == 0) {
        apply_redirections(infile, outfile);
        if (handle_builtin(argv)) {
            exit(0);
        }
        execvp(argv[0], argv);
        perror("Command not found");
        exit(1);
    }

    int ret_status = 0;
    if (background) {
        add_job(cpid, job_cmd);
    } else {
        int status;
        waitpid(cpid, &status, 0);
        if (WIFEXITED(status)) ret_status = WEXITSTATUS(status);
        else ret_status = 1;
    }

    free_segment(argv, infile, outfile);
    return ret_status;
}

// Split input line by ';' (outside quotes) and execute sequentially
void execute_chained_commands(char* cmdline) {
    if (cmdline == NULL) return;

    char* start = cmdline;
    char quote = 0;

    for (char* p = cmdline; ; p++) {
        if (quote) {
            if (*p == quote) quote = 0;
            if (*p == '\0') {
                execute_command_line(start);
                break;
            }
        } else {
            if (*p == '"' || *p == '\'') {
                quote = *p;
            } else if (*p == ';' || *p == '\0') {
                char saved = *p;
                *p = '\0';
                execute_command_line(start);
                if (saved == '\0') break;
                start = p + 1;
            }
        }
    }
}

// Helper: Trim leading and trailing whitespace in-place
static char* trim_ws(char* s) {
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    int len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[--len] = '\0';
    }
    return s;
}

// Helper: Check if a line starts with exact keyword ("if", "then", "else", "fi")
static int is_keyword(const char* line, const char* kw) {
    size_t klen = strlen(kw);
    if (strncmp(line, kw, klen) != 0) return 0;
    char next = line[klen];
    return (next == '\0' || next == ' ' || next == '\t' || next == ';');
}

// Feature 7: Parse and execute multi-line if-then-else-fi structure
int handle_if_block(char* first_line) {
    char* p = first_line;
    while (*p == ' ' || *p == '\t') p++;
    if (!is_keyword(p, "if")) return 0;

    char cond_cmd[MAX_LEN] = "";
    char* then_cmds[MAX_BLOCK_LINES];
    char* else_cmds[MAX_BLOCK_LINES];
    int then_count = 0;
    int else_count = 0;

    // States: 0 = reading condition (waiting for 'then'), 1 = reading 'then' block, 2 = reading 'else' block
    int state = 0;

    // Extract condition after "if" on the first line
    char* after_if = trim_ws(p + 2);
    if (*after_if != '\0') {
        strncpy(cond_cmd, after_if, MAX_LEN - 1);
        cond_cmd[MAX_LEN - 1] = '\0';
    }

    // Read subsequent lines until 'fi'
    while (1) {
        char* line = readline("> ");
        if (line == NULL) {
            fprintf(stderr, "Syntax error: unexpected EOF while looking for matching `fi'\n");
            for (int i = 0; i < then_count; i++) free(then_cmds[i]);
            for (int i = 0; i < else_count; i++) free(else_cmds[i]);
            return 1;
        }

        // Save each entered line to Readline history
        char* trimmed = trim_ws(line);
        if (*trimmed != '\0') {
            add_history(line);
        }

        // Skip empty lines or comments
        if (*trimmed == '\0' || *trimmed == '#') {
            free(line);
            continue;
        }

        if (is_keyword(trimmed, "then") && state == 0) {
            state = 1;
            char* rest = trim_ws(trimmed + 4);
            if (*rest != '\0' && then_count < MAX_BLOCK_LINES) {
                then_cmds[then_count++] = strdup(rest);
            }
        } else if (is_keyword(trimmed, "else") && state == 1) {
            state = 2;
            char* rest = trim_ws(trimmed + 4);
            if (*rest != '\0' && else_count < MAX_BLOCK_LINES) {
                else_cmds[else_count++] = strdup(rest);
            }
        } else if (is_keyword(trimmed, "fi") && (state == 1 || state == 2)) {
            free(line);
            break;
        } else {
            if (state == 0) {
                if (cond_cmd[0] == '\0') {
                    strncpy(cond_cmd, trimmed, MAX_LEN - 1);
                    cond_cmd[MAX_LEN - 1] = '\0';
                }
            } else if (state == 1) {
                if (then_count < MAX_BLOCK_LINES) {
                    then_cmds[then_count++] = strdup(trimmed);
                }
            } else if (state == 2) {
                if (else_count < MAX_BLOCK_LINES) {
                    else_cmds[else_count++] = strdup(trimmed);
                }
            }
        }
        free(line);
    }

    if (cond_cmd[0] == '\0') {
        fprintf(stderr, "Syntax error: missing condition in `if' statement\n");
        for (int i = 0; i < then_count; i++) free(then_cmds[i]);
        for (int i = 0; i < else_count; i++) free(else_cmds[i]);
        return 1;
    }

    // Execute the condition command and inspect its exit status
    int exit_status = execute_command_line(cond_cmd);

    if (exit_status == 0) {
        for (int i = 0; i < then_count; i++) {
            execute_chained_commands(then_cmds[i]);
        }
    } else {
        for (int i = 0; i < else_count; i++) {
            execute_chained_commands(else_cmds[i]);
        }
    }

    for (int i = 0; i < then_count; i++) free(then_cmds[i]);
    for (int i = 0; i < else_count; i++) free(else_cmds[i]);
    return 1;
}
