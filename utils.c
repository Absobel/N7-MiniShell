#define _XOPEN_SOURCE 700   // Permet que le compilateur reconnaisse certaines fonctions
#include "utils.h"
#include <stdlib.h>  /* getenv */
#include <string.h> /* strcmp */
#include <unistd.h> /* access */
#include <stdbool.h>  /* booleans */
#include <stdio.h>  /* sprintf */

// Define some color and style codes
#define COLOR_LIGHT_BLUE "\x1b[1;94m"
#define COLOR_RED "\x1b[1;31m"
#define COLOR_DARK_GRAY "\x1b[1;90m"
#define RESET "\x1b[0m"

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

            // Tout d'abord, vérifiez s'il s'agit d'une commande de chemin relatif "./file"
            if(strncmp(cmd_name, "./", 2) == 0 && access(cmd_name, X_OK) == 0) {
                command_found = true;
            }
            else {
                // Si ce n'est pas une commande "./file", nous parcourons les chemins dans PATH
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
            }

            if (!command_found) {
                strcpy(invalid_cmd, cmd_name);
                return;
            }
        }
    }
    strcpy(invalid_cmd, "");
}


void print_prompt() {
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));  // get current directory
    printf(COLOR_DARK_GRAY "┌─[" RESET COLOR_LIGHT_BLUE "%s" COLOR_DARK_GRAY "]\n" RESET, cwd);
    printf(COLOR_DARK_GRAY "└─[" RESET COLOR_RED "$" RESET COLOR_DARK_GRAY "]› " RESET);
}

void free_memory(char *inexistant_command) {
    if (inexistant_command != NULL) {
        free(inexistant_command);
    }
}