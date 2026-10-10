#include <stdio.h> 
#include <stdlib.h> 
#include <string.h>
#include <unistd.h>
#include <readline/history.h>
#include <readline/readline.h>
#include "history.h"
#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "expand.h"
#include "builtin.h"
#include "executor.h"
#include "jobs.h"

/* =========================================================
   DYNAMIC SHELLFORGE PROMPT
   ========================================================= */

static char *get_shell_prompt(const char *root_dir)
{
    static char prompt[8192];
    char cwd[4096];

    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        snprintf(prompt, sizeof(prompt), "shellforge$ ");
        return prompt;
    }

    if (strcmp(cwd, root_dir) == 0)
    {
        snprintf(prompt, sizeof(prompt), "shellforge> ");
        return prompt;
    }

    size_t root_len = strlen(root_dir);

    if (strncmp(cwd, root_dir, root_len) == 0 && cwd[root_len] == '/')
    {
        const char *relative = cwd + root_len + 1;
        snprintf(prompt, sizeof(prompt), "shellforge/%s> ", relative);
        return prompt;
    }

    snprintf(prompt, sizeof(prompt), "shellforge/%s> ", cwd);
    return prompt;
}

int main(void)
{
    char root_dir[4096];
    if (getcwd(root_dir, sizeof(root_dir)) == NULL)
    {
        perror("getcwd");
        return EXIT_FAILURE;
    }

    // Display a welcome banner when the shell starts
    printf("=====================================\n");
    printf("      Shellforge \n");
    printf(" A Unix Style Shell written in C\n");
    printf("=====================================\n");

    /* =============================================
       INSTALL BACKGROUND PROCESS HANDLER & JOB CONTROL
       ============================================= */

    jobs_init();
    setup_background_handler();

    using_history();
    token_list_t tokens;
    pipeline_t pipeline;
    char *line;

    while (1)
    {
        char *prompt = get_shell_prompt(root_dir);
        line = readline(prompt);
        if (line == NULL)
        {
            printf("\nGoodbye!\n");
            break;
        }
        if (strlen(line) == 0)
        {
            free(line);
            continue;
        }

        if (strcmp(line, "history") == 0)
        {
            print_history();
            free(line);
            continue;
        }

        // milestone 1 - enabling history
        add_history(line);

        // milestone 2.1 - tokenization and lexer
        lexer(line, &tokens);

        // milestone 2.2 - expansion of environment variables and parser
        if (parser(&tokens, &pipeline))
        {
            expand_variables(&pipeline);
        }

        if (pipeline.command_count == 1 && pipeline.commands[0].argc > 0 && strcmp(pipeline.commands[0].argv[0], "exit") == 0)
        {
            free(line);
            break;
        }

        execute_pipeline(&pipeline);

        free(line);
    }
    return 0;
}
