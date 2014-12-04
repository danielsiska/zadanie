#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/sem.h>

typedef union{
    int val; /* Value for SETVAL */
    struct semid_ds *buf; /* Buffer for IPC_STAT, IPC_SET */
    unsigned short *array; /* Array for GETALL, SETALL */
    struct seminfo *__buf; /* Buffer for IPC_INFO
                                           (Linux-specific) */
}semun;

void sigctrl(int param);
void sigctrl1(int param);
void sigpipe(int param);
void chyba(const char *text,int cislo){
        perror(text);
        exit(cislo);
}

struct structTep{	
float t1;
float t2;
float t3;
float tout;
};

int *pocitadlo=0;
int client;
char zap = 1;
int sem_id;
int poc_klientov=0;
int sockFileDesc;
struct structTep *teplota;
int shmid;
struct sembuf my;
int main(int argc,char *argv[]){
    struct sockaddr_in adresa,pripojilSa;
    int port;
    signal(SIGINT, sigctrl);
    if(argc > 1){
	port = atoi(argv[1]);
    } 
    else{
	printf("Nezadali ste port serveru!!\n");
	exit(0);
    }
    socklen_t velkost;
  if((sockFileDesc=socket(AF_INET, SOCK_STREAM, 0))<0)	chyba("Chyba pri vytvarani socketu:\n",-1);

//vytvorenie parametrickej struktury
    adresa.sin_family=AF_INET;
    adresa.sin_port=htons(port);
    adresa.sin_addr.s_addr=INADDR_ANY;
//Vystavenie socketu
    if(bind(sockFileDesc, (struct sockaddr *)&adresa, sizeof(adresa))<0)	chyba("Chyba pri vystavovani socketu:\n",-2);
//Nastavenie socketu
    if(listen(sockFileDesc,2)<0)						chyba("Chyba pri nastavovani pocuvania socketu:\n",-3);

//vytvor zdielanu pamat
	shmid = shmget(1112, sizeof(struct structTep), 0666 | IPC_CREAT);
        if (shmid == -1){
                perror("Problem s vytvorenim zdielanej pamate\n");
                exit(1);
        }
//pripoj zdielanu pamet
	teplota = shmat(shmid,NULL,0);
        if (teplota == (void *)-1){
                perror("Problem s pripojenim zdielanej pamate\n");
                exit(2);
        }
	teplota->t1 = 20;
	teplota->t2 = 20;
	teplota->t3 = 20;
	teplota->tout = 25;

//Vytvorenie  semaforu
    	if((sem_id = semget(1113, 1, 0666 | IPC_CREAT)) < 0) {
        	printf("Chyba\n");
        	exit(-2);
    	}	
	//Inicializacia semaforu
	semun un;
	un.val = 1;
	semctl(sem_id, 0, SETVAL, un);
//-------------------------
    printf("Server aktivny spusteny na porte %d\n",port);
    while(zap)
    {
	if((client=accept(sockFileDesc, (struct sockaddr *)&pripojilSa, &velkost))<0) chyba("Chyba pri vytvarani spojenia:\n",-4);
	//prijme klientov`
	poc_klientov++;
	switch(fork()){ //vytvori novy proces pre klienta
		case-1: chyba("Chyba pri vytvarani noveho procesu:\n",-104);break;
		case 0: if (close(sockFileDesc)<0)	chyba("Chyba pri zatvarani socketu:\n",-5);
			int por;
			struct timeval timeout;
      
			timeout.tv_sec = 10;
    			timeout.tv_usec = 0;
			if (setsockopt (client, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout,
                	sizeof(timeout)) < 0)	error("setsockopt failed\n");

			signal(SIGINT, sigctrl1);
			signal(SIGPIPE, sigpipe);
			recv(client,&por, sizeof(por),0);
			
			//pripoj zdielanu pamet
		        teplota = shmat(shmid,NULL,0);
   	     	     	if (teplota == (void *)-1){
                		perror("Problem s pripojenim zdielanej pamate\n");
                		exit(2);
        		}
        	
			printf("Pripojeny klient c:%d\n",por);			
			switch(por){
				case 1://klient izba 1
				case 2://klient izba 2
				case 3://klient izba 3
					while(zap){
					//blokovanie semafor----------
						my.sem_num = 1;
        					my.sem_op = -1;	
        					my.sem_flg = 0;
        					semop(sem_id, &my, 1);
					//----------------------------
						send(client,&(*teplota), sizeof(*teplota),0);//odosli teplotu klientovi
					//uvolni semafor--------------
						my.sem_op = 1;
        					semop(sem_id, &my, 1);
					//----------------------------
					}break;
				
				case 4:	while(zap){// klient nastavovanie vonkajsej teploty
                                        //blokovanie semafor----------
                                                my.sem_num = 1;
                                                my.sem_op = -1;
                                                my.sem_flg = 0;
                                                semop(sem_id, &my, 1);
                                        //----------------------------
						recv(client,&(*teplota), sizeof(*teplota),0);
                                       //uvolni semafor--------------
                                                my.sem_op = 1;
                                                semop(sem_id, &my, 1);
                                        //----------------------------

					}break;
			}
			if (close(client)<0)	chyba("Chyba pri zatvarani socketu:\n",-6);
	    		printf("Spojenie ukoncene klient c.%d\n",por);
	    		exit(0);					
		default:
	    		if (close(client)<0)	chyba("Chyba pri zatvarani socketu:\n",-7);
		}
    }

}
void sigctrl(int param)
{
  printf("Server sa vypina\n");
  printf("Vymazavam zdielanu pamet\n");
  if (shmctl(shmid, IPC_RMID, 0) == -1) {
  	perror("Zmazanie zdielanej pamate zlyhalo\n");
        exit(EXIT_FAILURE);
  }
  printf("Vymazavam semafor\n");
  semctl(sem_id, 0, IPC_RMID, NULL);
  sleep(1);	
  if (close(sockFileDesc)<0)    chyba("Chyba pri zatvarani socketu:\n",-7);
  exit(0);
}
void sigctrl1(int param)
{
//  printf("Vypinanie klientov\n");
  zap = 0;
}
void sigpipe(int param){
zap = 0;
}

