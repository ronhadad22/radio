/*
 ============================================================================
 Name        : radio_server.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include<stdio.h>
#include<string.h>    //strlen
#include<stdlib.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>    //write
#include<pthread.h> //for threading , link with lpthread
#include <fcntl.h>

struct client_s{
	int up;
	int socket;
	char * ip;
	unsigned short station;

}client_default={0};
typedef struct client_s client;
struct Station_s{
	unsigned long id;
	FILE* song;
	int size;
	char *songName;
};
typedef struct Station_s Station;
int fd11[50] ;
struct sockaddr_in addr11[50];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
unsigned short stationNum=0;

char *argarray[];
int upsongflag=0; // 0 means clear, 1 means busy
//the thread function

void *connection_handler(void *);
void *song_play(void *);
void *user_input(void *);

char * ChangeStation(char * MCgroup,int CurrentStationNumber, char* resultstr);
int numClient=0;
char * NStr[100][15];
fd_set readfds,readfds2;
struct timeval timeout;
Station StaArray[100];
client Client[100];

int main(int argc , char *argv[])
{
	stationNum=argc-4;
	int argnum=argc;
	int socket_desc , client_sock , c,i;
	struct sockaddr_in server , client;
	int threadindex=0;

	u_int yes=1;
	for(i=1;i<argc;i++)
	{
		argarray[i]=argv[i];
	}
	//Create socket
	socket_desc = socket(AF_INET , SOCK_STREAM , 0);
	if (socket_desc == -1)
	{
		printf("Could not create socket");
	}
	puts("Socket created");

	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(atoi(argv[1]));

	if (setsockopt(socket_desc,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes)) < 0) {
		perror("Reusing ADDR failed");
		exit(1);
	}
	//Bind
	if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
	{
		//print the error message
		perror("bind failed. Error");
		return 1;
	}
	puts("bind done");

	//Listen
	listen(socket_desc , 3);
	pthread_mutex_lock(&mutex);
	for(i=0;i<stationNum;i++){

		ChangeStation(argv[2],i,NStr[i]);
		StaArray[i].id=inet_addr(NStr[i]);
		StaArray[i].song=fopen(argv[4+i],"r");
		StaArray[i].songName=argv[4+i];
		if(StaArray[i].song==NULL)
		{
			printf("File open error");
		}
	}
	pthread_mutex_unlock(&mutex);
	//Accept and incoming connection
	puts("Waiting for incoming connections...");
	c = sizeof(struct sockaddr_in);
	pthread_t thread_id[100],thread_id2,thread_id3;


	if( pthread_create( &thread_id2 , NULL ,  song_play , (void*) argv[4]) < 0)
	{
		perror("could not create thread for udp");
		return 1;
	}

	if( pthread_create( &thread_id3 , NULL ,  user_input , (void*) argv[4]) < 0)
	{
		perror("could not create thread for udp");
		return 1;
	}

	while( (client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) )
	{
		puts("Connection accepted");

		if( pthread_create( &thread_id+threadindex, NULL ,  connection_handler , (void*) &client_sock) < 0)
		{
			perror("could not create thread");
			return 1;
		}
		threadindex++;
		puts("Handler assigned");
	}

	if (client_sock < 0)
	{
		perror("accept failed");
		return 1;
	}
	close(socket_desc);
	return 0;
}

/*
 * This will handle connection for each client
 * */
