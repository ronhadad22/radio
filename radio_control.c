/*
 ============================================================================
 Name        : radio_control.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>


#include <netinet/in.h>
//#include <arpa/inet.h>
#include <unistd.h>
#include<pthread.h> //for threading , link with lpthread
#include <sys/stat.h>
#include <fcntl.h>


void *PlaySong (void *);
int ChangeStation(char * MCgroup,int CurrentStationNumber);
void HandleInvalidCommand(char client_message[]);
int ChangeStationFlag=0,DisconnectFlag=0;// '0' means no change
unsigned long Multicastgroup;
unsigned short CurrentStationNumber=0;
fd_set readfds;

unsigned short portnumberU;
int main(int argv,char *args[]) {
	int fd=0,sret,read_size,myTCPsocket,read1;
	char command[100],*ipAddress = args[1],message[20] ,client_message1[3], client_message[1024],test[3];
	struct timeval timeout;
	FILE* fp;
	struct sockaddr_in server;
	unsigned short numberofstations;
	socklen_t addr_size;
	pthread_t thread_id;

	myTCPsocket = socket(AF_INET,SOCK_STREAM,0);
	if (myTCPsocket < 0)
		perror("ERROR opening socket");

	server.sin_addr.s_addr = inet_addr(ipAddress);
	server.sin_port = htons(atoi(args[2]));
	server.sin_family = AF_INET;
	addr_size=sizeof(server);
	if(connect(myTCPsocket,(struct sockaddr *)&server,addr_size) < 0)
	{
		perror("connect failed. Error");
		return 1;
	}
	puts("Connected\nData received");

	test[0]=0; // hello message
	test[1]=0;
	test[2]=0;
	write(myTCPsocket , test , 3);

	FD_ZERO(&readfds);
	FD_SET(myTCPsocket, &readfds);

	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	sret = select(myTCPsocket+1, &readfds, NULL, NULL, &timeout);
	if (sret == -1)
		perror("select() of hello");
	else if (sret>0){
		if (FD_ISSET(myTCPsocket,&readfds)){
			read_size = recv(myTCPsocket , client_message , 1024 , 0);

			if (client_message[0]==0)// check if its a welcome message
			{
				numberofstations=htons(*(unsigned short*)&(client_message[1]));
				Multicastgroup=*(unsigned long*)&(client_message[3]);
				portnumberU=*(unsigned short*)(client_message+7);
				//memset(client_message, 0, 1024);
				printf("Welcome message received successfully\n");
				printf("Number of stations: %u\n",numberofstations);
			}
			else
			{
				DisconnectFlag=1;
			}

		}
	}

	else if(sret==0) //timeout
	{
		printf("Welcome Message Timed Out.\n");
		DisconnectFlag=1;

	}

	else
	{
		printf("read_size ERROR.\n");
	}

	// plays the song of the first station
	if( pthread_create( &thread_id , NULL ,PlaySong , (void*) &Multicastgroup) < 0)
	{
		perror("could not create thread");
		return 1;
	}

	while(DisconnectFlag==0) //established status
	{
		printf("Insert 'q' to quit\nInsert 's' to upload a song\nOr insert a valid station number.\n");
		FD_ZERO(&readfds);
		FD_SET(fd,&readfds);
		FD_SET(myTCPsocket,&readfds);

		//wait to user or to server message

		sret = select (myTCPsocket+1,&readfds,NULL,NULL,NULL);

		if(sret > 0)
		{
			if (FD_ISSET(myTCPsocket,&readfds)) //read from server
			{
				read_size = recv(myTCPsocket , client_message , 1024 , 0);
				if(client_message[0]==3) HandleInvalidCommand(client_message);
				else if(client_message[0]==4)
					{
					numberofstations=htons(*(unsigned short*)&client_message[1]);
					printf("New station received.\nNumber of stations available: %d\n",numberofstations);
					}
				else
				{
					printf("Server Message not in format, Disconnecting.\n");
					DisconnectFlag=1;
					break;
				}
			}
			else if (FD_ISSET(fd,&readfds))    //read from the user
			{
				int i=0,errorflag=0,tostr=0,upload=0;
				unsigned long filesize;
				short UserNum=0;
				memset((void *)command,0,100);
				read(fd,(void*)command,10);
				if (command[0]=='s' && command[1]==10) //UPLOADsong
				{
					// got 's' as input
					printf("Please insert the name of the song\n");
					read(fd,(void*)command,80);
					while(command[tostr]!=10)tostr++;
					command[tostr]='\0';
					fp = fopen(command,"r");
					if(fp==NULL)
					{
						printf("File open error or not exist\n");
						DisconnectFlag=1;
						break;
					}
					fseek(fp, 0L, SEEK_END);
					filesize = ftell(fp);
					fseek(fp, 0L, SEEK_SET);
					rewind(fp);
					if(200<=filesize && filesize<=10000000)  //need to send upSong
					{
						client_message[0]=2;
						memcpy (client_message+1,&filesize,4);
						client_message[5]=strlen(command);
						memcpy (client_message+6,command,strlen(command));
						write (myTCPsocket, client_message, strlen(command)+6);
						FD_ZERO(&readfds);
						FD_SET(myTCPsocket,&readfds);
						sret = select (myTCPsocket+1,&readfds,NULL,NULL,NULL);
						if (FD_ISSET(myTCPsocket,&readfds)){
							read_size = recv(myTCPsocket , client_message , 1024 , 0);
							if(client_message[0]==2){
								if(client_message[1]==1){//valid upload permit=1
									i=0;
									timeout.tv_sec = 2;
									timeout.tv_usec = 0;
									while(1){
										read1=fread(client_message,1, 1024,fp);

										if(read1>0) write(myTCPsocket,client_message,read1);
										else {printf("\n%d could not read from file",read1);
										fclose(fp);
										break;
										}
										if (read1<1024){
											fclose(fp);
											printf("100%% completed.\nUpload Successful.\n");
											FD_ZERO(&readfds);
											FD_SET(myTCPsocket,&readfds);
											sret = select (myTCPsocket+1,&readfds,NULL,NULL,&timeout);
											if (FD_ISSET(myTCPsocket,&readfds)){
												recv(myTCPsocket , client_message , 1024 , 0);
                                            if (client_message[0]==4){

                                              numberofstations++;
                                              printf("we received new station.\nNumber of stations available %d\n",numberofstations);
                                              break;

                                            }
                                            else HandleInvalidCommand(client_message);
											}
											 break;// timeout


										}
										upload = upload + read1;
											if (upload > (i*filesize)/10 && upload <(10*filesize)/(i+1))
											{
												if(i==5) printf("53.2%% completed.\n");
												else printf("%d%% completed.\n",10*i);
												i++;
											}

										usleep(8000);
									}//while

								}// end of valid upload permit=1
								else
								{
									printf("Server doesn't accept the upload\n");
									fclose(fp);
								}
							}
						}
						if (client_message[0]==4); // new station
						else HandleInvalidCommand(client_message);
					}
					else printf("File size is out of range (200B - 10Mb)");
					//need to check the size of the song
				}
				else if (command[0]=='q' && command[1]==10)
				{
					printf("User inserted 'q' - Closing Connection.\n");
					break;
				}
				else
				{
					if (command[i]<'0' || command[i]>'9') errorflag=1;
					while(command[i]!=10 && command[i]<='9' && command[i]>='0' && errorflag==0)
					{
						UserNum = UserNum*10 + command[i]-'0';
						i++;
					}
					if (UserNum < numberofstations && errorflag==0)
					{

						printf("User inserted %d\n",UserNum);
						memset((void *)client_message, 0, 1024);//clear the message buffer
						client_message[0]=1;
						CurrentStationNumber=UserNum;
						UserNum = htons(UserNum);
						memcpy (client_message+1,&UserNum,2);
						write (myTCPsocket, client_message, 3); //sending asksong message

						FD_ZERO(&readfds);
						FD_SET(myTCPsocket, &readfds);
						sret = select(myTCPsocket+1, &readfds, NULL, NULL, NULL);// waiting for announce
						if (sret == -1)	perror("select()");
						else if (sret>0){
							if(FD_ISSET(myTCPsocket,&readfds)){
								read_size = recv(myTCPsocket , client_message , 1024 , 0);
								if (client_message[0]==3)HandleInvalidCommand(client_message);
								else if(client_message[0]==4) numberofstations=htons(*(unsigned short*)&client_message[1]);
								else if(client_message[0]==1)
								{

									int size = (int)client_message[1];
									char  songname[size];
									memcpy (songname,client_message+2,size);
									printf("%s is now playing...\n",songname);
									ChangeStationFlag=1;

								}
								else
								{
									printf("Server Message not in format, Disconnecting\n");
									DisconnectFlag=1;
									break;
								}

							}
						}
						else if(sret==0)
						{
							printf("TIMED THE FUCK OUT\n");
						}


					}
					else
					{
						if(errorflag==0) printf("Station number is out of range.\n");
						else printf("Wrong Input Format.\n");
					}
				}

			}


		}
		else puts("somthing's wrong with the select" );



	}

	close(myTCPsocket);
	printf("Connection Closed.\n");
	return 0;




	return EXIT_SUCCESS;
}
void *PlaySong(void *Multicastadd)// playing the song we get from the multicast group
{
	unsigned long multicastIp=htonl(*(unsigned long*)(Multicastadd));
	FILE* songfp;
	struct sockaddr_in addr;
	struct ip_mreq mreq;
	int myUDPsocket,addrlen;
	char buffer[1024];
	u_int yes=1;

	myUDPsocket = socket(PF_INET,SOCK_DGRAM,0); //open udp socket
	if (myUDPsocket < 0)
		perror("ERROR opening socket\n");
	if (setsockopt(myUDPsocket,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes)) < 0) {
		perror("Reusing ADDR failed");
		exit(1);
	}

	memset(&addr,0,sizeof(addr)); // set up destination address
	addr.sin_family=PF_INET;
	addr.sin_addr.s_addr=htonl(INADDR_ANY); /* N.B.: differs from sender */
	addr.sin_port= portnumberU;


	if (bind(myUDPsocket,(struct sockaddr *) &addr,sizeof(addr)) < 0) { // bind to receive address
		perror("bind");
		exit(1);
	}

	/* use setsockopt() to request that the kernel join a multicast group */
	mreq.imr_multiaddr.s_addr=inet_addr("224.1.1.1");
	mreq.imr_interface.s_addr=htonl(INADDR_ANY);
	if (setsockopt(myUDPsocket,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq)) < 0) {
		perror("setsockopt");
		exit(1);
	}
	puts("UDP connection established.");
	songfp = popen("play -t mp3 -> /dev/null 2>&1", "w");
	while(1)
	{
		if (ChangeStationFlag==1) // need to make this critical section
		{
			setsockopt(myUDPsocket,IPPROTO_IP,IP_DROP_MEMBERSHIP,&mreq,sizeof(mreq));
			mreq.imr_multiaddr.s_addr=ChangeStation(inet_ntoa(multicastIp),CurrentStationNumber);
			mreq.imr_interface.s_addr=htonl(INADDR_ANY);
			setsockopt(myUDPsocket,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq));
			ChangeStationFlag=0;
		}
		int sret;
		FD_ZERO(&readfds);
		FD_SET(myUDPsocket,&readfds);
		sret = select (myUDPsocket+1,&readfds,NULL,NULL,NULL);
		addrlen=sizeof(addr);
		if(sret > 0)
		{

			if (FD_ISSET(myUDPsocket,&readfds))    //read from the user
			{
				int byts;
				byts=recvfrom(myUDPsocket, buffer, 1024, 0,(struct sockaddr *) &addr, &addrlen);
				if(byts>0){
					fwrite (buffer , sizeof(char), byts, songfp);
				}

				else puts("fail to read\n");
				memset((void *)buffer,0,1024);
			}
		}
		else puts("somthing's wrong with the select.\n" );

	}

	close(myUDPsocket);
	return 0;


}
void HandleInvalidCommand(char client_message[])
{
	char error[client_message[1]+1];
	memcpy(error,client_message+2,client_message[1]+1);
	error[client_message[1]+1]='\0';
	printf("%s",error);
	memset((void *)error,0,client_message[1]);
}
int ChangeStation(char * MCgroup,int CurrentStationNumber){ // increment the station number
	int LastField=0,i=0,dot=0,Index;
	char * newMCgroup = MCgroup;
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
	return 	inet_addr(newMCgroup);
}
