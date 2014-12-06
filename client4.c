#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/wait.h>
#include <sys/sem.h> 
#include <signal.h>
#include <time.h>

int sockFileDesc;

typedef union{
    int val; /* Value for SETVAL */
    struct semid_ds *buf; /* Buffer for IPC_STAT, IPC_SET */
    unsigned short *array; /* Array for GETALL, SETALL */
    struct seminfo *__buf; /* Buffer for IPC_INFO
                                           (Linux-specific) */
}semun;
int sem_id;

char on=1;


float teplota_vonku = 25;
float teplota_pomoc = 25;

void semafor_lock(){
	struct sembuf param_sem;
	param_sem.sem_num = 1;
    	param_sem.sem_op = -1;
    	param_sem.sem_flg = 0;
	semop(sem_id, &param_sem, 1);
}
void semafor_unlock(){
	struct sembuf param_sem;
	param_sem.sem_num = 1;
    	param_sem.sem_flg = 0;
	param_sem.sem_op = 1;
	semop(sem_id, &param_sem, 1);
}

void strata_spoj(int param);

void* nacitaj_teplotu(void *vstup){
	while(on){
		semafor_lock();
		if(send(sockFileDesc,&teplota_vonku, sizeof(teplota_vonku),0)==0)strata_spoj(0);//primanie vonkajsej teploty so serveru
		semafor_unlock();
		usleep(100000);
	}
}


void ctrlc(int param);

int main(int argc,char *argv[]){
	int port;
	char *ip;
	struct sockaddr_in param_sock;
	struct hostent *host;

 	if(argc < 2)	  { perror("Error, nebola zadana IP a port\n"); exit(0);}
	else if (argc > 3){ perror("Error, prilis mnoho parametrov\n"); exit(0);}
	ip = &argv[1][0];	//ip s argumentov
	port = atoi(argv[2]);	// port s argumentov
	
	if((sockFileDesc=socket(AF_INET, SOCK_STREAM, 0))<0){ 				perror("Error, vytvaranie socketu:\n");	exit(0); }
	if((host=gethostbyname(ip))==NULL){						perror("Error, nespravna IP\n"); exit(0); }
	
	param_sock.sin_family=AF_INET;
        param_sock.sin_port=htons(port);
    	param_sock.sin_addr.s_addr=*((in_addr_t *)(host->h_addr_list[0]));
	if((connect(sockFileDesc, (struct sockaddr *)&param_sock, sizeof(param_sock)))!=0){	perror("Error, pripojenie\n"); 	exit(0);  }

	int por = 4;
        send(sockFileDesc,&por, sizeof(por),0); //Porad cislo klienta
	signal(SIGINT, ctrlc);			//Ctrl+c
	signal(SIGPIPE, strata_spoj);		//Strata spojenia
  
	
        printf("Nastavovanie teploty pripojene na IP(%s),PORT(%d)\n",ip,port);
	
	//Vytvorenie vlakna pre primanie vonkajsej teploty
	pthread_t vlaknoTep;
        pthread_create(&vlaknoTep,NULL,&nacitaj_teplotu,NULL);
	
    	if((sem_id = semget(getpid(), 1, 0666 | IPC_CREAT)) < 0){printf("Error, semafor\n"); exit(0); } //vytvaranie semaforu	
	semun init; init.val = 1; semctl(sem_id, 0, SETVAL, init);					//inicializacia semaforu
	while(on){
		semafor_lock();
 		printf("Nastav teplotu vonku na: ");
	       	scanf("%F",&teplota_pomoc);	
		fflush(stdin);
		semafor_lock();
		teplota_vonku = teplota_pomoc;
		semafor_unlock();
                usleep(500000);
	}
}
void strata_spoj(int param){//Strata spoj
	printf("Strata spojenia\n");
	on=0;
	semctl(sem_id, 0, IPC_RMID, NULL);
        if(close(sockFileDesc)<0){ perror("Nepodarilo sa uzavriet socket"); }
	exit(0);
}
void ctrlc(int param){//Ctrl+c
	printf("Zrusenie klienta\n");
	on = 0;
	semctl(sem_id, 0, IPC_RMID, NULL);
        if(close(sockFileDesc)<0){ perror("Nepodarilo sa uzavriet socket"); }
	exit(0);
}