void *song_play(void *song){
	pthread_mutex_lock(&mutex);

	unsigned short cpynum = stationNum;
	int i=0,read;
	pthread_mutex_unlock(&mutex);
	char ToBuffer[40000];

	char  NecessaryStrings[100][15];
	/* set up destination address */
	/* create what looks like an ordinary UDP socket */
	for(i=0;i<cpynum;i++)
	{

		if ((fd11[i]=socket(AF_INET,SOCK_DGRAM,0)) < 0) {
			perror("socket");
			exit(1);
		}
		memset(&addr11[i],0,sizeof(addr11[i]));
		addr11[i].sin_family=AF_INET;
		addr11[i].sin_addr.s_addr=StaArray[i].id;
		addr11[i].sin_port=htons(atoi(argarray[3]));
		int ttl=60,ttl1=1;
		setsockopt(fd11[i], IPPROTO_IP, IP_MULTICAST_IF, &addr11[i],sizeof(struct in_addr));
		setsockopt(fd11[i], IPPROTO_IP, IP_MULTICAST_TTL, &ttl,sizeof(unsigned char));
		setsockopt(fd11[i], IPPROTO_IP, IP_MULTICAST_LOOP,&ttl1, sizeof(unsigned char));
		pthread_mutex_lock(&mutex);
		ChangeStation(argarray[2],i,NecessaryStrings[i]);
		pthread_mutex_unlock(&mutex);
		setsockopt(fd11[i], IPPROTO_IP, IP_ADD_MEMBERSHIP,NecessaryStrings[i], 15);
	}

	while(1){
		for(i=0;i<stationNum;i++)
		{
			read=fread(ToBuffer,1, 1024, StaArray[i].song);

			if(read>0) sendto(fd11[i],ToBuffer,1024,0,(struct sockaddr *) &addr11[i],sizeof(addr11[i]));
			else printf("\n%d could not read from file",read);
			if (read<1024)rewind(StaArray[i].song);

		}
		usleep(62500);
	}
	for(i=0;i<cpynum;i++)
	{
		fclose(StaArray[i].song);
	}

}
void *connection_handler(void *socket_desc)
{
	pthread_mutex_lock(&mutex);
	//Get the socket descriptor
	fd_set readfds;
	struct timeval timeout;
	int sock = *(int*)socket_desc,sizeSongToUp;
	int read_size,i,permit=1,ret,sret;
	int unsigned short s;
	char  Server_message[1024];

	unsigned long MulticastAsNum= inet_addr(argarray[2]);
	unsigned short PortU = htons(atoi(argarray[3]));

	unsigned short stationNumH=0;

	//Receive a message from client
	FD_ZERO(&readfds);
	FD_SET(sock,&readfds);

	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	ret = select(sock+1,&readfds,NULL,NULL,&timeout);
	if((ret>0)&&(FD_ISSET(sock,&readfds))){
		if((read_size = recv(sock , Server_message , 1024 , 0)) > 0 ){
			// need to disconnect client if he timedout.
			if (Server_message[0]==0 && Server_message[1]==0 && Server_message[2]==0) // check type and reserved.
			{
				Server_message[0]=0; // type (needs to be 0) for welcome

				stationNumH=htons(stationNum);
				memcpy (Server_message+1,&stationNumH,2); // insert the station number
				MulticastAsNum=htonl(MulticastAsNum);
				memcpy (Server_message+3,&MulticastAsNum,4); // insert the multicast address
				memcpy (Server_message+7,&PortU,2); // insert the port (udp)
				write (sock, Server_message, 9); // sends the welcome message to the client
				puts("we received hello message and sent welcome");
				numClient++; //MUTEX
				pthread_mutex_unlock(&mutex);
				Client[sock].socket=sock;
				Client[sock].station=0;
				Client[sock].up=1;

			}
			else
			{
				perror("ERROR HELLO MESSAGE\n");
				return 0;
			}
		}
		else perror("hello receive fail");
	}





	while(1){


		if((read_size = recv(sock , Server_message , 1024 , 0)) > 0)
		{

			if (Server_message[0]==1)// if Server_message[0]==1 -> asksong message

			{
				pthread_mutex_lock(&mutex);
				if (stationNum>=htons(*(unsigned short*)&(Server_message[1]))) // if the station exists return announce message
				{
					Client[sock].station=htons(*(unsigned short*)&(Server_message[1]));

					Server_message[0]=1; // type (needs to be 1) for announce
					Server_message[1]=(unsigned char)strlen(StaArray[Client[sock].station].songName); // song name size (in bytes)
					memcpy (Server_message+2,StaArray[Client[sock].station].songName,strlen(StaArray[Client[sock].station].songName));
					write (sock, Server_message, strlen(StaArray[Client[sock].station].songName)+2); // sends the announce message to the client
				}
				else
				{
					printf ("station number out of boundry %d \n",*(unsigned short*)&(Server_message[1]));
				}
				pthread_mutex_unlock(&mutex);

			}
			else if (Server_message[0]==2)  //up song
			{

				sizeSongToUp=*(unsigned int*)&(Server_message[1]);// song size (in bytes)
				char  songToUP[Server_message[5]];
				memcpy(songToUP,&(Server_message[6]),(unsigned int)Server_message[5]); //copy the song name
				pthread_mutex_lock(&mutex);
				for(i=0;i<stationNum;i++){
					if(strcmp((StaArray[i].songName),(songToUP))== 0){
						permit=0;
						printf("the song already exists.\n");
						break;
					}

				}//for

				if (upsongflag==1)permit=0;
				pthread_mutex_unlock(&mutex);
				if((200>sizeSongToUp)||(sizeSongToUp>10000000))permit=0;
				Server_message[0]=2; // type (needs to be 2) for permit song
				Server_message[1]=permit;
				write (sock, Server_message,2); // sends the announce message to the client
				if(permit==1){
					pthread_mutex_lock(&mutex);
					upsongflag=1;
					pthread_mutex_unlock(&mutex);
					FILE * pFile;
					pFile = fopen (songToUP, "w");
					int sizereceived=0;
					while(1){
						FD_ZERO(&readfds);
						FD_SET(sock,&readfds);

						sret = select (sock+1,&readfds,NULL,NULL,NULL);
						if((sret>0)&&(FD_ISSET(sock,&readfds))){
							read_size = recv(sock , Server_message , 1024 , 0);
							fwrite (Server_message , sizeof(char),read_size, pFile);
							sizereceived+=read_size;
							if (sizereceived==sizeSongToUp) //send a new station
							{
								pthread_mutex_lock(&mutex);
								s=htons(stationNum+1);
								pthread_mutex_unlock(&mutex);
								StaArray[stationNum].song=fopen(songToUP,"r");
								StaArray[stationNum].songName=songToUP;

								if(StaArray[stationNum].song==NULL)
								{
									printf("File open error");
								}
								ChangeStation(argarray[2],stationNum,NStr[stationNum]);
								int ttl1=1 , ttl =60;
								char  NecessaryStrings[100][15];
								StaArray[stationNum].id=inet_addr(NStr[stationNum]);
								addr11[stationNum].sin_family=AF_INET;
								addr11[stationNum].sin_addr.s_addr=StaArray[stationNum].id;
								addr11[stationNum].sin_port=htons(atoi(argarray[3]));

								if ((fd11[stationNum]=socket(AF_INET,SOCK_DGRAM,0)) < 0) {
									perror("socket");
									exit(1);
								}
								setsockopt(fd11[stationNum], IPPROTO_IP, IP_MULTICAST_IF, &addr11[stationNum],sizeof(struct in_addr));
								setsockopt(fd11[stationNum], IPPROTO_IP, IP_MULTICAST_TTL, &ttl,sizeof(unsigned char));
								setsockopt(fd11[stationNum], IPPROTO_IP, IP_MULTICAST_LOOP,&ttl1, sizeof(unsigned char));
								pthread_mutex_lock(&mutex);
								ChangeStation(argarray[2],stationNum,NecessaryStrings[stationNum]);
								pthread_mutex_unlock(&mutex);
								setsockopt(fd11[stationNum], IPPROTO_IP, IP_ADD_MEMBERSHIP,NecessaryStrings[stationNum], 15);
								stationNum++;
								Server_message[0]=4;
								memcpy(Server_message+1,&s,2);
								for(i=0;i<100;i++){

									if(Client[i].up==1){
										Server_message[0]=4;
										memcpy(Server_message+1,&s,2);
										write (Client[i].socket, Server_message,3);
									}
								}
								fclose(pFile);
								break;

							}
						}

					}

				}
				else{
					puts("could not upload");
				}
				permit=1;
				pthread_mutex_lock(&mutex);
				upsongflag=1;
				pthread_mutex_unlock(&mutex);
				// now client needs to upload
				// after the song is uploaded, we need to send a "new station" message broadcasting to all clients


			}
			else
			{
				printf("Invalid Command -> type error\n");
				exit(1);
			}
			//echo;

		}
		else
		{
			perror("recv failed\n");
			pthread_mutex_lock(&mutex);
			numClient--;
			pthread_mutex_unlock(&mutex);
			Client[sock].up=0;
			printf("we are now disconnected");
			close(sock);
			break;
		}

	}//while
}//connection_handler

