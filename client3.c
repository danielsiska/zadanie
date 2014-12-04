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
#include <sys/ipc.h>
#include <sys/sem.h> 
#include <math.h>
#include <signal.h>
#include <time.h>

int sockFileDesc;
struct sockaddr_in adresa;
struct hostent *host;

char zap=1;

typedef union{
    int val; /* Value for SETVAL */
    struct semid_ds *buf; /* Buffer for IPC_STAT, IPC_SET */
    unsigned short *array; /* Array for GETALL, SETALL */
    struct seminfo *__buf; /* Buffer for IPC_INFO
                                           (Linux-specific) */
}semun;
int sem_id;
struct sembuf my;

struct structTep{
float t1;
float t2;
float t3;
float tout;
};
struct structTep teplota = {0,0,0,0};
float takt_last = 20;
float takt;
float tout_last=0;
float trad = 0;
float tnast_last=20;
float t = 0;

float ms = 100; //cas opakovania

void* readSocket(void *arg){
while(zap){
//blokovanie semafor--------------
    	my.sem_num = 1;
    	my.sem_op = -1;
    	my.sem_flg = 0;
	semop(sem_id, &my, 1);
//--------------------------------
	recv(sockFileDesc,&teplota, sizeof(teplota),0);
	if(teplota.tout != tout_last || tnast_last != teplota.t1){
                takt_last = takt;
                t = 0;
//		trad = 0;
		tout_last = teplota.tout;
		tnast_last = teplota.t1;
		printf("zmena");
        }
//uvolni semafor------------------
    	my.sem_op = 1;
	semop(sem_id, &my, 1);
//--------------------------------
}
}
void funkcia(int signal , siginfo_t * siginfo, void * ptr){
	switch (signal){
    		case SIGUSR2:
                //blokovanie semafor----------
                my.sem_num = 1;
                my.sem_op = -1;
                my.sem_flg = 0;
                semop(sem_id, &my, 1);
        
	        takt = (teplota.tout+trad-takt_last)*(1-exp(-(10/9)*t))+takt_last;//vypocet aktualnej teploty
                float e = teplota.t3-takt;					 //vypocet regulacnej odchylky
                if (e > 0)                      trad+=0.1;
                else if (e < 0)                 trad-=0.1;
                //----------------------------
                if(e < 0)               printf("teplota izby 3(klima pustena) : %F\n",takt);
                else if(e > 0)          printf("teplota izby 3(radiator pusteny) : %F\n",takt);
                else                    printf("teplota izby 3 : %F\n",takt);
                t+=(ms/1000);
                //uvolni semafor--------------
                my.sem_op = 1;
                semop(sem_id, &my, 1);
                //----------------------------
		break;
	}
}
void sigend(int param);
void sigpipe(int param);
int main(int argc,char *argv[]){
	int port;
	char *ip;
 	if(argc > 2){
		ip = &argv[1][0];//ip s argumentov
		port = atoi(argv[2]);// port s argumentov
		if((sockFileDesc=socket(AF_INET, SOCK_STREAM, 0))<0){ //vytvorenie socketu
        		perror("Chyba pri vytvarani socketu:\n");
        		exit(1);
    		}
	}
	else{ 
		perror("Chyba, nebola zadana IP a port\n");
		exit(0);
	}
	adresa.sin_family=AF_INET;
   	adresa.sin_port=htons(port);
	if((host=gethostbyname(ip))==NULL){
        	perror("Pocitac neexistuje:\n");
        	exit(2);
    	}
    	adresa.sin_addr.s_addr=*((in_addr_t *)(host->h_addr_list[0]));
	if((connect(sockFileDesc, (struct sockaddr *)&adresa, sizeof(adresa)))!=0){//pripojenie k socketu
        	perror("Chyba pripojenia\n");
        	exit(3);
    	}
	int por = 3;
        send(sockFileDesc,&por, sizeof(por),0); // odosli poradove cislo klienta
	signal(SIGINT, sigend);			//prerusenie pomocou ctrl+c
	signal(SIGPIPE, sigpipe);		//neocakavane prerusenie spojenia so serverom
  
        printf("Klient c.%d pripojeny(%s,%d)\n",por,ip,port);
	
	//Vytvorenie  semaforu
    	if((sem_id = semget(getpid(), 1, 0666 | IPC_CREAT)) < 0) {
        	printf("Chyba\n");
        	exit(-2);
    	}	
	//Inicializacia semaforu
	semun un;
	un.val = 1;
	semctl(sem_id, 0, SETVAL, un);

	//Vytvorenie vlakna pre primanie vonkajsej teploty
	pthread_t vlakno;
        pthread_create(&vlakno,NULL,&readSocket,NULL);
	
	//nastavenie timeru
	sigset_t signalSet;
  	struct sigaction nastavovacia;
  	sigemptyset(&signalSet);
  	nastavovacia.sa_sigaction=funkcia;
  	nastavovacia.sa_flags=SA_SIGINFO;
  	nastavovacia.sa_mask=signalSet;
  	sigaction(SIGUSR2,&nastavovacia,NULL);

	struct sigevent kam;
  	kam.sigev_notify=SIGEV_SIGNAL;
  	kam.sigev_signo=SIGUSR2;

	timer_t casovac;
	timer_create(CLOCK_REALTIME, &kam, &casovac);

	struct itimerspec casik;
  	casik.it_value.tv_sec=0;
  	casik.it_value.tv_nsec=(long)ms*1000000;
  	casik.it_interval.tv_sec=0;
  	casik.it_interval.tv_nsec=(long)ms*1000000;
  	timer_settime(casovac,CLOCK_REALTIME,&casik,NULL);
	//----------
	
	while(zap);

	//Uzatvorenie socketu
	if(close(sockFileDesc)<0){
		perror("Nepodarilo sa uzavriet socket");
		exit(4);
	}

	return 0;
}
void sigpipe(int param){
	printf("Server bol neocakavane zruseny\n");
	zap=0;
	semctl(sem_id, 0, IPC_RMID, NULL);
        if(close(sockFileDesc)<0){
                perror("Nepodarilo sa uzavriet socket");
                exit(4);
        }

	exit(0);

}
void sigend(int param){
	printf("Klient uspesne vypnuty\n");
	zap = 0;
	semctl(sem_id, 0, IPC_RMID, NULL);
        if(close(sockFileDesc)<0){
                perror("Nepodarilo sa uzavriet socket");
                exit(4);
        }
	exit(0);
}
