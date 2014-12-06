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

void ctrlc(int param);
void ctrlc_child(int param);
void strata_spojenia(int param);

int client;
int sockFileDesc;
int msqid;
int por;

int main(int argc,char *argv[]){
    struct sockaddr_in param_sock,pripojit;
    int port;
    messagetout tout;

    if (argc < 2){printf("Zadajte port serveru\n"); exit(0);}
    port = atoi(argv[1]);
    
    socklen_t velkost;
    if((sockFileDesc=socket(AF_INET, SOCK_STREAM, 0))<0){	          perror("Chyba pri vytvarani socketu:\n");exit(0);}//vytvaranie socketu

    param_sock.sin_family=AF_INET;
    param_sock.sin_port=htons(port);
    param_sock.sin_addr.s_addr=INADDR_ANY;
    if(bind(sockFileDesc, (struct sockaddr *)&param_sock, sizeof(param_sock))<0){ perror("Error, vytvaranie socket:\n");   exit(0); }//pripojenie parametrov pre socket
    if(listen(sockFileDesc,2)<0){ 					  perror("Error, pocuvanie na sockete\n"); exit(0); }	//nastavenie pocuvania na sockete

    printf("Server aktivny spusteny na porte %d\n",port);
    if ((msqid = msgget(getpid(), IPC_CREAT | 0666 )) < 0) { perror("msgget"); exit(1); }       //vytvorenie zdielanej pamete pre klienta1
    while(1)
    {
	if((client=accept(sockFileDesc, (struct sockaddr *)&pripojit, &velkost))<0){ 
		perror("Error, prijmanie klienta\n");
		exit(0);
	}
	signal(SIGINT, ctrlc);
	switch(fork()){ //vytvori novy proces pre klienta
		case-1: perror("Chyba pri vytvarani noveho procesu:\n"); exit(0); break;
		case 0: if (close(sockFileDesc)<0){	perror("Chyba pri zatvarani socketu:\n");exit(0);} //zatvorenie socketu servera
			struct timeval timeout;
			timeout.tv_sec = 20;
    			timeout.tv_usec = 0;
			if (setsockopt (client, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout,
                	sizeof(timeout)) < 0)	error("Error, nastavenie timeout\n");

			signal(SIGINT, ctrlc_child);
			signal(SIGPIPE, strata_spojenia);
			recv(client,&por, sizeof(por),0);	
			printf("Pripojeny klient c:%d\n",por);			
			switch(por){
				case 1:
				case 2:	
				case 3:while(1){
						if (msgrcv(msqid, &tout, sizeof(tout), 1, 0) > 0){
							if(send(client,&tout.tout, sizeof(tout.tout),0)==0) strata_spojenia(0);//odoslanie klientovi
						}
						else{
							printf("test");
						}
                                        }break;
				case 4:	tout.type = 1;
					while(1){// klient nastavovanie vonkajsej teploty
						recv(client,&tout.tout, sizeof(tout.tout),0);
						msgsnd(msqid, &tout, sizeof(tout), IPC_NOWAIT);
						msgsnd(msqid, &tout, sizeof(tout), IPC_NOWAIT);
						msgsnd(msqid, &tout, sizeof(tout), IPC_NOWAIT);
					}break;
			}	
		default:
	    		if (close(client)<0){	perror("Chyba pri zatvarani socketu:\n");exit(0);}
		}
    }

}
void ctrlc(int param){
	printf("Hlavne vlakno ukoncene\n");
	sleep(1);
        msgctl(msqid,IPC_RMID,NULL);
	if (close(sockFileDesc)<0){   perror("Chyba pri zatvarani socketu:\n");exit(0);}
	exit(0);
}
void ctrlc_child(int param){
	if (close(client)<0){   perror("Chyba pri zatvarani socketu:\n");exit(0);}
        printf("Spojenie ukoncene klient c.%d\n",por);
        exit(0);

}
void strata_spojenia(int param){
	ctrlc_child(0);
}

