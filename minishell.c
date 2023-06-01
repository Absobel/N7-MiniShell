#define _XOPEN_SOURCE 700   // Permet que le compilateur reconnaisse certaines fonctions
#include <stdio.h>    /* entrées sorties */
#include <unistd.h>   /* pimitives de base : fork, ...*/
#include <stdlib.h>   /* exit */
#include <sys/wait.h> /* wait */
#include <string.h>   /* opérations sur les chaines */
#include <fcntl.h>    /* opérations sur les fichiers */
#include <stdbool.h>  /* booleans */
#include <signal.h>   /* signaux */
#include <errno.h>   /* gestion des erreurs */

#include "readcmd.h"
#include "utils.h"

#define PROMPT "minishell> "
#define CMD_SIZE 4096

bool suspend_flag = false;



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
            if (jobs[i].pid == pid && jobs[i].state != TERMINE && WIFEXITED(status)) {
                jobs[i].state = TERMINE;
                break;
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
        jobs[id].state = suspend_flag ? SUSPENDU : TERMINE;
        suspend_flag = false;
        return;
    }
    fprintf(stderr, "minishell: fg: job not found: %d\n", id);
}

void suspend_fg_job(job jobs[]) {
    for (int i = 0; i < NB_JOBS_MAX; i++) {
        if (jobs[i].state == ACTIF) {
            kill(jobs[i].pid, SIGSTOP);
            jobs[i].state = SUSPENDU;
            return;
        }
    }
}
void destroy_fg_job(job jobs[]) {
    for (int i = 0; i < NB_JOBS_MAX; i++) {
        if (jobs[i].state == ACTIF) {
            jobs[i].state = TERMINE;
            kill(jobs[i].pid, SIGINT);
            return;
        }
    }
}







//////////////   MAIN

job jobs[NB_JOBS_MAX];
int fd_in; int saved_stdin;
int fd_out; int saved_stdout;

void handler_sig(int sig) {
    switch (sig) {
        case SIGTSTP:
            suspend_fg_job(jobs);
            suspend_flag = true;
            break;
        case SIGINT:
            if (!suspend_flag) {
                printf("\n");
                destroy_fg_job(jobs);
            }
            break;
    }
}

int main() {
    // Signal handler
    struct sigaction sa;
    sa.sa_handler = handler_sig;
    sa.sa_flags = 0;  // Renvoie moins de signaux SIGCHLD
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTSTP, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);


    struct cmdline *cmd;
    init_jobs(jobs);

    sigset_t mask, oldmask;
    sigemptyset(&mask);

    // Bloquer SIGTSTP et SIGINT
    sigaddset(&mask, SIGTSTP);
    sigaddset(&mask, SIGINT);

    do {
        printf(PROMPT);
        
        // Bloquer SIGTSTP et SIGINT avant readcmd
        if (sigprocmask(SIG_BLOCK, &mask, &oldmask) < 0) {
            perror("sigprocmask block");
            exit(EXIT_FAILURE);
        }
        cmd = readcmd();
        // Débloquer SIGTSTP et SIGINT après readcmd
        if (sigprocmask(SIG_SETMASK, &oldmask, NULL) < 0) {
            perror("sigprocmask unblock");
            exit(EXIT_FAILURE);
        }

        if (cmd == NULL) {  // EOF (ne devrait pas être un signal car bloqué)
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

        char* inexistant_command = malloc(CMD_SIZE);
        checkCommand(cmd->seq, inexistant_command);
        if (strcmp(inexistant_command, "") != 0) {
            fprintf(stderr, "minishell: %s: command not found\n", inexistant_command);
            continue;
        }

        // Redirections
        if (cmd->in != NULL) {
            fd_in = open(cmd->in, O_RDONLY);
            if (fd_in == -1) {
                if (errno == ENOENT) {
                    fprintf(stderr, "minishell: %s: No such file or directory\n", cmd->in);
                } else {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
            }
            saved_stdin = dup(STDIN_FILENO);
            if (dup2(fd_in, STDIN_FILENO) == -1) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
        }
        if (cmd->out != NULL) {
            fd_out = open(cmd->out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd_out == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            saved_stdout = dup(STDOUT_FILENO);
            if (dup2(fd_out, STDOUT_FILENO) == -1) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
        }

        // built-in commands
        if (strcmp(cmd->seq[0][0], "exit") == 0 && cmd->seq[1] == NULL) {
            if (cmd->seq[0][1] == NULL) {
                exit(EXIT_SUCCESS);
            } else {
                fprintf(stderr, "exit: too many arguments\n");
            }
        }
        else if (strcmp(cmd->seq[0][0], "cd") == 0 && cmd->seq[1] == NULL) {
            if (cmd->seq[0][1] == NULL) {
                chdir(getenv("HOME"));
            } else if (cmd->seq[0][1] != NULL && cmd->seq[0][2] == NULL) {
                int err = chdir(cmd->seq[0][1]);
                if (err == -1) {
                    fprintf(stderr, "cd: The directory '%s' does not exist\n", cmd->seq[0][1]);
                }
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
            for (int i = 0; cmd->seq[i] != NULL; i++) {
                pid_t pid = fork();
                if (pid == -1) {
                    perror("fork");
                    exit(EXIT_FAILURE);
                }
                if (pid == 0) {
                    int exit_code = execvp(cmd->seq[i][0], cmd->seq[i]);
                    exit(exit_code);
                } else {
                    int id_or_error = add_job(jobs, (job) {pid, ACTIF, cmd_to_str(cmd)});
                    if (id_or_error == -1) {
                        continue;
                    }
                    int status;
                    if (cmd->backgrounded == NULL) {
                        waitpid(pid, &status, 0);
                        jobs[id_or_error].state = suspend_flag ? SUSPENDU : TERMINE;
                        suspend_flag = false;
                    }
                    if (WIFEXITED(status) && WEXITSTATUS(status) == 255) {
                        fprintf(stderr, "minishell: %s: command not found\n", cmd->seq[i][0]);
                    }
                }
            }
        }

        if (cmd->in != NULL) {
            close(fd_in);
            if (dup2(saved_stdin, STDIN_FILENO) == -1) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
        }
        if (cmd->out != NULL) {
            close(fd_out);
            if (dup2(saved_stdout, STDOUT_FILENO) == -1) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
        }

        maj_jobs(jobs);
    } while (1);

    return EXIT_SUCCESS;
}