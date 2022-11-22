#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "myassert.h"

#include "master_client.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <pthread.h>
#include <math.h>

// chaines possibles pour le premier paramètre de la ligne de commande
#define TK_STOP      "stop"
#define TK_COMPUTE   "compute"
#define TK_HOW_MANY  "howmany"
#define TK_HIGHEST   "highest"
#define TK_LOCAL     "local"

/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/

static void usage(const char *exeName, const char *message)
{
    fprintf(stderr, "usage : %s <ordre> [<nombre>]\n", exeName);
    fprintf(stderr, "   ordre \"" TK_STOP  "\" : arrêt master\n");
    fprintf(stderr, "   ordre \"" TK_COMPUTE  "\" : calcul de nombre premier\n");
    fprintf(stderr, "                       <nombre> doit être fourni\n");
    fprintf(stderr, "   ordre \"" TK_HOW_MANY "\" : combien de nombres premiers calculés\n");
    fprintf(stderr, "   ordre \"" TK_HIGHEST "\" : quel est le plus grand nombre premier calculé\n");
    fprintf(stderr, "   ordre \"" TK_LOCAL  "\" : calcul de nombres premiers en local\n");
    if (message != NULL)
        fprintf(stderr, "message : %s\n", message);
    exit(EXIT_FAILURE);
}

static int parseArgs(int argc, char * argv[], int *number)
{
    int order = ORDER_NONE;

    if ((argc != 2) && (argc != 3))
        usage(argv[0], "Nombre d'arguments incorrect");

    if (strcmp(argv[1], TK_STOP) == 0)
        order = ORDER_STOP;
    else if (strcmp(argv[1], TK_COMPUTE) == 0)
        order = ORDER_COMPUTE_PRIME;
    else if (strcmp(argv[1], TK_HOW_MANY) == 0)
        order = ORDER_HOW_MANY_PRIME;
    else if (strcmp(argv[1], TK_HIGHEST) == 0)
        order = ORDER_HIGHEST_PRIME;
    else if (strcmp(argv[1], TK_LOCAL) == 0)
        order = ORDER_COMPUTE_PRIME_LOCAL;
    
    if (order == ORDER_NONE)
        usage(argv[0], "ordre incorrect");
    if ((order == ORDER_STOP) && (argc != 2))
        usage(argv[0], TK_STOP" : il ne faut pas de second argument");
    if ((order == ORDER_COMPUTE_PRIME) && (argc != 3))
        usage(argv[0], TK_COMPUTE " : il faut le second argument");
    if ((order == ORDER_HOW_MANY_PRIME) && (argc != 2))
        usage(argv[0], TK_HOW_MANY" : il ne faut pas de second argument");
    if ((order == ORDER_HIGHEST_PRIME) && (argc != 2))
        usage(argv[0], TK_HIGHEST " : il ne faut pas de second argument");
    if ((order == ORDER_COMPUTE_PRIME_LOCAL) && (argc != 3))
        usage(argv[0], TK_LOCAL " : il faut le second argument");
    if ((order == ORDER_COMPUTE_PRIME) || (order == ORDER_COMPUTE_PRIME_LOCAL))
    {
        *number = strtol(argv[2], NULL, 10);
        if (*number < 2)
             usage(argv[0], "le nombre doit être >= 2");
    }       
    
    return order;
}

/************************************************************************
 * Structures
 ************************************************************************/
typedef struct
{
	bool *tab;
	int n;
	int size;
	pthread_mutex_t * mutex;
} ThreadData;

typedef struct
{
	int semId;
	int clientMaster;
	int masterClient;
} ClientData;

/************************************************************************
 * Fonction threads
 ************************************************************************/
void * cribleEratosthene(void * arg){
	
	ThreadData *data = (ThreadData *) arg;

	for(int i = 0; i < data->size; i++){
		
		if((i+2)%data->n == 0 && data->tab[i] == true && (i+2)!=data->n){
			// Verrouiller
			pthread_mutex_lock(data->mutex);
			data->tab[i] = false;
			// Déverrouiller
			pthread_mutex_unlock(data->mutex);
		}
	}

	return NULL;
}

/************************************************************************
 * Fonction local
 ************************************************************************/
 void local(int number){
 	int ret, size = number-1;
 	bool *tab = malloc(sizeof(bool)*size);
	for(int i = 0; i < size; i++){
		tab[i] = true;
	}
	pthread_mutex_t monMutex = PTHREAD_MUTEX_INITIALIZER;
	int nbThread = sqrt(number)-1;

	pthread_t tids[nbThread];
	ThreadData datas[nbThread];
	
	// initialisation des structures pour les threads
	for (int i = 0; i < nbThread; i++)
	{
		datas[i].tab = tab;
		datas[i].n = i+2;
		datas[i].size = size;
		datas[i].mutex = &monMutex;
	}
	// lancement des threads
	for (int i = 0; i < nbThread; i++)
	{
	    ret = pthread_create(&tids[i], NULL, cribleEratosthene, &datas[i]); 
	    myassert(ret == 0, "Erreur : création thread");
	}
	
	// attente des threads 
	for (int i = 0; i < nbThread; i++)
	{
	    ret = pthread_join(tids[i], NULL);
	    myassert(ret == 0, "Erreur : join thread");
	}
	
	printf("Nombre premier jusqu'à %d : [", number);
	for(int i = 0; i < size; i++){
		if(tab[i] == true){
			if(i!=size && i!=0){
				printf(", ");
			}
			printf("%d", i+2);
		}
	}
	printf("]\n");
	pthread_mutex_unlock(&monMutex);
	free(tab);
 }

