#include "shell.h"

static char* history[HISTORY_SIZE];
static int history_count = 0;

typedef struct {
    int id;
    pid_t pid;
    char cmd[MAX_LEN];
    int active;
} Job;

static Job jobs_list[MAX_JOBS];
static int next_job_id = 1;

// Feature 8: Linked list node for shell variables
typedef struct VarNode {
    char* name;
    char* value;
    struct VarNode* next;
} VarNode;

static VarNode* var_head = NULL;

void set_shell_var(const char* name, const char* value) {
    if (name == NULL || name[0] == '\0') return;

    VarNode* curr = var_head;
    while (curr != NULL) {
        if (strcmp(curr->name, name) == 0) {
            free(curr->value);
            curr->value = strdup(value ? value : "");
            return;
        }
        curr = curr->next;
    }

    VarNode* node = (VarNode*)malloc(sizeof(VarNode));
    if (node == NULL) return;
    node->name = strdup(name);
    node->value = strdup(value ? value : "");
    node->next = var_head;
    var_head = node;
}

const char* get_shell_var(const char* name) {
    if (name == NULL || name[0] == '\0') return NULL;

    VarNode* curr = var_head;
    while (curr != NULL) {
        if (strcmp(curr->name, name) == 0) {
            return curr->value;
        }
        curr = curr->next;
    }
    return getenv(name);
}

void print_shell_vars(void) {
    if (var_head == NULL) {
        printf("No shell variables defined.\n");
        return;
    }
    VarNode* curr = var_head;
    while (curr != NULL) {
        printf("%s=%s\n", curr->name, curr->value);
        curr = curr->next;
    }
}

// Check if token is a valid VARNAME=value assignment (no spaces around '=')
int try_variable_assignment(const char* token) {
    if (token == NULL) return 0;

    const char* eq = strchr(token, '=');
    if (eq == NULL || eq == token) return 0;

    // Validate variable name: must start with letter or '_' and contain only alphanumeric or '_'
    if (!(isalpha((unsigned char)token[0]) || token[0] == '_')) {
        return 0;
    }
    for (const char* p = token + 1; p < eq; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_')) {
            return 0;
        }
    }

    size_t name_len = (size_t)(eq - token);
    char* name = (char*)malloc(name_len + 1);
    strncpy(name, token, name_len);
    name[name_len] = '\0';

    // Expand any $VAR inside the assigned value
    char* expanded_val = expand_variables_in_str(eq + 1);
    set_shell_var(name, expanded_val);

    free(name);
    free(expanded_val);
    return 1;
}

// Expand all $VARNAME occurrences in a string (allocates and returns a new string)
char* expand_variables_in_str(const char* str) {
    if (str == NULL) return NULL;

    size_t cap = MAX_LEN * 2;
    char* out = (char*)malloc(cap);
    size_t opos = 0;

    for (size_t i = 0; str[i] != '\0'; ) {
        if (str[i] == '$' && (isalpha((unsigned char)str[i + 1]) || str[i + 1] == '_')) {
            size_t start = i + 1;
            size_t end = start;
            while (isalnum((unsigned char)str[end]) || str[end] == '_') {
                end++;
            }
            size_t vlen = end - start;
            char* vname = (char*)malloc(vlen + 1);
            strncpy(vname, str + start, vlen);
            vname[vlen] = '\0';

            const char* val = get_shell_var(vname);
            free(vname);

            if (val != NULL) {
                size_t val_len = strlen(val);
                while (opos + val_len + 2 >= cap) {
                    cap *= 2;
                    out = (char*)realloc(out, cap);
                }
                memcpy(out + opos, val, val_len);
                opos += val_len;
            }
            i = end;
        } else {
            if (opos + 2 >= cap) {
                cap *= 2;
                out = (char*)realloc(out, cap);
            }
            out[opos++] = str[i++];
        }
    }
    out[opos] = '\0';
    return out;
}

void add_job(pid_t pid, const char* cmd) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!jobs_list[i].active) {
            jobs_list[i].id = next_job_id++;
            jobs_list[i].pid = pid;
            strncpy(jobs_list[i].cmd, cmd, MAX_LEN - 1);
            jobs_list[i].cmd[MAX_LEN - 1] = '\0';
            jobs_list[i].active = 1;
            printf("[%d] %d\n", jobs_list[i].id, pid);
            return;
        }
    }
    fprintf(stderr, "Maximum background jobs limit reached.\n");
}

