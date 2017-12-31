#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <readline/readline.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pwd.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>

#include "sfish.h"
#include "debug.h"

volatile sig_atomic_t stop;
char** input;
char*** pipedInput;
bool exited = false;
char* lastDir = "~\0";
char* currentDir = "~\0";
char* endingDir = " :: eunhokim >>\0";
int pipeInside = 0;
int redir = 0;
int pidsSize = 0;
char** pidArr;

void sf_help() {
    char *str = "help       prints a list of all builtins\nexit       exit the shell\ncd         goes to the user's home directory\ncd .       current directory\ncd ..      previous directory of the current directory\ncd [/dir]  goes to the directory if it exists\npwd        prints the absolute path of the currrent working directory";
    printf("%s\n", str);
}
void sf_exit() {
    exited = true;
}
void sf_cd1() {
    currentDir = lastDir;
}
void sf_cd2() {
    char* temp = lastDir;
    lastDir = currentDir;
    currentDir = temp;
}
void sf_cd3() {
    if (strcmp(currentDir, "~\0") != 0) {
        lastDir = calloc(1, strlen(currentDir) + 1);
        strcpy(lastDir, currentDir);
        int charInd = strlen(currentDir) - 1;
        while (currentDir[charInd] != '/') {
            charInd--;
        }
        currentDir[charInd] = '\0';
    }
    else {
        printf(BUILTIN_ERROR, "Cannot go to parent directory");
    }
}
void sf_cd4() {
    lastDir = currentDir;
    currentDir = calloc(1, 2*sizeof(char));
    currentDir[0] = '~';
}
void sf_cd5(char *dir, const char *homedir) {
    char *newDir = calloc(1, strlen(dir) + strlen(currentDir) + 1);
    strcat(newDir, currentDir);
    strcat(newDir, dir);
    char *curDir = calloc(1, strlen(newDir) + strlen(homedir));
    strcat(curDir, homedir);
    strcat(curDir, newDir + 1);
    DIR* direc = opendir(curDir);
    if (direc){
        currentDir = newDir;
        closedir(direc);
    }
    else if (ENOENT == errno){
        printf(BUILTIN_ERROR, "Directory not found");
    }
}
void sf_pwd(const char *homedir) {
    char *curDir = calloc(1, strlen(currentDir) + strlen(homedir));
    strcat(curDir, homedir);
    strcat(curDir, currentDir + 1);
    printf("%s\n", curDir);
}

char **parse_line(char *str) {
    char **parsed_line = malloc(64 * sizeof(char*));
    char *token = strtok(str, " ");
    int strInd = 0;
    while (token != NULL) {
        if (strcmp(token, "|") == 0) {
            pipeInside++;
        }
        else if (strcmp(token, ">") == 0) {
            if (redir == 0) {
                redir = 1;
            }
            else {
                redir = 3;
            }
        }
        if (strcmp(token, "<") == 0) {

        }
        else {
            parsed_line[strInd] = calloc(1, strlen(token) + 1);
            strncpy(parsed_line[strInd++], token, strlen(token));
        }
        token = strtok(NULL, " ");
    }
    parsed_line[strInd] = NULL;
    return parsed_line;
}

void executable(char **args) {
    pid_t pid = fork();
    if (pid == 0) {
        if (execvp(args[0], args) < 0){
            printf(EXEC_NOT_FOUND, args[0]);
        }
        EXIT_SUCCESS;
    }
    else {
        waitpid(-1, NULL, 0);
    }
}

void handler(int n){
    int j = 0;
    while (j < 64) {
        if (pidArr[j] == NULL) {
            pidArr[j] = input[0];
            break;
        }
        j++;
    }
    stop = 1;
}

void pipeExcutable(char **args) {
    int argInd = 0;
    for (int i = 0; i < pipeInside; i++) {
        char **com1 = malloc(64 * sizeof(char*));
        char **com2 = malloc(64 * sizeof(char*));
        int comInd = 0;
        while (args[argInd] != NULL && strcmp(args[argInd], "|") != 0) {
            strcpy(com1[comInd], args[argInd]);
            comInd++;
            argInd++;
        }
        argInd++;
        comInd = 0;
        while (args[argInd] != NULL && strcmp(args[argInd], "|") != 0) {
            strcpy(com2[comInd], args[argInd]);
            comInd++;
            argInd++;
        }

        int fd[2];
        pipe(fd);
        pid_t pid;
        if ((pid = fork()) == 0) {
            dup2(fd[1], 1);
            close(fd[0]);
            close(fd[1]);
            execvp(com1[0], com1);
        }
        else {
            dup2(fd[0], 0);
            close(fd[0]);
            close(fd[1]);
            execvp(com2[0], com2);
        }
    }
}
void sf_jobs() {
    for (int i = 0; i < pidsSize; i++) {
        printf(JOBS_LIST_ITEM, i, pidArr[i]);
    }
}

