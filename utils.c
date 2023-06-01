#define _XOPEN_SOURCE 700   // Permet que le compilateur reconnaisse certaines fonctions
#include "utils.h"
#include <stdlib.h>  /* getenv */
#include <string.h> /* strcmp */
#include <unistd.h> /* access */
#include <stdbool.h>  /* booleans */
#include <stdio.h>  /* sprintf */

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

void checkCommand(char*** seq, char* invalid_cmd) {
    char *path = getenv("PATH");
    char *path_copy = strdup(path); // strtok modifie la chaine
    char *dir = strtok(path_copy, ":"); // On récupère le premier chemin

    for(int i = 0; seq[i] != NULL; i++) {
        char* cmd_name = seq[i][0]; 
        if(strcmp(cmd_name, "cd") != 0 && strcmp(cmd_name, "fg") != 0 && strcmp(cmd_name, "bg") != 0 
            && strcmp(cmd_name, "sj") != 0 && strcmp(cmd_name, "lj") != 0 && strcmp(cmd_name, "exit") != 0) {

            bool command_found = false;

            // On parcourt les chemins de PATH
            while (dir) {
                char *full_path = malloc(strlen(dir) + strlen(cmd_name) + 2); // +2 pour '/' et '\0'
                sprintf(full_path, "%s/%s", dir, cmd_name);

                if(access(full_path, X_OK) == 0) {
                    command_found = true;
                    free(full_path);
                    break;
                }

                free(full_path);
                dir = strtok(NULL, ":"); // On récupère le chemin suivant
            }

            if (!command_found) {
                strcpy(invalid_cmd, cmd_name);
                return;
            }
        }
    }
    strcpy(invalid_cmd, "");
}