void remove_job(pid_t pid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs_list[i].active && jobs_list[i].pid == pid) {
            jobs_list[i].active = 0;
            break;
        }
    }
    int any_active = 0;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs_list[i].active) {
            any_active = 1;
            break;
        }
    }
    if (!any_active) {
        next_job_id = 1;
    }
}

void reap_zombies(void) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        remove_job(pid);
    }
}

void print_jobs(void) {
    reap_zombies();
    int found = 0;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs_list[i].active) {
            printf("[%d]  Running                 %s (PID: %d)\n",
                   jobs_list[i].id, jobs_list[i].cmd, jobs_list[i].pid);
            found = 1;
        }
    }
    if (!found) {
        printf("No active background jobs.\n");
    }
}

void add_to_history(const char* cmd) {
    if (cmd == NULL || cmd[0] == '\0') return;

    int only_space = 1;
    for (int i = 0; cmd[i] != '\0'; i++) {
        if (cmd[i] != ' ' && cmd[i] != '\t' && cmd[i] != '\n') {
            only_space = 0;
            break;
        }
    }
    if (only_space) return;

    if (history_count < HISTORY_SIZE) {
        history[history_count++] = strdup(cmd);
    } else {
        free(history[0]);
        for (int i = 1; i < HISTORY_SIZE; i++) {
            history[i - 1] = history[i];
        }
        history[HISTORY_SIZE - 1] = strdup(cmd);
    }
}

void print_history(void) {
    for (int i = 0; i < history_count; i++) {
        printf("%d  %s\n", i + 1, history[i]);
    }
}

const char* get_history_cmd(int index) {
    if (index < 1 || index > history_count) {
        return NULL;
    }
    return history[index - 1];
}

char** tokenize(char* cmdline) {
    // Edge case: empty command line
    if (cmdline == NULL || cmdline[0] == '\0' || cmdline[0] == '\n') {
        return NULL;
    }

    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    for (int i = 0; i < MAXARGS + 1; i++) {
        arglist[i] = (char*)malloc(sizeof(char) * ARGLEN);
        bzero(arglist[i], ARGLEN);
    }

    char* cp = cmdline;
    char* start;
    int len;
    int argnum = 0;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++; // Skip leading whitespace
        
        if (*cp == '\0') break; // Line was only whitespace

        start = cp;
        len = 1;
        while (*++cp != '\0' && !(*cp == ' ' || *cp == '\t')) {
            len++;
        }
        strncpy(arglist[argnum], start, len);
        arglist[argnum][len] = '\0';
        argnum++;
    }

    if (argnum == 0) { // No arguments were parsed
        for(int i = 0; i < MAXARGS + 1; i++) free(arglist[i]);
        free(arglist);
        return NULL;
    }

    arglist[argnum] = NULL;
    return arglist;
}

int handle_builtin(char** arglist) {
    if (arglist == NULL || arglist[0] == NULL) {
        return 0;
    }

    // Feature 8: Handle variable assignment (e.g., VARNAME=value) as a built-in action
    if (arglist[1] == NULL && try_variable_assignment(arglist[0])) {
        return 1;
    }

    if (strcmp(arglist[0], "exit") == 0) {
        exit(0);
    }

    if (strcmp(arglist[0], "cd") == 0) {
        if (arglist[1] == NULL) {
            char* home = getenv("HOME");
            if (home == NULL || chdir(home) != 0) {
                perror("cd");
            }
        } else {
            if (chdir(arglist[1]) != 0) {
                perror("cd");
            }
        }
        return 1;
    }

    if (strcmp(arglist[0], "help") == 0) {
        printf("MyShell Built-in Commands:\n");
        printf("  cd <dir>    - Change the current working directory\n");
        printf("  exit        - Terminate the shell gracefully\n");
        printf("  help        - Display this help message\n");
        printf("  jobs        - List active background jobs\n");
        printf("  history     - Display command history (use !n to re-execute) (use !n to re-execute)\n");
        printf("  set         - Display all defined shell variables\n");
        printf("  VAR=VALUE   - Assign a value to a shell variable\n");
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        print_jobs();
        return 1;
    }

    if (strcmp(arglist[0], "history") == 0) {
        print_history();
        return 1;
    }

    if (strcmp(arglist[0], "set") == 0) {
        print_shell_vars();
        return 1;
    }

    return 0;
}