void redirect(char **args, int n) {
    char **temp = malloc(100);
    pid_t pid = fork();
    if (pid == 0) {
        if (n == 1) {
            int k = 0;
            for (k = 0; strcmp(args[k], ">") != 0; k++) {
            }
            args[k] = NULL;
            FILE *fp = fopen(args[k + 1], "w");
            fwrite (executable(args), 1, sizeof(executable(args)), fp);
            fclose(fp);
        }
        else if (n == 2) {
            int k;
            for (k = 0; strcmp(args[k], "<") != 0; k++) {
                strcpy(temp[k], args[k]);
            }
            k++;
            int file = 0;
            file = open(args[k], O_RDONLY);
            dup2(file, STDIN_FILENO);
            close(file);
        }
        else if (n == 3) {
            int k;
            for (k = 0; strcmp(args[k], "<") != 0; k++) {
                strcpy(temp[k], args[k]);
            }
            k++;
            int file = 0;
            file = open(args[k], O_RDONLY);
            dup2(file, STDIN_FILENO);
            close(file);
        }
        else if (n == 4) {
            int k;
            for (k = 0; strcmp(args[k], "<") != 0; k++) {
                strcpy(temp[k], args[k]);
            }
            k++;
            int file = 0;
            file = open(args[k], O_RDONLY);
            dup2(file, STDIN_FILENO);
            close(file);
        }
        EXIT_SUCCESS;
    }
    else {
        wait(NULL);
    }
}

int main(int argc, char *argv[], char* envp[]) {
    pidArr = calloc(1,sizeof (char*) * 64);
    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;
    chdir(homedir);
    struct sigaction sa;
    sa.sa_handler = handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTSTP, &sa, NULL);

    if(!isatty(STDIN_FILENO)) {
        // If your shell is reading from a piped file
        // Don't have readline write anything to that file.
        // Such as the prompt or "user input"
        if((rl_outstream = fopen("/dev/null", "w")) == NULL){
            perror("Failed trying to open DEVNULL");
            exit(EXIT_FAILURE);
        }
    }
    do {
        char * new_str;
        new_str = malloc(strlen(currentDir) + strlen(endingDir) + 1);
        new_str[0] = '\0';
        strcat(new_str, currentDir);
        strcat(new_str, endingDir);
        input = parse_line(readline(new_str));

        write(1, "\e[s", strlen("\e[s"));
        write(1, "\e[20;10H", strlen("\e[20;10H"));
        write(1, "SomeText", strlen("SomeText"));
        write(1, "\e[u", strlen("\e[u"));

        // If EOF is read (aka ^D) readline returns NULL
        if(input[0] == NULL) {
            continue;
        }
        else if (strcmp(input[0], "help") == 0) {
            sf_help();
        }
        else if (strcmp(input[0], "exit") == 0) {
            sf_exit();
        }
        else if (strcmp(input[0], "cd") == 0) {
            if (input[1] == NULL)  {
                sf_cd4();
            }
            else if (strcmp(input[1], "-") == 0) {
                sf_cd1();
            }
            else if (strcmp(input[1], ".") == 0) {
                sf_cd2();
            }
            else if (strcmp(input[1], "..") == 0) {
                sf_cd3();
            }
            else {
                sf_cd5(input[1], homedir);
            }
            char *curDir = calloc(1, strlen(currentDir) + strlen(homedir));
            strcat(curDir, homedir);
            strcat(curDir, currentDir + 1);
            chdir(curDir);
        }
        else if (strcmp(input[0], "pwd") == 0) {
            sf_pwd(homedir);
        }
        else if (strcmp(input[0], "jobs") == 0) {
            sf_jobs();
        }
        else if(strcmp(input[0],"kill")==0){
            if (input[1] == NULL) {
                printf(BUILTIN_ERROR, "No argument after kill");
            }
            else if(input[1][0]=='%'){
                int x = atoi(input[1] + 1);
                kill(x,SIGKILL);
            }else{
                int x = atoi(input[1]);
                kill(x,SIGKILL);
            }
        }
        else {
            if (pipeInside == 0 && redir == 0)  {
                executable(input);
            }
            else if (pipeInside == 0) {
                redirect(input, redir);
            }
            else {
                pipeExcutable(input);
                pipeInside = 0;
            }
        }

        // Readline mallocs the space for input. You must free it.
        rl_free(input);
        free(new_str);

    } while(!exited);

    debug("%s", "user entered 'exit'");

    return EXIT_SUCCESS;
}
