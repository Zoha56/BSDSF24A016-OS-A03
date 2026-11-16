#include "shell.h"

int main() {
    char* cmdline;
    char** arglist;

    while ((cmdline = read_cmd(PROMPT, stdin)) != NULL) {
        // Task 4: Handle !n before tokenization or adding to history
        char* p = cmdline;
        while (*p == ' ' || *p == '\t') p++;

        if (*p == '!') {
            char* endptr;
            long n = strtol(p + 1, &endptr, 10);
            while (*endptr == ' ' || *endptr == '\t') endptr++;

            if (p[1] == '\0' || *endptr != '\0' || n <= 0) {
                printf("Invalid history reference\n");
                free(cmdline);
                continue;
            }

            const char* hist_cmd = get_history_cmd((int)n);
            if (hist_cmd == NULL) {
                printf("No such command in history: !%ld\n", n);
                free(cmdline);
                continue;
            }

            printf("%s\n", hist_cmd);
            free(cmdline);
            cmdline = strdup(hist_cmd);
        }

        // Task 2: Add command to history
        add_to_history(cmdline);

        if ((arglist = tokenize(cmdline)) != NULL) {
            if (!handle_builtin(arglist)) {
                execute(arglist);
            }

            // Free the memory allocated by tokenize()
            for (int i = 0; arglist[i] != NULL; i++) {
                free(arglist[i]);
            }
            free(arglist);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
