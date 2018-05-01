// Clayton kilmer, kilmer, this is my own work except the arraylist, bqueue, and p1fxns implentations

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

#include <stdio.h>

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

void parseProc(){ 
    pid_t pid = currentPCB->pid;
    char* files[] = {"/cmdline", "/stat", "/io"};
    char procPidPath[] = "/proc/";
    
    char pid_str[100]; 
    p1itoa(pid, pid_str);  
    p1strcat(procPidPath, pid_str);
    int pidPathLen = p1strlen(procPidPath);

    char fileName[pidPathLen + 9];
    char data[1000];
    char word[100];
    int wordCount = 0;
    int fd;
    int i = 0;
    int lineSize;
    printf("pid: %d\n", pid);

    for (int j = 0; j < 3; j++) {
        p1strcpy(fileName, procPidPath);
        p1strcat(fileName, files[j]);
        fd = open(fileName, O_RDONLY);
        if (j == 0) {
            lineSize = p1getline(fd, data, 255);
            while (++i < lineSize) {
                if (data[i] == '\0') {
                    data[i] = ' ';
                }
            }
            printf("cmdline: %s\n", data);
            
        } else if (j == 1) {
            p1getline(fd, data, 1000);
            printf("stats: ");
            while ((i = p1getword(data, i, word)) != -1) {
                wordCount++;
                switch (wordCount) {
                    case 14:
                        printf("usrModeTime: %s ", word);
                        break;
                    case 15:
                        printf("sysModeTime: %s ", word);
                        break;
                    case 23:
                        printf("virtualMemSize: %s ", word);
                        break;
                }
                if (wordCount > 23) {
                    break;
                }
            }
            printf("\n");
        } else if (j == 2) {
            while ((lineSize = p1getline(fd, data, 200)) > 1) {
                i = p1strchr(data, '\n');
                if (i != -1) {
                    data[i] = ' ';
                }
                p1strcat(data, " ");
                printf("%s ", data);
            }
            printf("\n");
        }
        close(fd);
    }
    printf("\n");
}

void signal_handler(int signo) {
    int alarmsEnabled = 1;
    void* tempVoidPCB;
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
                    printf("switching process:\n");
                    parseProc();
                }

                bq_remove(PCBs, &tempVoidPCB);
                currentPCB = (PCB*)tempVoidPCB;

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
    int index = 0;
    char* ptr = command->args[index];

    printf("Command : ");
    while(ptr != NULL) {
        printf("%s ",ptr);
        index++;
        ptr = command->args[index];
    }
    printf("\n");
}

Command* createCmd(char* line) {
    Command* cmd = (Command*) malloc(sizeof(Command));
    if (cmd == NULL) {
        exit(2);
    }

    int MAX_WORD_SIZE = 50; 
    int numArgs = 0;
    int lineIndex = p1strchr(line, '\n');
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

    char** args = malloc(sizeof(char*) * numArgs); 
    if (args == NULL) {
        exit(2);
    }

    for (int i = 0; i < numArgs; i++) { // creates enough space for a char* args[] like argv
        if (i == numArgs -1) {          // and null terminates it
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
    while (lineIndex != -1) { // copies each arg into its correct place in args
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

void emergencyCleanup(ArrayList* al, void (*arrayDestroy)(void* arrayElement), BQueue* bq, void (*bqueueDestroy)(void* bqElement), int fd) {
    al_destroy(al, arrayDestroy);
    bq_destroy(bq, bqueueDestroy);
    close(fd);
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
            printf("%s\n", fileName);
            p1perror(STDERR_FILENO, "ERROR opening file\n");
            exit(1);
        }
    } else {
        // "open" stdin
        fd = 0;
    } 
    return fd;
}

void childHandler() {
    void* tempPCB;
    pid_t expiredPid;
    int status;

    bq_remove(PCBs, &tempPCB);
    currentPCB = (PCB*)tempPCB;

    kill(currentPCB->pid, SIGUSR1);
    currentPCB->wasStarted = 1;
    parseProc();
    // printf("starting %d\n", currentPCB->pid); 

    while((expiredPid = waitpid(-1, &status, WNOHANG)) != -1) {
        if (!expiredPid) {
            continue;
        }
        printf("expiredPid: %d status: %d\n", expiredPid, status);
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
        print_command(cmd);   
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
            emergencyCleanup(commands, destroyCmd, PCBs, free, fd);
            exit(-2);

        } else if (pid < 0) {
            p1perror(STDERR_FILENO, "ERROR fork failed\n");
            emergencyCleanup(commands, destroyCmd, PCBs, free, fd);
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