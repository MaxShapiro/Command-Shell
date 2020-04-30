#include "LineParser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_INPUT_BYTES 2048

#define TERMINATED  -1
#define RUNNING 1
#define SUSPENDED 0

typedef struct process{
    cmdLine* cmd;                         /* the parsed command line*/
    pid_t pid; 		                  /* the process id that is running the command*/
    int status;                           /* status of the process: RUNNING/SUSPENDED/TERMINATED */
    struct process *next;	                  /* next process in chain */
} process;

void addProcess(process** process_list, cmdLine* cmd, pid_t pid){
    // making a new process struct
    int status;
    process* newProcess = malloc(sizeof(*newProcess));
    newProcess->cmd = cmd;
    newProcess->pid = pid;
    newProcess->status = 1;
    
    //adding the new process
    if (*process_list==NULL){
        newProcess->next = NULL;
        *process_list = newProcess; 
    }
    else{
        process* tmp = *process_list;
        while(tmp->next!=NULL){
            tmp = tmp->next;
        }
        tmp->next = newProcess;
        newProcess->next = NULL;
    }
}

void freeProcessList(process* process_list){
    process* toDelete = process_list->next;
    freeCmdLines(process_list->cmd);
    free(process_list);
    if (toDelete!=NULL){
        freeProcessList(toDelete);
    }
}

void updateProcessStatus(process* process_list, int pid, int status){
    process* tmp = process_list;
    while(tmp!=NULL){
        if (tmp->pid == pid){
            tmp->status = status;
        }
        tmp = tmp->next;
    }
}

void updateProcessList(process **process_list){
    int status;
    pid_t waitVal;
    process* tmp = *process_list;
    while (tmp!=NULL){
        waitVal = waitpid(tmp->pid, &status, WNOHANG);
        if (waitVal == -1){
            updateProcessStatus(tmp, tmp->pid, -1);
        }
        if (waitVal == tmp->pid){
            if (WIFCONTINUED(status)){
                updateProcessStatus(tmp, tmp->pid, 1);
            }
            if (WIFSTOPPED(status)){
                updateProcessStatus(tmp, tmp->pid, 0);
            }
            else{
                updateProcessStatus(tmp, tmp->pid, -1);
            }
        }
        tmp = tmp->next;
    }
}

void printProcessList(process** process_list){
    if (!(*process_list)){
        return;
    }
    process* tmp = (*process_list)->next;
    process** prev = process_list;

    while (tmp!=NULL){
        printf("PID         Command         Status\n%i          %s          %i\n", tmp->pid, tmp->cmd->arguments[0], tmp->status);
        // after printing we want to delete the terminated process
        if (tmp->status == -1){
            (*prev)->next = tmp->next;
            tmp = tmp->next;
        }
        else{
            (*prev) = (*prev)->next;
            tmp = tmp->next;
        }
    }

    if ((*process_list)->status == -1) {
        printf("PID         Command         Status\n%i          %s          %i\n", (*prev)->pid, (*prev)->cmd->arguments[0], (*prev)->status);
        *process_list = (*process_list)->next;
    }
}

pid_t execute(cmdLine *pCmdLine){
    int status;
    pid_t pid = fork();
    if (pid == 0){          // it is a child process
        if (execvp(pCmdLine->arguments[0], pCmdLine->arguments) == -1 ){
            perror("ERROR blyat");
            _exit(1);
        }
    }
    if (pCmdLine->blocking == 1){
        waitpid(pid, &status, 0);
    }
    return pid;
}

int command(process* process_list ,cmdLine *pCmdLine){
    if (strcmp(pCmdLine->arguments[0], "quit")==0){
        exit(0);
    }
    else if(strcmp(pCmdLine->arguments[0], "cd")==0){
        if(chdir(pCmdLine->arguments[1]) == -1){
            perror("Path is unavailable"); 
        }
        return 1;
    }
    else if(strcmp(pCmdLine->arguments[0], "procs")==0){
        updateProcessList(&process_list);
        printProcessList(&process_list);
        return 1;
    }
    return 0;
}

int main(int argc, const char* argv[]){
    int rc;
    char userInput[MAX_INPUT_BYTES];
    char pathName[PATH_MAX];
    cmdLine* pCmdLine;
    pid_t pid;
    int debug = 0;
    process* process_list = NULL;

    if (argc > 1){
        if (strcmp(argv[1], "-d")==0){
            debug = 1;
        }
    }
    while (1){
        rc = 1;
        getcwd(pathName, PATH_MAX);                 // get the path for prompt
        printf("%s: > ", pathName);             
        fgets(userInput, MAX_INPUT_BYTES, stdin);   // get the user command
        pCmdLine = parseCmdLines(userInput);
        if (pCmdLine==NULL){
            continue;
        }        
        rc = command(process_list, pCmdLine);
        if (rc == 0){
            pid = execute(pCmdLine);
            addProcess(&process_list, pCmdLine, pid);
            if (debug == 1){
                printf("The given command is: %s\nThe PID is: %i\n", userInput, pid);
            }
        }
    }
    free(pCmdLine);
    return 0;
}




