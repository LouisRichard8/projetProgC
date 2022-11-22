#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "myassert.h"

#include "master_worker.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

/************************************************************************
 * Données persistantes d'un worker
 ************************************************************************/

// on peut ici définir une structure stockant tout ce dont le worker
// a besoin : le nombre premier dont il a la charge, ...

typedef struct{
	int p;
	int fdToMaster;
	int fdRP; // lire ce qu'écrit le parent
	int fdWF; // écrire au fils
} WorkerData;


/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/

static void usage(const char *exeName, const char *message)
{
    fprintf(stderr, "usage : %s <n> <fdIn> <fdToMaster>\n", exeName);
    fprintf(stderr, "   <n> : nombre premier géré par le worker\n");
    fprintf(stderr, "   <fdIn> : canal d'entrée pour tester un nombre\n");
    fprintf(stderr, "   <fdToMaster> : canal de sortie pour indiquer si un nombre est premier ou non\n");
    if (message != NULL)
        fprintf(stderr, "message : %s\n", message);
    exit(EXIT_FAILURE);
}

static void parseArgs(int argc, char * argv[], WorkerData * datas)
{
    if (argc != 4)
        usage(argv[0], "Nombre d'arguments incorrect");

    // remplir la structure
    datas->p = atoi(argv[1]);
    datas->fdToMaster = atoi(argv[2]);
    datas->fdRP = atoi(argv[3]);
    datas->fdWF = 0;
}

/************************************************************************
 * Boucle principale de traitement
 ************************************************************************/

void loop(WorkerData * datas)
{
    // boucle infinie :
    //    attendre l'arrivée d'un nombre à tester
    //    si ordre d'arrêt
    //       si il y a un worker suivant, transmettre l'ordre et attendre sa fin
    //       sortir de la boucle
    //    sinon c'est un nombre à tester, 4 possibilités :
    //           - le nombre est premier
    //           - le nombre n'est pas premier
    //           - s'il y a un worker suivant lui transmettre le nombre
    //           - s'il n'y a pas de worker suivant, le créer
    
    int retFork = -1;
    
    while(true){
    	int val = 0;
    	int ret = read(datas->fdRP, &val, sizeof(int));
    	myassert(ret == sizeof(int), "Erreur : read");
    	
    	if(val == -1){
    		if(retFork != -1){
    			ret = write(datas->fdWF, &val, sizeof(int));
    			myassert(ret == sizeof(int), "Erreur : write");
    			wait(NULL);
    		}
    		break;
    	}
    	else{
    		if(val == datas->p){
    			val = 1;
    			ret = write(datas->fdToMaster, &val, sizeof(int));
    			myassert(ret == sizeof(int), "Erreur : write");
    		}
    		else if(val%datas->p == 0){
    			val = 0;
    			ret = write(datas->fdToMaster, &val, sizeof(int));
    			myassert(ret == sizeof(int), "Erreur : write");
    		}
    		else if(val > datas->p){
    			if(retFork == -1){
    				int fds[2];
    				ret = pipe(fds);
    				myassert(ret != -1, "Erreur : pipe");
    				
    				datas->fdWF = fds[1];
    				
    				retFork = fork();
    				if(retFork == 0){
    					initWorker(val, datas->fdToMaster, fds[0]);
						exit(EXIT_SUCCESS);
    				}
    			}
    			ret = write(datas->fdWF, &val, sizeof(int));
    			myassert(ret == sizeof(int), "Erreur : write");
    		}
    	}
    }
}

/************************************************************************
 * Programme principal
 ************************************************************************/

int main(int argc, char * argv[])
{
	WorkerData datas;
    parseArgs(argc, argv, &datas);
    
    // Si on est créé c'est qu'on est un nombre premier
    // Envoyer au master un message positif pour dire
    // que le nombre testé est bien premier

    loop(&datas);

    // libérer les ressources : fermeture des files descriptors par exemple

    return EXIT_SUCCESS;
}