/************************************************************************
 * Fonction initialisation
 ************************************************************************/
 void init(ClientData * datas){
 	int semId = semget(CLE, 2, 0);
    			
	int ret = semop(semId, &operationMoinsCritique, 1);
	myassert((ret != -1), "Erreur : semop");
	
	int clientMaster = open(SEM_CLI_MAST, O_WRONLY);
	myassert(clientMaster != -1, "Erreur : open clientMaster");
	int masterClient = open(SEM_MAST_CLI, O_RDONLY);
	myassert(masterClient != -1, "Erreur : open masterClient");
	
	datas->semId = semId;
	datas->clientMaster = clientMaster;
	datas->masterClient = masterClient;
 }
 
 /************************************************************************
 * Fonction destruction
 ************************************************************************/
 void destroy(ClientData * datas){
 	int ret = semop(datas->semId, &operationPlusCritique, 1);
	myassert((ret != -1), "Erreur : semop");
	
	ret = close(datas->masterClient);
	myassert((ret == 0), "Erreur : close masterClient");
	ret = close(datas->clientMaster);
	myassert((ret == 0), "Erreur : close clientMaster");
	
	ret = semop(datas->semId, &operationPlusMaster, 1);
	myassert((ret != -1), "Erreur : semop");
 }

/************************************************************************
 * Fonction principale
 ************************************************************************/

int main(int argc, char * argv[])
{
    int number = 0;
    int order = parseArgs(argc, argv, &number);
    printf("%d\n", order); // pour éviter le warning

    // order peut valoir 5 valeurs (cf. master_client.h) :
    //      - ORDER_COMPUTE_PRIME_LOCAL
    //      - ORDER_STOP
    //      - ORDER_COMPUTE_PRIME
    //      - ORDER_HOW_MANY_PRIME
    //      - ORDER_HIGHEST_PRIME
    //
    // si c'est ORDER_COMPUTE_PRIME_LOCAL
    //    alors c'est un code complètement à part multi-thread
    // sinon
    //    - entrer en section critique :
    //           . pour empêcher que 2 clients communiquent simultanément
    //           . le mutex est déjà créé par le master
    //    - ouvrir les tubes nommés (ils sont déjà créés par le master)
    //           . les ouvertures sont bloquantes, il faut s'assurer que
    //             le master ouvre les tubes dans le même ordre
    //    - envoyer l'ordre et les données éventuelles au master
    //    - attendre la réponse sur le second tube
    //    - sortir de la section critique
    //    - libérer les ressources (fermeture des tubes, ...)
    //    - débloquer le master grâce à un second sémaphore (cf. ci-dessous)
    // 
    // Une fois que le master a envoyé la réponse au client, il se bloque
    // sur un sémaphore ; le dernier point permet donc au master de continuer
    //
    // N'hésitez pas à faire des fonctions annexes ; si la fonction main
    // ne dépassait pas une trentaine de lignes, ce serait bien.
    
    if(order == ORDER_COMPUTE_PRIME_LOCAL){
    	local(number);
    }
    else{
    	ClientData datas;
    	init(&datas);
    	
    	int message[2] = {order, number}, result;
    	
    	int ret = write(datas.clientMaster, &message, 2*sizeof(int));
    	myassert(ret == sizeof(int)*2, "Erreur : write clientMaster");

    	ret = read(datas.masterClient, &result, sizeof(int));
    	myassert(ret == sizeof(int)||ret == 0, "Erreur : read masterClient");
    	
    	switch(order){
    		case ORDER_STOP : 
    				 printf("Tout les workers et le master ont été détruits.\n");
    			break;
    		case ORDER_COMPUTE_PRIME : 
					 printf("%d", number);
					 if(result == 0){
					 	printf(" n'est pas un nombre premier.\n");
					 }
					 else{
					 	printf(" est un nombre premier.\n");
					 }
    			break;
    		case ORDER_HOW_MANY_PRIME : 
    				 printf("Il y a %d nombre(s) premier calculé.\n", result);
    			break;
    		case ORDER_HIGHEST_PRIME : 
    				 printf("Le plus grand nombre premier calculé est %d.\n", result);
    			break;
    	}
		destroy(&datas);
    }
    return EXIT_SUCCESS;
}
