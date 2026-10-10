#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "lexer.h"
#include "parser.h"
#include "builtin.h"


/* =========================================================
   DYNAMIC SHELLFORGE PROMPT
   ========================================================= */

static char *get_shell_prompt(const char *root_dir)
{
    static char prompt[8192];
    char cwd[4096];

    /*
     * Get current working directory
     */
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        snprintf(prompt, sizeof(prompt), "shellforge> ");
        return prompt;
    }

    /*
     * If we are in the original ShellForge directory
     */
    if (strcmp(cwd, root_dir) == 0)
    {
        snprintf(prompt, sizeof(prompt), "shellforge> ");
        return prompt;
    }

    /*
     * If we are inside the original ShellForge directory
     */
    size_t root_len = strlen(root_dir);

    if (strncmp(cwd, root_dir, root_len) == 0 &&
        cwd[root_len] == '/')
    {
        const char *relative = cwd + root_len + 1;

        snprintf(prompt,
                 sizeof(prompt),
                 "shellforge/%s> ",
                 relative);

        return prompt;
    }

    /*
     * If user moves outside the ShellForge directory
     */
    snprintf(prompt,
             sizeof(prompt),
             "shellforge/%s> ",
             cwd);

    return prompt;
}


/* =========================================================
   EXECUTE ONE EXTERNAL COMMAND
   ========================================================= */

static int execute_external(command_t *cmd)
{
    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return -1;
    }

    /* Child process */
    if (pid == 0)
    {
        /* -------------------------
           Input redirection
           ------------------------- */

        if (cmd->input[0] != '\0')
        {
            int fd = open(cmd->input, O_RDONLY);

            if (fd < 0)
            {
                perror(cmd->input);
                exit(EXIT_FAILURE);
            }

            if (dup2(fd, STDIN_FILENO) < 0)
            {
                perror("dup2");
                close(fd);
                exit(EXIT_FAILURE);
            }

            close(fd);
        }


        /* -------------------------
           Output redirection
           ------------------------- */

        if (cmd->output[0] != '\0')
        {
            int flags = O_WRONLY | O_CREAT;

            if (cmd->append)
                flags |= O_APPEND;
            else
                flags |= O_TRUNC;

            int fd = open(cmd->output, flags, 0644);

            if (fd < 0)
            {
                perror(cmd->output);
                exit(EXIT_FAILURE);
            }

            if (dup2(fd, STDOUT_FILENO) < 0)
            {
                perror("dup2");
                close(fd);
                exit(EXIT_FAILURE);
            }

            close(fd);
        }


        /* -------------------------
           Execute external command
           ------------------------- */

        execvp(cmd->argv[0], cmd->argv);

        /* execvp only returns on error */
        perror(cmd->argv[0]);
        exit(EXIT_FAILURE);
    }


    /* Parent process */

    if (cmd->background)
    {
        printf("[background pid %d]\n", pid);
    }
    else
    {
        waitpid(pid, NULL, 0);
    }

    return 0;
}


/* =========================================================
   EXECUTE PIPELINE
   ========================================================= */

