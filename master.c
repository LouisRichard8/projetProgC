#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "myassert.h"

#include "master_client.h"
#include "master_worker.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/wait.h>

/************************************************************************
 * Données persistantes d'un master
 ************************************************************************/

// on peut ici définir une structure stockant tout ce dont le master
// a besoin

typedef struct{
	int semId;
	int highest;
	int nbPrim;
	int fdWtoM;
	int fdMtoW;
	int pidF;
} MasterData;


/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/

static void usage(const char *exeName, const char *message)
{
    fprintf(stderr, "usage : %s\n", exeName);
    if (message != NULL)
        fprintf(stderr, "message : %s\n", message);
    exit(EXIT_FAILURE);
}

/************************************************************************
 * Initialisation
 ************************************************************************/
void init(MasterData *datas){
	int semId = semget(CLE, 2, IPC_CREAT | IPC_EXCL | 0641);
	myassert(semId != -1, "Erreur : semget");

	int ret = semctl(semId, 0, SETVAL, 1);
	myassert(ret != -1, "Erreur : semctl");

	ret = semctl(semId, 1, SETVAL, 1);
	myassert(ret != -1, "Erreur : semctl");

	ret = mkfifo(SEM_CLI_MAST, 0644);
	myassert(ret != -1, "Erreur : mkfifo");

	ret = mkfifo(SEM_MAST_CLI, 0644);
	myassert(ret != -1, "Erreur : mkfifo");

	datas->semId = semId;
	datas->highest = 2;
	datas->nbPrim = 0;
	
	int fdMtoW[2], fdWtoM[2];
	ret = pipe(fdMtoW);
	myassert(ret != -1, "Erreur : pipe");
	
	ret = pipe(fdWtoM);
	myassert(ret != -1, "Erreur : pipe");
	
	datas->fdWtoM = fdWtoM[0];
	datas->fdMtoW = fdMtoW[1];
	
	int retFork = fork();
	if(retFork == 0){
		initWorker(2, fdWtoM[1], fdMtoW[0]);
		exit(EXIT_SUCCESS);
	}
	datas->pidF = retFork;
	close(fdWtoM[1]);
	close(fdMtoW[0]);	
}

/************************************************************************
 * Destruction
 ************************************************************************/
void destroy(MasterData * datas){

	int ret = unlink(SEM_MAST_CLI);
	myassert((ret == 0), "Erreur : unlink masterClient");
	
    ret = unlink(SEM_CLI_MAST);
    myassert((ret == 0), "Erreur : unlink clientMaster");
    
    ret = semctl(datas->semId, -1, IPC_RMID);
    myassert((ret == 0), "Erreur : suppression semmaphore");
}

/************************************************************************
 * boucle principale de communication avec le client
 ************************************************************************/
void loop(MasterData *datas)
{
    // boucle infinie :
    // - ouverture des tubes (cf. rq client.c)
    // - attente d'un ordre du client (via le tube nommé)
    // - si ORDER_STOP
    //       . envoyer ordre de fin au premier worker et attendre sa fin
    //       . envoyer un accusé de réception au client
    // - si ORDER_COMPUTE_PRIME
    //       . récupérer le nombre N à tester provenant du client
    //       . construire le pipeline jusqu'au nombre N-1 (si non encore fait) :
    //             il faut connaître le plus nombre (M) déjà enovoyé aux workers
    //             on leur envoie tous les nombres entre M+1 et N-1
    //             note : chaque envoie déclenche une réponse des workers
    //       . envoyer N dans le pipeline
    //       . récupérer la réponse
    //       . la transmettre au client
    // - si ORDER_HOW_MANY_PRIME
    //       . transmettre la réponse au client
    // - si ORDER_HIGHEST_PRIME
    //       . transmettre la réponse au client
    // - fermer les tubes nommés
    // - attendre ordre du client avant de continuer (sémaphore : précédence)
    // - revenir en début de boucle
    //
    // il est important d'ouvrir et fermer les tubes nommés à chaque itération
    // voyez-vous pourquoi ?
    
    
    int ret;
    
    while(true){
    	int order[2] = {0,0}, retour = 1;
    	bool stop = false;
    	
    	ret = semop(datas->semId, &operationMoinsMaster, 1);
		myassert((ret != -1), "Erreur : semop");
    	
		int clientMaster = open(SEM_CLI_MAST, O_RDONLY);
		myassert(clientMaster != -1, "Erreur : open clientMaster");
		int masterClient = open(SEM_MAST_CLI, O_WRONLY);
		myassert(masterClient != -1, "Erreur : open masterClient");
    	
    	ret = read(clientMaster, &order, 2*sizeof(int));
    	myassert(ret == sizeof(int)*2, "Erreur : read clientMaster"); 

    	switch(order[0]){
    		case ORDER_STOP : 
					 ret = write(datas->fdMtoW, &order[0], sizeof(int));
					 myassert(ret == sizeof(int), "Erreur : write fils");
					 ret = waitpid(datas->pidF, NULL, 0);
					 myassert(ret != -1, "Erreur : wait");
					 stop = true;
    			break;
    		case ORDER_COMPUTE_PRIME : 
    				 if(order[1] > datas->highest+1){
    				 	for(int i = datas->highest; i < order[1]; i++){
    				 		ret = write(datas->fdMtoW, &i, sizeof(int));
    				 		myassert(ret == sizeof(int), "Erreur : write fils");
    				 		ret = read(datas->fdWtoM, &retour, sizeof(int));
    				 		myassert(ret == sizeof(int), "Erreur : read fils"); 
    				 	}
    				 }
    				 ret = write(datas->fdMtoW, &order[1], sizeof(int));
    				 myassert(ret == sizeof(int), "Erreur : write fils");
    				 ret = read(datas->fdWtoM, &retour, sizeof(int));
    				 myassert(ret == sizeof(int), "Erreur : read fils"); 
    				 if(retour == 1)
    				 	datas->highest = datas->highest<order[1] ? order[1] : datas->highest;
    				 datas->nbPrim++;
    			break;
    		case ORDER_HOW_MANY_PRIME : 
    				 retour = datas->nbPrim;
    			break;
    		case ORDER_HIGHEST_PRIME : 
    				 retour = datas->highest;
    			break;
    	}
    	if(!stop){
    		ret = write(masterClient, &retour, sizeof(int));
    		myassert(ret == sizeof(int), "Erreur : write masterClient"); 
    	}
    	
    	ret = close(clientMaster);
    	myassert(ret != -1, "Erreur : close clientMaster");
    	ret = close(masterClient);
    	myassert(ret != -1, "Erreur : close masterClient");
    	
    	if(stop){
			ret = semop(datas->semId, &operationMoinsMaster, 1);
			myassert((ret != -1), "Erreur : semop");
    		break;
    	}
    }
}


/************************************************************************
 * Fonction principale
 ************************************************************************/

int main(int argc, char * argv[])
{
    if (argc != 1)
        usage(argv[0], NULL);

    // - création des sémaphores
    // - création des tubes nommés
    // - création du premier worker
    MasterData datas;
    init(&datas);

    // boucle infinie
    loop(&datas);

    // destruction des tubes nommés, des sémaphores, ...
    destroy(&datas);

    return EXIT_SUCCESS;
}

// N'hésitez pas à faire des fonctions annexes ; si les fonctions main
// et loop pouvaient être "courtes", ce serait bien