char * ChangeStation(char * MCgroup,int CurrentStationNumber, char * resultstr){ // increment the station number
	int LastField=0,i=0,dot=0,Index,index2=0;
	char  newMCgroup[15];

	while(MCgroup[index2]!=0)
	{
		newMCgroup[index2]=MCgroup[index2];
		index2++;
	}
	newMCgroup[index2]='\0';
	int a,b;
	while(dot<3)
	{
		if(MCgroup[i]=='.') dot++;
		i++;
	}
	Index=i;
	while(MCgroup[i]!=10 && MCgroup[i]<='9' && MCgroup[i]>='0')
	{
		LastField = LastField*10 + MCgroup[i]-'0';
		i++;
	}
	LastField+=CurrentStationNumber%256; // the new multicast number
	while(i-Index>0)
	{
		a=1;
		for (b=0;b<i-Index-1;b++) a= a*10;

		if(i-Index>1) newMCgroup[Index]=LastField/a+'0';
		else newMCgroup[Index]=LastField+'0';
		LastField = LastField%a;
		Index++;
	}

	index2=0;
	while(newMCgroup[index2]!='\0')
	{
		resultstr[index2]=newMCgroup[index2];
		index2++;
	}
	resultstr[index2]='\0';
	return 	resultstr;
}
void *user_input(void * para1)
{
	char userMessage[10];
	int j,ret,user=0;
	fd_set readfds2;
	while(1){
		FD_ZERO(&readfds2);
		FD_SET(user,&readfds2);
		printf("insert p to print or q to quit\n");
		ret = select(user+1,&readfds2,NULL,NULL,NULL);
		if((ret>0)&&(FD_ISSET(user,&readfds2))){
			fgets(userMessage, sizeof(userMessage), stdin);
			pthread_mutex_lock(&mutex);
			if(userMessage[0]=='q'){

				for(j=0;j<numClient;j++){
					if(Client[j].up==1){
						close(Client[j].socket);}
				}
				printf("we are now disconnected.\n");
				exit(1);
			}

			else if(userMessage[0]=='p'){
				printf("num of stations:%d\n",stationNum);
				for(j=0;j<stationNum;j++){
					printf("\station number:%d song name:%s MC group: %s\n",j,StaArray[j].songName,	NStr[j]);
				}
				printf("num of client:%d\n",numClient);

			}

			else{
				puts("invalid command from user");
			}
			pthread_mutex_unlock(&mutex);
		}}

}
