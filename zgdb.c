#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <ncurses.h>
#include <pthread.h>

#define GDBPATH "/usr/bin/gdb"
#define MAX_BUFF 1024
#define MAX_INPUT 256

int gdbExists();
void gdbRun();
void gdbInterface();
void prompt();
void closeGDBRunPipes();
char *getInput();

// file descriptor for pipe
int gdb_output_fd[2];
int send_cmd_fd[2];

/* closeGDBRunPipes closes the pipe ends we don't need when 
 * communicating TO gdb */
void closeGDBRunPipes() {
    close(send_cmd_fd[1]);
    close(gdb_output_fd[0]);
}

/* closeGDBRunPipes closes the pipe ends we don't need when 
 * reading FROM gdb */
void closeGDBInterfacePipes() {
    close(gdb_output_fd[1]);
}

/* gdbExists is responsible for verifying gdb exist in the path provided. If no
 * path is provided it will use the default GDBPATH definition /usr/bin/gdb */
int gdbExists()
{
    FILE *file;
    if ((file = fopen(GDBPATH, "r")) != NULL) {
        fclose(file);
        return 1;
    }
    return 0;
}

/* gdbRun routes i/o to proper pipes and executes the gdb program */
void gdbRun()
{
    closeGDBRunPipes();

    /* setup pipes */
    dup2(send_cmd_fd[0], STDIN_FILENO);

    // OUTPUT OF GDB send std out and err to pipe
    dup2(gdb_output_fd[1], STDOUT_FILENO);
    dup2(gdb_output_fd[1], STDERR_FILENO); 

    execl(GDBPATH, "", NULL);
}

/* gdbInterface reads from gdb_output_fd and is responsible for 
 * interperetting gdb ouput and determining gdb state */
void gdbInterface()
{
    closeGDBInterfacePipes();

    int i, arr_mult;
    char *r_buf;
    char ch;

    r_buf = calloc(MAX_BUFF, 1);
    arr_mult = 1;
    i = 0;

    /* read from gdb exec pip */
    while(1) {
        read(gdb_output_fd[0], &ch, 1);
        printf("%c", ch);
        /* TODO: Need to build out interpereter here */
        
        /* dynamically grow array */
        if (i >= MAX_BUFF) {
            r_buf = realloc(r_buf, arr_mult * MAX_BUFF);
            memset(r_buf + MAX_BUFF, 0, MAX_BUFF);
            arr_mult++;
            i = 0;
        }
        r_buf[i] = ch; 
        i++;
    }

}

/* prompt is resonsible for interpretering what the user
 * inputs and piping it to gdb */
void prompt() {
    char *in;
    while(1) {
        printf("(zgdb): ");
        in = getInput();
        write(send_cmd_fd[1], in, strlen(in));
        usleep(10000);
    }
}

/* getInput callocs an input buffer of len MAX_INPUT then reads input from stdin 
 * in parent process and returns the char pointer */
char *getInput() {
    char *in;
    in = calloc(MAX_INPUT, sizeof(char));
    fgets(in, MAX_BUFF, stdin);
    if (strcmp(in, "q\n") == 0 || (strcmp(in, "quit\n") == 0)) {
        exit(EXIT_FAILURE);
    }
    return in;
}

int main()
{
    pid_t pid;
    // verify gdb exists
    if (gdbExists() != 1) {
        printf("could not find gdb");
        exit(EXIT_FAILURE);
    }

    // pipes for communication
    pipe(gdb_output_fd);
    pipe(send_cmd_fd);

    pid = fork();
    if (pid == 0) {
        /* child 1 -- runs gdb exec */
        gdbRun();
    } else {
        /* parent */
        pid = fork();
        if (pid == 0) {
            /* child 2 -- gdbInterface */
            gdbInterface();
        } else {
            /* parent -- prompt */
            prompt();
        }
    }

    return 0;
}


/*

      ┌────────────────┐
      │     parent     │
      └────────────────┘
               │
               │
            (fork) ─────┐
      ┌────────┘        │
      │                 │
      │                 ▼
      ▼          ┌──────────────┐
┌──────────┐     │    parent    │
│gdb binary│     └──────────────┘
└──────────┘             │
                       (fork)
                         ├──────────────────┐
                         │                  ▼
                         │        ┌──────────────────┐
                         │        │    zgdb prompt   │
                         ▼        └──────────────────┘
                ┌────────────────┐
                │ gdb interface  │
                └────────────────┘

*/