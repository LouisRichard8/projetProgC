#ifndef CLIENT_CRIBLE
#define CLIENT_CRIBLE

// On peut mettre ici des éléments propres au couple master/client :
//    - des constantes pour rendre plus lisible les comunications
//    - des fonctions communes (création tubes, écriture dans un tube,
//      manipulation de sémaphores, ...)

// ordres possibles pour le master
#define ORDER_NONE                0
#define ORDER_STOP               -1
#define ORDER_COMPUTE_PRIME       1
#define ORDER_HOW_MANY_PRIME      2
#define ORDER_HIGHEST_PRIME       3
#define ORDER_COMPUTE_PRIME_LOCAL 4   // ne concerne pas le master

#define CLE ftok("./master_client.h", 0)
#define SEM_MAST_CLI "masterClient"
#define SEM_CLI_MAST "clientMaster"

// bref n'hésitez à mettre nombre de fonctions avec des noms explicites
// pour masquer l'implémentation

extern struct sembuf operationMoinsCritique;
extern struct sembuf operationPlusCritique;
extern struct sembuf operationPlusMaster;
extern struct sembuf operationMoinsMaster;


#endif
