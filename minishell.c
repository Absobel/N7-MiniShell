#define _XOPEN_SOURCE 700   // Permet d'avoir la fonction WIFCONTINUED
#include <stdio.h>    /* entrées sorties */
#include <unistd.h>   /* pimitives de base : fork, ...*/
#include <stdlib.h>   /* exit */
#include <sys/wait.h> /* wait */
#include <string.h>   /* opérations sur les chaines */
#include <fcntl.h>    /* opérations sur les fichiers */
#include <stdbool.h>  /* booleans */
#include <signal.h>   /* signaux */

#include "readcmd.h"



// ////////////   JOBS

#define NB_JOBS_MAX 20

typedef enum state {
    ACTIF,
    SUSPENDU,
    TERMINE,
} state;

typedef struct job {
    int pid;
    state state;
    char* cmd;
} job;

void init_jobs(job jobs[]) {
    for (int i = 0; i < NB_JOBS_MAX; i++) {
        jobs[i] = (job) {-1, TERMINE, NULL};
    }
}

// returns -1 if error, id of the job otherwise
int add_job(job jobs[], job job) {
    for (int i = 0; i < NB_JOBS_MAX; i++) {
        if (jobs[i].state == TERMINE) {
            free(jobs[i].cmd);
            jobs[i] = job;
            return i;
        }
    }
    fprintf(stderr, "minishell: too many process\n");
    return -1;
}

void maj_jobs(job jobs[]) {
    int status;
    int pid = waitpid(-1, &status, WNOHANG);
    while(pid > 0) {
        for (int i = 0; i < NB_JOBS_MAX; i++) {
            if (jobs[i].state != TERMINE && WIFEXITED(status)) {
                jobs[i].state = TERMINE;
            }
        }
        pid = waitpid(-1, &status, WNOHANG);
    }
}

void destroy_job(job jobs[], int id) {
    if (id <= NB_JOBS_MAX && jobs[id].state != TERMINE) {
        jobs[id].state = TERMINE;
        free(jobs[id].cmd);
        jobs[id].cmd = NULL;
        return;
    }
    fprintf(stderr, "minishell: bg: job not found: %d\n", id);
}

void destroy_job_pid(job jobs[], pid_t pid) {
    for (int i = 0; i < NB_JOBS_MAX; i++) {
        if (jobs[i].pid == pid) {
            jobs[i].state = TERMINE;
            free(jobs[i].cmd);
            jobs[i].cmd = NULL;
            return;
        }
    }
}

// lj
void list_jobs(job jobs[]) {
    printf("%-4s %-6s %s\n", "id", "state", "command");
    for (int i = 0; i < NB_JOBS_MAX; i++) {
        if (jobs[i].state != TERMINE) {
            printf("%-4d %-6s %s\n", i, jobs[i].state == ACTIF ? "actif" : "suspendu", jobs[i].cmd);
        }
    }
}

// sj
void stop_job(job jobs[], int id) {
    if (id <= NB_JOBS_MAX && jobs[id].state != TERMINE) {
        kill(jobs[id].pid, SIGSTOP);
        jobs[id].state = SUSPENDU;
        return;
    }
    fprintf(stderr, "minishell: sj: job not found: %d\n", id);
}

// bg
void continue_job(job jobs[], int id) {
    if (id <= NB_JOBS_MAX && jobs[id].state != TERMINE) {
        kill(jobs[id].pid, SIGCONT);
        jobs[id].state = ACTIF;
        return;
    }
    fprintf(stderr, "minishell: bg: job not found: %d\n", id);
}

// fg
void foreground_job(job jobs[], int id) {
    if (id <= NB_JOBS_MAX && jobs[id].state != TERMINE) {
        kill(jobs[id].pid, SIGCONT);
        jobs[id].state = ACTIF;
        int status;
        waitpid(jobs[id].pid, &status, 0);
        jobs[id].state = TERMINE;
        return;
    }
    fprintf(stderr, "minishell: fg: job not found: %d\n", id);
}








//////////////   MAIN

