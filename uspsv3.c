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
#include "bqueue.h"

// #include <stdio.h>

typedef struct _command {
    char** args;
} Command;

typedef struct _pcb {
    pid_t pid;
    int wasStarted;
    int isFinished;
} PCB;

BQueue* PCBs;
PCB* currentPCB;

// use this volatile variable to relay information from signal handler.
volatile int run = 0;

void signal_handler(int signo) {
    int alarmsEnabled = 1;
    void* pcb_holder;
    int sig;

    switch(signo) {
        case SIGUSR1:
            run = 1;
            break;

        case SIGALRM:
            if (alarmsEnabled) {
                alarmsEnabled = 0;
                if (currentPCB->isFinished) {
                    free(currentPCB);
                } else {
                    kill(currentPCB->pid, SIGSTOP);
                    bq_add(PCBs, currentPCB);
                    p1putstr(STDOUT_FILENO, "stopping ");
                    p1putint(STDOUT_FILENO, currentPCB->pid);
                    p1putstr(STDOUT_FILENO, "\n");
                }

                bq_remove(PCBs, &pcb_holder);
                currentPCB = (PCB*)pcb_holder;

                p1putstr(STDOUT_FILENO, "starting ");
                p1putint(STDOUT_FILENO, currentPCB->pid);
                p1putstr(STDOUT_FILENO, "\n");

                sig = currentPCB->wasStarted ? SIGCONT : SIGUSR1; 
                kill(currentPCB->pid, sig);
                currentPCB->wasStarted = 1;

                // printf("starting %d\n", currentPCB->pid); 

                alarmsEnabled = 1;
            }
            signal(SIGALRM, signal_handler); // c11 only listens to one SIGALRM, have to resubscribe everytime
            break;
    }
}

void subscribeToSignals() {
    signal(SIGUSR1, signal_handler);
    signal(SIGALRM, signal_handler);
}

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

int parseArgs(int argc, char* argv[], int* interval) {
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
    *interval = quantum;

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

void childHandler() {
    void* pcb_holder;
    pid_t expiredPid;
    int status;

    bq_remove(PCBs, &pcb_holder);
    currentPCB = (PCB*)pcb_holder;

    kill(currentPCB->pid, SIGUSR1);
    currentPCB->wasStarted = 1;
    // printf("starting %d\n", currentPCB->pid); 

    while((expiredPid = waitpid(-1, &status, WNOHANG)) != -1) {
        if (!expiredPid) {
            continue;
        }
        p1putstr(STDOUT_FILENO, "expiredPid: ");
        p1putint(STDOUT_FILENO, expiredPid);
        p1putstr(STDOUT_FILENO, "\n");
        // printf("expiredPid: %d status: %d\n", expiredPid, status);
        currentPCB->isFinished = 1;
    }
    free(currentPCB);
}

void executeWorkfile(int fd) {
    char line[255];
    int lineSize;
    ArrayList* commands = al_create(15); 

    while ((lineSize = p1getline(fd, line, 255)) > 1) { // construct Commands
        Command* cmd = createCmd(line);
        al_add(commands, cmd);
        // print_command(cmd);   
    }
    
    int numChildren = al_size(commands);
    PCBs = bq_create(numChildren); 
    int i;
    for (i = 0; i < numChildren; i++) {
        pid_t pid = fork();
        PCB* pcb = malloc(sizeof(PCB));
        pcb->pid = pid;
        pcb->wasStarted = 0;
        pcb->isFinished = 0;
        bq_add(PCBs, pcb);

        if (pid == 0) {
            while (run != 1) {
                pause();
            }
            void* command;
            al_get(commands, i, &command);

            execvp(((Command*)command)->args[0], ((Command*)command)->args);
            p1perror(STDERR_FILENO, "execvp FAILED!\n");
            exit(-1);

        } else if (pid < 0) {
            p1perror(STDERR_FILENO, "ERROR fork failed\n");
            exit(-1);
        }
    }

    al_destroy(commands, destroyCmd);
    close(fd);
}


int main(int argc, char* argv[]) {
    subscribeToSignals();
    int interval;
    int fd = parseArgs(argc, argv, &interval);

    executeWorkfile(fd);

    struct itimerval it_val;
    it_val.it_value.tv_sec = interval/1000;
    it_val.it_value.tv_usec = (interval*1000) % 1000000;
    it_val.it_interval = it_val.it_value;
    if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
        p1perror(STDERR_FILENO, "error calling setitimer()");
    }

    childHandler();
    bq_destroy(PCBs, free);
    
    return 0;
}