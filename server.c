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
#include <sys/msg.h>

typedef struct msgtout {
         long    type;
         float   tout;
} messagetout;

void sigctrl(int param);
void sigctrl1(int param);
void sigpipe(int param);

int *pocitadlo=0;
int client;
char zap = 1;
int poc_klientov=0;
int sockFileDesc;
int msqid;
int msqid1;
int msqid2;


int main(int argc,char *argv[]){
    struct sockaddr_in adresa,pripojilSa;
    int port;
    messagetout tout;
    signal(SIGINT, sigctrl);

    if (argc < 1){printf("Zadajte port serveru\n"); exit(0);}
    port = atoi(argv[1]);
    
    socklen_t velkost;
    if((sockFileDesc=socket(AF_INET, SOCK_STREAM, 0))<0){	          perror("Chyba pri vytvarani socketu:\n");exit(0);}

//vytvorenie parametrickej struktury
    adresa.sin_family=AF_INET;
    adresa.sin_port=htons(port);
    adresa.sin_addr.s_addr=INADDR_ANY;
//Vystavenie socketu
    if(bind(sockFileDesc, (struct sockaddr *)&adresa, sizeof(adresa))<0){ perror("Error, vytvaranie socket:\n");   exit(0); }
    if(listen(sockFileDesc,2)<0){ 					  perror("Error, pocuvanie na sockete\n"); exit(0); }	

    printf("Server aktivny spusteny na porte %d\n",port);
    while(zap)
    {
	if((client=accept(sockFileDesc, (struct sockaddr *)&pripojilSa, &velkost))<0){ 
		perror("Error, prijmanie klienta\n");
		exit(0);
	}
	if ((msqid = msgget(1111, IPC_CREAT | 0666 )) < 0) { perror("msgget"); exit(1); }	//vytvorenie zdielanej pamete pre klienta1
	if ((msqid1 = msgget(1112, IPC_CREAT | 0666 )) < 0) { perror("msgget"); exit(1); }	//vytvorenie zdielanej pamete pre klienta2
	if ((msqid2 = msgget(1113, IPC_CREAT | 0666 )) < 0) { perror("msgget"); exit(1); }	//vytvorenie zdielanej pamete pre klienta3

	switch(fork()){ //vytvori novy proces pre klienta
		case-1: perror("Chyba pri vytvarani noveho procesu:\n"); exit(0); break;
		case 0: if (close(sockFileDesc)<0){	perror("Chyba pri zatvarani socketu:\n");exit(0);}
			int por;

			struct timeval timeout;
			timeout.tv_sec = 5;
    			timeout.tv_usec = 0;
			if (setsockopt (client, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout,
                	sizeof(timeout)) < 0)	error("setsockopt failed\n");

			signal(SIGINT, sigctrl1);
			signal(SIGPIPE, sigpipe);
			recv(client,&por, sizeof(por),0);
		
			printf("Pripojeny klient c:%d\n",por);			
			switch(por){
				case 1:	while(zap){
						if (msgrcv(msqid, &tout, sizeof(tout), 1, 0) < 0) { perror("msgrcv"); exit(1); }
						send(client,&tout.tout, sizeof(tout.tout),0);
                                        }break;
				break;
				case 2:		
                                        while(zap){
                                                if (msgrcv(msqid1, &tout, sizeof(tout), 1, 0) < 0) { perror("msgrcv"); exit(1); }
						send(client,&tout.tout, sizeof(tout.tout),0);
                                        }break;
				break;
				case 3:
					while(zap){
						if (msgrcv(msqid2, &tout, sizeof(tout), 1, 0) < 0) { perror("msgrcv"); exit(1); }
						send(client,&tout.tout, sizeof(tout.tout),0);
					}break;
				
				case 4:	
					tout.type = 1;
					while(zap){// klient nastavovanie vonkajsej teploty
						recv(client,&tout.tout, sizeof(tout.tout),0);
						if (msgsnd(msqid, &tout, sizeof(tout), IPC_NOWAIT) < 0) { perror("msgsnd"); exit(1); }
						if (msgsnd(msqid1, &tout, sizeof(tout), IPC_NOWAIT) < 0) { perror("msgsnd"); exit(1); }
						if (msgsnd(msqid2, &tout, sizeof(tout), IPC_NOWAIT) < 0) { perror("msgsnd"); exit(1); }
					}break;
			}
			if (close(client)<0){	perror("Chyba pri zatvarani socketu:\n");exit(0);}
	    		printf("Spojenie ukoncene klient c.%d\n",por);
	    		exit(0);					
		default:
	    		if (close(client)<0){	perror("Chyba pri zatvarani socketu:\n");exit(0);}
		}
    }

}
void sigctrl(int param)
{
  printf("Server sa vypina\n");
  msgctl(msqid,IPC_RMID,NULL);
  msgctl(msqid1,IPC_RMID,NULL);
  msgctl(msqid2,IPC_RMID,NULL);
  sleep(1);	
  if (close(sockFileDesc)<0){    perror("Chyba pri zatvarani socketu:\n");exit(0);}
  exit(0);
}
void sigctrl1(int param)
{
	zap = 0;
}
void sigpipe(int param){
	zap = 0;
}