static int execute_pipeline(pipeline_t *pipeline)
{
    int count = pipeline->command_count;

    /* -------------------------
       Empty pipeline
       ------------------------- */

    if (count <= 0)
        return 0;


    /* =====================================================
       SINGLE COMMAND
       ===================================================== */

    if (count == 1)
    {
        command_t *cmd = &pipeline->commands[0];

        if (cmd->argc == 0)
            return 0;

        /*
         * Built-in commands must execute in the parent
         * process for commands such as cd.
         */
        if (is_builtin(cmd))
        {
            return execute_builtin(cmd);
        }

        return execute_external(cmd);
    }


    /* =====================================================
       MULTIPLE COMMANDS / PIPELINE
       ===================================================== */

    int previous_pipe = -1;

    pid_t pids[MAX_COMMANDS];

    for (int i = 0; i < count; i++)
    {
        int pipefd[2] = {-1, -1};

        /*
         * Create pipe unless this is the last command.
         */
        if (i < count - 1)
        {
            if (pipe(pipefd) < 0)
            {
                perror("pipe");
                return -1;
            }
        }


        /* -------------------------
           Create child
           ------------------------- */

        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork");

            if (pipefd[0] != -1)
                close(pipefd[0]);

            if (pipefd[1] != -1)
                close(pipefd[1]);

            return -1;
        }


        /* =================================================
           CHILD
           ================================================= */

        if (pid == 0)
        {
            command_t *cmd = &pipeline->commands[i];


            /* -------------------------
               Connect previous pipe
               to stdin
               ------------------------- */

            if (previous_pipe != -1)
            {
                if (dup2(previous_pipe, STDIN_FILENO) < 0)
                {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }

                close(previous_pipe);
            }


            /* -------------------------
               Connect current pipe
               to stdout
               ------------------------- */

            if (i < count - 1)
            {
                close(pipefd[0]);

                if (dup2(pipefd[1], STDOUT_FILENO) < 0)
                {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }

                close(pipefd[1]);
            }


            /* -------------------------
               Input redirection
               ------------------------- */

            if (cmd->input[0] != '\0')
            {
                int fd = open(cmd->input, O_RDONLY);

                if (fd < 0)
                {
                    perror(cmd->input);
                    exit(EXIT_FAILURE);
                }

                if (dup2(fd, STDIN_FILENO) < 0)
                {
                    perror("dup2");
                    close(fd);
                    exit(EXIT_FAILURE);
                }

                close(fd);
            }


            /* -------------------------
               Output redirection
               ------------------------- */

            if (cmd->output[0] != '\0')
            {
                int flags = O_WRONLY | O_CREAT;

                if (cmd->append)
                    flags |= O_APPEND;
                else
                    flags |= O_TRUNC;

                int fd = open(cmd->output, flags, 0644);

                if (fd < 0)
                {
                    perror(cmd->output);
                    exit(EXIT_FAILURE);
                }

                if (dup2(fd, STDOUT_FILENO) < 0)
                {
                    perror("dup2");
                    close(fd);
                    exit(EXIT_FAILURE);
                }

                close(fd);
            }


            /* -------------------------
               Execute command
               ------------------------- */

            if (cmd->argc == 0)
                exit(EXIT_SUCCESS);


            /*
             * Built-ins inside a pipeline run
             * inside the child process.
             */
            if (is_builtin(cmd))
            {
                execute_builtin(cmd);
                exit(EXIT_SUCCESS);
            }


            /* External command */
            execvp(cmd->argv[0], cmd->argv);

            perror(cmd->argv[0]);
            exit(EXIT_FAILURE);
        }


        /* =================================================
           PARENT
           ================================================= */

        pids[i] = pid;


        /*
         * The parent no longer needs the previous pipe.
         */
        if (previous_pipe != -1)
        {
            close(previous_pipe);
        }


        /*
         * Keep the read end of the current pipe.
         * It becomes the previous pipe for the next command.
         */
        if (i < count - 1)
        {
            close(pipefd[1]);
            previous_pipe = pipefd[0];
        }
        else
        {
            previous_pipe = -1;
        }
    }


    /* =====================================================
       WAIT FOR PIPELINE
       ===================================================== */

    for (int i = 0; i < count; i++)
    {
        waitpid(pids[i], NULL, 0);
    }

    return 0;
}


/* =========================================================
   MAIN
   ========================================================= */

int main(void)
{
    char *input;

    /*
     * Store the directory from which ShellForge started.
     */
    char root_dir[4096];

    if (getcwd(root_dir, sizeof(root_dir)) == NULL)
    {
        perror("getcwd");
        return EXIT_FAILURE;
    }


    printf("=================================\n");
    printf("        ShellForge Shell\n");
    printf("=================================\n");


    /* =====================================================
       SHELL LOOP
       ===================================================== */

    while (1)
    {
        /*
         * Generate a dynamic prompt based on
         * the current working directory.
         */
        char *prompt = get_shell_prompt(root_dir);

        /*
         * Read command using GNU readline.
         */
        input = readline(prompt);


        /*
         * Ctrl+D / EOF
         */
        if (input == NULL)
        {
            printf("\n");
            break;
        }


        /*
         * Ignore empty commands.
         */
        if (input[0] == '\0')
        {
            free(input);
            continue;
        }


        /*
         * Add command to readline history.
         */
        add_history(input);


        /* =================================================
           LEXER
           ================================================= */

        token_list_t tokens;

        lexer(input, &tokens);


        /* =================================================
           PARSER
           ================================================= */

        pipeline_t pipeline;

        if (!parser(&tokens, &pipeline))
        {
            free(input);
            continue;
        }


        /* =================================================
           EXECUTION
           ================================================= */

        execute_pipeline(&pipeline);


        /*
         * Free memory allocated by readline.
         */
        free(input);
    }


    printf("ShellForge exiting...\n");

    return 0;
}
