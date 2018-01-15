// Clayton kilmer, kilmer, this is my own work except the arraylist and bqueue implentations


#define _POSIX_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/time.h>
#include "p1fxns.h"
#include "arraylist.h"

#include <stdio.h>

typedef struct _command {
    // char* cmd;
    char** args;
} Command;

void print_command(Command *command) {
    int index =0;
    char *ptr = command->args[index];

    p1putstr(STDOUT_FILENO, "Command : ");
    while(ptr != NULL) {
        // p1strcat(ptr, " ");
        p1putstr(STDOUT_FILENO, ptr);
        p1putstr(STDOUT_FILENO, " ");
        // printf("%s ",ptr);
        index = index + 1;
        ptr = command->args[index];
    }
    p1putstr(STDOUT_FILENO, "\n");
}

Command* createCmd(char* line) {
    Command* cmd = (Command*) malloc(sizeof(Command));
    if (cmd == NULL) {
        exit(2);
    }

    int MAX_WORD_SIZE = 50; 
    int numArgs = 0;
    int lineIndex;
    lineIndex = p1strchr(line, '\n');
    if (lineIndex != -1) {
        line[lineIndex] = '\0'; 
    } 
    lineIndex = 0;

    char word[MAX_WORD_SIZE];
    while (lineIndex != -1) { // count numArgs in line
        lineIndex = p1getword(line, lineIndex, word);
        numArgs++;
    }
    lineIndex = 0;

    int i;
    char** args = malloc(sizeof(char*) * numArgs); 
    if (args == NULL) {
        exit(2);
    }
    for (i = 0; i < numArgs; i++) { 
        if (i == numArgs -1) {
            args[numArgs-1] = NULL;
        } else {
            args[i] = malloc(sizeof(char) * MAX_WORD_SIZE);
            if (args[i] == NULL) {
                exit(2);
            }
        }
    }

    lineIndex = p1getword(line, lineIndex, word);
    int argIndex = 0;
    while (lineIndex != -1) {
        p1strcpy(args[argIndex++], word);
        lineIndex = p1getword(line, lineIndex, word);
    }
    
    cmd->args = args;
    return cmd;
}

void destroyCmd(void* command) {
    Command* cmd = (Command*) command;
    int i = 0;
    char* arg = cmd->args[i++];
    while (arg != NULL) {
        free(arg);
        arg = cmd->args[i++];
    }
    free(cmd->args);
    free(cmd);
}

int parseArgs(int argc, char* argv[]) {
    int index;
    int quantum = 0;
    int fd;

    char* fileName = NULL;
    char* arg = NULL;

    if (argc < 4) {
        for (index = 1; index < argc; index++) {
            arg = argv[index];

            if (p1strneq("--quantum=", arg, 10)) {
                quantum = p1atoi(arg + 10);
            } else {
                fileName = arg;
            }
        }   
    } else {
        p1perror(STDERR_FILENO, "TOO MANY ARGS!\n");
        exit(1);
    }

    char* env = getenv("USPS_QUANTUM_MSEC");
    if (quantum <= 0 && !env) {
        p1perror(STDERR_FILENO, "ERROR USPS_QUANTUM_MSEC not specified or IMPOSSIBLE\n");
        exit(1);
    } 

    if (fileName != NULL) {
        // open fileName
        fd = open(fileName, O_RDONLY);
        if (fd < 0) {
            p1perror(STDERR_FILENO, "ERROR opeing file\n");
            exit(1);
        }
    } else {
        // "open" stdin
        fd = 0;
    } 
    return fd;
}

int main(int argc, char* argv[]) {
    int fd = parseArgs(argc, argv);


    char line[255];
    int lineSize;
    ArrayList* commands = al_create(15); 

    while (1) { // construct Commands
        lineSize = p1getline(fd, line, 255);
        if (lineSize < 1) {
            break;
        }
        Command* cmd = createCmd(line);
        al_add(commands, cmd);
        print_command(cmd);   
    }
    
    int numCommands = al_size(commands);
    pid_t pids[numCommands];
    int i;
    for (i = 0; i < numCommands; i++) {
        pids[i] = fork();

        if (pids[i] == 0) {
            void* command = NULL;
            al_get(commands, i, &command);

            execvp(((Command*)command)->args[0], ((Command*)command)->args);

            p1perror(STDERR_FILENO, "execvp FAILED!\n");
            exit(-1);

        } else if (pids[i] < 0) {
            p1perror(STDERR_FILENO, "ERROR fork failed\n");
            exit(-1);
        }
    }

    for (i = 0; i < numCommands; i++) {
        int status;
        if (waitpid(-1, &status, 0) >= 0) {
            if (WIFEXITED(status)) {
                p1putstr(STDOUT_FILENO, "Child process exited with status ");
                p1putint(STDOUT_FILENO, WEXITSTATUS(status));
                p1putstr(STDOUT_FILENO, "\n");
                // printf("Child process exited with status %d\n", WEXITSTATUS(status));
            }
        }
    }

    al_destroy(commands, destroyCmd);
    return 0;
}