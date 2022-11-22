#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>

#include "myassert.h"

#include "master_worker.h"

#include <unistd.h>

// fonctions éventuelles internes au fichier

// fonctions éventuelles proposées dans le .h
void initWorker(int value, int fdWriteToM, int fdRead){
	char * argv[5];
	char buff1[10]; 
	char buff2[10]; 
	char buff3[10]; 
	
	argv[0] = "worker";
	sprintf(buff1, "%d", value);
	argv[1] = buff1;
	sprintf(buff2, "%d", fdWriteToM);
	argv[2] = buff2;
	sprintf(buff3, "%d", fdRead);
	argv[3] = buff3;
	argv[4] = NULL;
	
	int ret = execv("worker", argv);
	myassert(ret != -1, "Erreur : exec");
}
