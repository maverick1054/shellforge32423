#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "executor.h"
#include "builtin.h"

/* =========================================================
   EXECUTE SINGLE COMMAND
   ========================================================= */

int execute_command(command_t *cmd)
{
    pid_t pid;
    int status;

    if (cmd == NULL || cmd->argc == 0)
    {
        return -1;
    }

    /* Built-in command */
    if (is_builtin(cmd))
    {
        return execute_builtin(cmd);
    }

    /* Create child */
    pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return -1;
    }

    /* Child process */
    if (pid == 0)
    {
        char *args[MAX_ARGS + 1];

        // Input redirection if specified
        if (cmd->input[0] != '\0')
        {
            int fd_in = open(cmd->input, O_RDONLY);
            if (fd_in < 0) { perror("open input"); _exit(EXIT_FAILURE); }
            if (dup2(fd_in, STDIN_FILENO) == -1) { perror("dup2 input"); _exit(EXIT_FAILURE); }
            close(fd_in);
        }
        // Output redirection if specified
        if (cmd->output[0] != '\0')
        {
            int flags = O_WRONLY | O_CREAT;
            if (cmd->append) flags |= O_APPEND; else flags |= O_TRUNC;
            int fd_out = open(cmd->output, flags, 0644);
            if (fd_out < 0) { perror("open output"); _exit(EXIT_FAILURE); }
            if (dup2(fd_out, STDOUT_FILENO) == -1) { perror("dup2 output"); _exit(EXIT_FAILURE); }
            close(fd_out);
        }

        for (int i = 0; i < cmd->argc; i++)
        {
            args[i] = cmd->argv[i];
        }
        args[cmd->argc] = NULL;

        execvp(args[0], args);
        perror(args[0]);
        _exit(127);
    }

    /* Parent process */
    if (cmd->background)
    {
        // Background job: do not wait for child
        return 0;
    }
    if (waitpid(pid, &status, 0) < 0)
    {
        perror("waitpid");
        return -1;
    }

    if (WIFEXITED(status))
    {
        return WEXITSTATUS(status);
    }

    return -1;
}

/* =========================================================
   EXECUTE PIPELINE
   ========================================================= */

int execute_pipeline(pipeline_t *pipeline)
{
    int previous_read = -1;
    pid_t pids[MAX_COMMANDS];
    int command_count;

    if (pipeline == NULL)
    {
        return -1;
    }

    command_count = pipeline->command_count;
    if (command_count == 0)
    {
        return -1;
    }

    if (command_count == 1)
    {
        return execute_command(&pipeline->commands[0]);
    }

    for (int i = 0; i < command_count; i++)
    {
        int pipefd[2];
        if (i < command_count - 1)
        {
            if (pipe(pipefd) == -1)
            {
                perror("pipe");
                return -1;
            }
        }

        pids[i] = fork();
        if (pids[i] < 0)
        {
            perror("fork");
            return -1;
        }

        if (pids[i] == 0)
        {
            command_t *cmd = &pipeline->commands[i];

            if (previous_read != -1)
            {
                if (dup2(previous_read, STDIN_FILENO) == -1)
                {
                    perror("dup2 input");
                    _exit(EXIT_FAILURE);
                }
            }

            if (i < command_count - 1)
            {
                if (dup2(pipefd[1], STDOUT_FILENO) == -1)
                {
                    perror("dup2 output");
                    _exit(EXIT_FAILURE);
                }
            }

            if (previous_read != -1)
            {
                close(previous_read);
            }
            if (i < command_count - 1)
            {
                close(pipefd[0]);
                close(pipefd[1]);
            }

// Input redirection if specified
            if (cmd->input[0] != '\0') {
                int fd_in = open(cmd->input, O_RDONLY);
                if (fd_in < 0) { perror("open input"); _exit(EXIT_FAILURE); }
                if (dup2(fd_in, STDIN_FILENO) == -1) { perror("dup2 input"); _exit(EXIT_FAILURE); }
                close(fd_in);
            }
            // Output redirection if specified
            if (cmd->output[0] != '\0') {
                int flags = O_WRONLY | O_CREAT;
                if (cmd->append) flags |= O_APPEND; else flags |= O_TRUNC;
                int fd_out = open(cmd->output, flags, 0644);
                if (fd_out < 0) { perror("open output"); _exit(EXIT_FAILURE); }
                if (dup2(fd_out, STDOUT_FILENO) == -1) { perror("dup2 output"); _exit(EXIT_FAILURE); }
                close(fd_out);
            }
            char *args[MAX_ARGS + 1];
            for (int j = 0; j < cmd->argc; j++) {
                args[j] = cmd->argv[j];
            }
            args[cmd->argc] = NULL;

            if (is_builtin(cmd))
            {
                int result = execute_builtin(cmd);
                _exit(result == 0 ? 0 : 1);
            }

            execvp(args[0], args);
            perror(args[0]);
            _exit(127);
        }

        if (previous_read != -1)
        {
            close(previous_read);
        }

        if (i < command_count - 1)
        {
            close(pipefd[1]);
            previous_read = pipefd[0];
        }
        else
        {
            previous_read = -1;
        }
    }

    // Check if any command in the pipeline is set to run in background
    int any_background = 0;
    for (int i = 0; i < command_count; i++) {
        if (pipeline->commands[i].background) {
            any_background = 1;
            break;
        }
    }
    if (any_background) {
        // Do not wait for background jobs; return success immediately
        return 0;
    }

    int final_status = 0;
    for (int i = 0; i < command_count; i++)
    {
        int status;
        if (waitpid(pids[i], &status, 0) < 0)
        {
            perror("waitpid");
            continue;
        }
        if (i == command_count - 1 && WIFEXITED(status))
        {
            final_status = WEXITSTATUS(status);
        }
    }
    return final_status;
}
