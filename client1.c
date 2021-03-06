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
#include <math.h>
#include <termios.h>
#include <fcntl.h>
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

char buff[15];
char on=1;

float teplota_akt=20;
float teplota_ziad=20;
float teplota_vonku = 25;
float teplota_kur = 0;

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
		if(recv(sockFileDesc,&teplota_vonku, sizeof(teplota_vonku),0)==0)strata_spoj(0);//primanie vonkajsej teploty so serveru
		semafor_unlock();
		
	}
}


void vypocet(int signal , siginfo_t * siginfo, void * ptr);
int kbhit(void);
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

	int por = 1;
        send(sockFileDesc,&por, sizeof(por),0); //Porad cislo klienta
	signal(SIGINT, ctrlc);			//Ctrl+c
	signal(SIGPIPE, strata_spoj);		//Strata spojenia
  
	
        printf("Izba %d. pripojena na IP(%s),PORT(%d)\n",por,ip,port);
	
	//Vytvorenie vlakna pre primanie vonkajsej teploty
	pthread_t vlaknoTep;
        pthread_create(&vlaknoTep,NULL,&nacitaj_teplotu,NULL);
	

	//Casovac
	//nastavenie obsluznej funkcie
	sigset_t signalSet;
  	struct sigaction param_timer;
  	sigemptyset(&signalSet);
  	param_timer.sa_sigaction=vypocet;
  	param_timer.sa_flags=SA_SIGINFO;
  	param_timer.sa_mask=signalSet;
  	sigaction(SIGUSR1,&param_timer,NULL);
	//nastavenie signalu
	struct sigevent urc_signal;
  	urc_signal.sigev_notify=SIGEV_SIGNAL;
  	urc_signal.sigev_signo=SIGUSR1;
	//vytvorenie casovaca
	timer_t casovac;
	timer_create(CLOCK_REALTIME, &urc_signal, &casovac);
	//nastavenie casu casovaca
	struct itimerspec nast_cas;
  	nast_cas.it_value.tv_sec=0;
  	nast_cas.it_value.tv_nsec=100000000;
  	nast_cas.it_interval.tv_sec=0;
  	nast_cas.it_interval.tv_nsec=100000000;
  	timer_settime(casovac,CLOCK_REALTIME,&nast_cas,NULL);
	//-----------------------------------------------------
	
    	if((sem_id = semget(getpid(), 1, 0666 | IPC_CREAT)) < 0){printf("Error, semafor\n"); exit(0); } //vytvaranie semaforu	
	semun init; init.val = 1; semctl(sem_id, 0, SETVAL, init);					//inicializacia semaforu
	while(on){
		semafor_lock();
        	if (kbhit()){//caka na stlacenie klavesy
                	char znak = getchar();//prijatie znaku
                	if(znak != '\n'){
                        	sprintf(buff,"%s%c",buff,znak);//prida znak do bufferu
                	}
                	else{
                        	teplota_ziad = atof(buff);//prevedie na float
				buff[0]='\0';//vymaze buffer
                	}
        	}
        	else{
			printf("\033[2J");//vycisti konzolu
      			printf("\033[H");//nastavi na pociatok
			printf("Izba 1.\n");
                	if(teplota_akt < teplota_ziad-0.1) 		printf("teplota aktualna:\t\t\t%f (kurim)\n",teplota_akt);
			else if(teplota_akt > teplota_ziad+0.1)		printf("teplota aktualna:\t\t\t%f (chladim)\n",teplota_akt);
                	else						printf("teplota aktualna:\t\t\t%f \n",teplota_akt);
			printf("teplota ziadana:\t\t\t%f\n",teplota_ziad);
                	printf("teplota vonku:\t\t\t\t%f\n",teplota_vonku);
                	printf("Nastav ziadanu teplotu na:\t\t%s\n",buff);
        	}
		semafor_unlock();
                usleep(500000);
	}
}
void vypocet(int signal , siginfo_t * siginfo, void * ptr){//obsluzna funkcia casovaca
	switch (signal){
    		case SIGUSR1:
			semafor_lock();
			if (teplota_akt < teplota_ziad){
				teplota_kur+=0.1;
			}
			else if(teplota_akt > teplota_ziad){
				teplota_kur-=0.1;
			}
                	teplota_akt = teplota_vonku+teplota_kur;
			semafor_unlock();
		break;
	}
}

int kbhit(void){//funkcia na asynchronne prijimanie znaku z netu
  struct termios oldt, newt;
  int ch;
  int oldf;
 
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
 
  ch = getchar();
 
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf);
 
  if(ch != EOF)
  {
    ungetc(ch, stdin);
    return 1;
  }
 
  return 0;
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