char* cmd_to_str(struct cmdline* cmd) {
    int needed_size = 0;
    for (int i = 0; cmd->seq[0][i]; i++) {
        needed_size += strlen(cmd->seq[0][i]) + 1;
    }
    char* str = malloc(needed_size);
    str[0] = '\0';
    for (int i = 0; cmd->seq[0][i]; i++) {
        strcat(str, cmd->seq[0][i]);
        strcat(str, " ");
    }
    return str;
}

int main() {
    struct cmdline *cmd;
    job jobs[NB_JOBS_MAX];
    init_jobs(jobs);


    do {
        printf(">>> ");
        cmd = readcmd();

        if (cmd == NULL) {  // EOF
            printf("\n");
            break;
        }
        if (cmd->err != NULL) {
            fprintf(stderr, "Syntax error: %s\n", cmd->err);
            continue;
        }
        if (cmd->seq[0] == NULL) {   // ligne vide
            continue;
        }

        // built-in commands
        else if (strcmp(cmd->seq[0][0], "exit") == 0 && cmd->seq[1] == NULL) {
            if (cmd->seq[0][1] == NULL) {
                exit(EXIT_SUCCESS);
            } else {
                fprintf(stderr, "exit: too many arguments\n");
                continue;
            }
        }
        else if (strcmp(cmd->seq[0][0], "cd") == 0 && cmd->seq[1] == NULL) {
            if (cmd->seq[0][1] == NULL) {
                chdir(getenv("HOME"));
            } else if (cmd->seq[0][2] != NULL) {
                chdir(cmd->seq[0][1]);
            } else {
                fprintf(stderr, "cd: too many arguments\n");
            }
            continue;
        }
        else if (strcmp(cmd->seq[0][0], "lj") == 0 && cmd->seq[1] == NULL) {
            if (cmd->seq[0][1] == NULL) {
                list_jobs(jobs);
            } else {
                fprintf(stderr, "lj: too many arguments\n");
            }
        }
        else if (strcmp(cmd->seq[0][0], "sj") == 0 && cmd->seq[1] == NULL) {
            if (cmd->seq[0][1] == NULL) {
                fprintf(stderr, "sj: missing argument\n");
            } else if (cmd->seq[0][2] == NULL) {
                stop_job(jobs, atoi(cmd->seq[0][1]));
            } else {
                fprintf(stderr, "sj: too many arguments\n");
            }
        }
        else if (strcmp(cmd->seq[0][0], "bg") == 0 && cmd->seq[1] == NULL) {
            if (cmd->seq[0][1] == NULL) {
                fprintf(stderr, "bg: missing argument\n");
            } else if (cmd->seq[0][2] == NULL) {
                continue_job(jobs, atoi(cmd->seq[0][1]));
            } else {
                fprintf(stderr, "bg: too many arguments\n");
            }
        }
        else if (strcmp(cmd->seq[0][0], "fg") == 0 && cmd->seq[1] == NULL) {
            if (cmd->seq[0][1] == NULL) {
                fprintf(stderr, "fg: missing argument\n");
            } else if (cmd->seq[0][2] == NULL) {
                foreground_job(jobs, atoi(cmd->seq[0][1]));
            } else {
                fprintf(stderr, "fg: too many arguments\n");
            }
        } 
        
        else {
            pid_t pid = fork();
            if (pid == -1) {
                perror("fork");
                exit(EXIT_FAILURE);
            }
            if (pid == 0) {
                int exit_code = execvp(cmd->seq[0][0], cmd->seq[0]);
                exit(exit_code);
            } else {
                int id_or_error = add_job(jobs, (job) {pid, ACTIF, cmd_to_str(cmd)});
                if (id_or_error == -1) {
                    continue;
                }
                int status;
                if (cmd->backgrounded == NULL) {
                    waitpid(pid, &status, 0);
                    jobs[id_or_error].state = TERMINE;
                }
                if (WIFEXITED(status) && WEXITSTATUS(status) == 255) {
                    fprintf(stderr, "minishell: %s: command not found\n", cmd->seq[0][0]);
                    destroy_job(jobs, id_or_error);
                }
            }
        }
        maj_jobs(jobs);
    } while (1);

    return EXIT_SUCCESS;
}