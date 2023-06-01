#ifndef UTILS_H
#define UTILS_H

#include "readcmd.h"

char* cmd_to_str(struct cmdline* cmd);
void checkCommand(char*** seq, char* invalid_cmd);

#endif