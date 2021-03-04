#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))
#define BUF_SIZE 8192
#define READ_WRITE_EXECUTE 0777
#define NUM_SIGNALS 8

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens* tokens);
int cmd_help(struct tokens* tokens);
int cmd_cd(struct tokens* tokens);
int cmd_pwd(struct tokens* tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens* tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
    cmd_fun_t* fun;
    char* cmd;
    char* doc;
} fun_desc_t;

int signals[] = {SIGINT, SIGQUIT, SIGKILL, SIGTERM, SIGTSTP, SIGCONT, SIGTTIN, SIGTTOU};

fun_desc_t cmd_table[] = {
        {cmd_help, "?", "show this help menu"},
        {cmd_exit, "exit", "exit the command shell"},
        {cmd_cd, "cd", "change the current directory"},
        {cmd_pwd, "pwd", "print the current directory"},
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens* tokens) {
    for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
        printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
    return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens* tokens) { exit(0); }

/* Takes a token, validates it, and, if correct, changes the directory. */
int cmd_cd(struct tokens* tokens) {
    char* new_directory;
    int ret_val;

    if (tokens == NULL) {
        return -1;
    }

    new_directory = tokens_get_token(tokens, 1);
    if (new_directory == NULL) {
        perror("Invalid directory");
        return -1;
    } else if (strcmp(new_directory, "~") == 0) {
        new_directory = getenv("HOME");
    }

    ret_val = chdir(new_directory);
    if (ret_val == -1) {
        perror("Error changing directory");
    }
    return 1;
}

/* Takes a token, and, if valid, prints the current working directory. */
int cmd_pwd(struct tokens* tokens) {
    char buf[BUF_SIZE];
    char* ret_val;

    ret_val = getcwd(buf, sizeof(buf));
    if (ret_val == NULL) {
        perror("Error printing current working directory");
    }
    fprintf(stdout, "%s\n", buf);
    return 1;
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
    for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
        if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
            return i;
    return -1;
}

/* Initialization procedures for this shell */
void init_shell() {
    /* Our shell is connected to standard input. */
    shell_terminal = STDIN_FILENO;

    /* Check if we are running interactively */
    shell_is_interactive = isatty(shell_terminal);

    if (shell_is_interactive) {
        /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
         * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
         * foreground, we'll receive a SIGCONT. */
        while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
            kill(-shell_pgid, SIGTTIN);

        /* Saves the shell's process id */
        shell_pgid = getpid();

        /* Take control of the terminal */
        tcsetpgrp(shell_terminal, shell_pgid);

        /* Save the current termios to a variable, so it can be restored later. */
        tcgetattr(shell_terminal, &shell_tmodes);
    }
}

/* Sets the signals. */
void set_signals(void* val) {
    tcsetpgrp(0, getpid());
    for (int i = 0; i < NUM_SIGNALS; ++i) {
        signal(signals[i], val);
    }
}

/* Returns (pass by reference) the count and start of the second partition (zero if there is only one partition). */
void count_parts(char* input, int* count, int* partition_index) {
    const char DELIMITER = '|';

    for (*count = 0, *partition_index = 0; input[*count] != '\0'; *count += 1) {
        if (input[*count] == DELIMITER) {
            *partition_index = *count;
            break;
        }
    }
}

/* Fills array ARR with tokens. */
void tokens_to_arr(char* arr[], struct tokens* tokens, size_t token_length) {
    for (int i = 0; i < token_length; ++i) {
        arr[i] = tokens_get_token(tokens, i);
    }
    arr[token_length] = NULL;
}

/* Returns true if the path is absolute. */
bool is_absolute_path(char* curr_path) {
    for (int i = 0; i < sizeof(curr_path); ++i) {
        if (curr_path[i] == '/') {
            return true;
        }
    }
    return false;
}

/* Finds possible directories for a given relative path. */
char* find_potential_path(char* relative_path) {
    char curr_env[BUF_SIZE / 8];
    const char DELIMITER = ':';
    struct tokens* tokenized_paths;
    size_t num_paths;

    if (relative_path == NULL) {
        return NULL;
    }

    strcpy(curr_env, getenv("PATH"));
    /* Removing : and replacing it with spaces. This prepares input for tokenization. */
    for (int i = 0; i < sizeof(curr_env)/sizeof(char); ++i) {
        if (curr_env[i] == DELIMITER) {
            curr_env[i] = ' ';
        }
    }
    tokenized_paths = tokenize(curr_env);
    num_paths = tokens_get_length(tokenized_paths);

    for (int i = 0; i < num_paths; ++i) {
        char temp_delimiter[] = {'/', '\0'};
        char* temp_path = strcat(tokens_get_token(tokenized_paths, i), strcat(temp_delimiter, relative_path));
        if (access(temp_path, F_OK) != -1) {
            return temp_path;
        }
    }

    exit(1);
}

