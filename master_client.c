#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#define _XOPEN_SOURCE

#include <stdlib.h>
#include <stdio.h>

#include "myassert.h"

#include "master_client.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/sem.h>

// opérations sur les sémaphores
struct sembuf operationMoinsCritique = {0, -1, 0};
struct sembuf operationPlusCritique = {0, 1, 0};
struct sembuf operationPlusMaster = {1, 1, 0};
struct sembuf operationMoinsMaster = {1, -1, 0};

// fonctions éventuelles internes au fichier

// fonctions éventuelles proposées dans le .h

