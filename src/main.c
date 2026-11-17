#include "shell.h"

int main() {
    char* cmdline;

    // Enable default Readline filename/command tab completion
    rl_bind_key('\t', rl_complete);

    while (1) {
        // Reap any finished background processes before prompting
        reap_zombies();

        cmdline = readline(PROMPT);
        if (cmdline == NULL) {
            break;
        }

        // Handle !n before tokenization or adding to history
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

        // Save non-empty command to both Readline history and custom history
        if (*p != '\0') {
            add_history(cmdline);
            add_to_history(cmdline);
        }

        // Execute chained (;), background (&), redirected (<, >), or piped (|) commands
        execute_chained_commands(cmdline);

        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