/* Processing '>' and '<' redirections, if any. */
int redirections_handler(char* program_args[]) {
    char temp_in_buf[BUF_SIZE / 64];
    char temp_out_buf[BUF_SIZE / 64];
    int file_desc;

    for (int i = 0; program_args[i] != NULL; ++i) {
        if (strcmp(">", program_args[i]) == 0) {
            strcpy(temp_out_buf, program_args[i + 1]);
            program_args[i] = NULL;
            file_desc = creat(temp_out_buf, READ_WRITE_EXECUTE);
            if (file_desc < 0) {
                perror("Error Opening Output File");
                return -1;
            }
            dup2(file_desc, STDOUT_FILENO);
            close(file_desc);

        } else if (strcmp("<", program_args[i]) == 0) {
            strcpy(temp_in_buf, program_args[i + 1]);
            program_args[i] = NULL;
            file_desc = open(temp_in_buf, O_RDONLY, 0);
            if (file_desc < 0) {
                perror("Error Opening Input File");
                return -1;
            }
            dup2(file_desc, STDIN_FILENO);
            close(file_desc);
        }
    }
    return 0;
}

/* Executes single command. */
int exec_single_program(char* input) {
    char* curr_path, *final_path;
    int input_len, ret_val;
    struct tokens* tks;

    /* Handling signals */
    setpgid(getpid(), getpid());
    set_signals(SIG_DFL);

    tks = tokenize(input);
    input_len = tokens_get_length(tks);
    curr_path = tokens_get_token(tks, 0);

    char* program_args[input_len + 1];
    tokens_to_arr(program_args, tks, input_len);
    final_path = (is_absolute_path(curr_path)) ? curr_path : find_potential_path(curr_path);

    ret_val = redirections_handler(program_args);
    if (ret_val != 0) {
        return ret_val;
    }

    execv(final_path, program_args);
    return 0;
}

/* Runs programs mentioned in the given input. */
int exec_programs(char* input) {
    int count, partition_index;
    int pipe_file_desc[2];
    char first_part[BUF_SIZE / 2];
    char other_parts[BUF_SIZE];
    char* offset_input;
    pid_t process_ID;

    if (input == NULL) {
        return -1;
    }
    count_parts(input, &count, &partition_index);

    if (partition_index) {
        if (pipe(pipe_file_desc) < 0) {
            perror("Pipe Creation Failed");
            return -1;
        }

        process_ID = fork();
        if (process_ID > 0) {
            close(pipe_file_desc[1]);
            dup2(pipe_file_desc[0], STDIN_FILENO);
            /* Copy the rest of input to buffer calculating the location of next token. We assume there is always spaces
             * to each side of '|'. */
            offset_input = 2 + input + count;
            strcpy(other_parts, offset_input);
            return exec_programs(other_parts);

        } else if (process_ID == 0) {
            close(pipe_file_desc[0]);
            dup2(pipe_file_desc[1], STDOUT_FILENO);

            /* Copying the first token. */
            strncpy(first_part, input, partition_index - 1);
            first_part[partition_index - 1] = '\0';
            return exec_single_program(first_part);
        } else {
            perror("Fork Failed");
            return -1;
        }
    } else {
        return exec_single_program(input);
    }
}

int main(unused int argc, unused char* argv[]) {
    int status;
    pid_t process_ID;
    init_shell();

    /* Handling signals */
    setpgid(getpid(), getpid());
    set_signals(SIG_IGN);

    static char line[4096];
    int line_num = 0;
    int ret_val = 0;

    /* Please only print shell prompts when standard input is not a tty */
    if (shell_is_interactive)
        fprintf(stdout, "%d: ", line_num);

    while (fgets(line, 4096, stdin)) {
        /* Split our line into words. */
        struct tokens* tokens = tokenize(line);

        /* Find which built-in function to run. */
        int fundex = lookup(tokens_get_token(tokens, 0));

        if (fundex >= 0) {
            cmd_table[fundex].fun(tokens);
        } else {
            /* Run commands as programs. */
            process_ID = fork();
            if (process_ID > 0) {
                /* Parent process waits. */
                wait(&status);
            } else if (process_ID == 0) {
                /* Child process executes programs. */
                ret_val = exec_programs(line);
            } else {
                perror("Main Fork Failed");
                return -1;
            }
        }

        if (shell_is_interactive)
            /* Please only print shell prompts when standard input is not a tty */
            fprintf(stdout, "%d: ", ++line_num);

        /* Clean up memory */
        tokens_destroy(tokens);

        set_signals(SIG_IGN);
    }

    return ret_val;
}
