#include "unp.h"
#include <netdb.h>
#include "hw_addrs.h"
#include "unp.h"
#include <netpacket/packet.h>
#include <net/ethernet.h>

#define ODR_PATH "/tmp/odr8132"
#define MAX_RETRIES 6

int debug = 0;
char destinationIP[20], sourceIP[20];
char buff[MAXLINE];
struct hostent *clienthptr;
struct in_addr clientIpaddr;
char serverVM[50], clientVM[50];
char destIp[20], srcIp[20], msgData[MAXLINE];
int srcport, destport;
struct sockaddr_un cliaddr, odraddr;
int msg_send(int sockfd, char destIP[], int destPort, char msgData[], int flag);
void msg_recv(int sockfd);
void GetHostByName(char vm[]);
void getlocalhost();
void GetClientHostname();
char* getCurrentTime ();

int main(int argc, char **argv)
{
	int choice, sockfd, destPort, flag = 0, templen, selreturn, retries = 0;
	char choiceChar[5], nameBuff[32], msgData[512];
	struct sockaddr_un temp;
	fd_set rset;
	int maxfdp1, msg_send_return = 0, odr_notreach = 0;
	struct timeval timeout;

	getlocalhost();
	GetClientHostname();
	sockfd = Socket(AF_LOCAL, SOCK_DGRAM, 0);
	bzero(&cliaddr, sizeof(cliaddr));
	bzero(&odraddr, sizeof(odraddr));
	cliaddr.sun_family = AF_LOCAL;
	odraddr.sun_family = AF_LOCAL;
	strcpy(odraddr.sun_path, ODR_PATH);

	strncpy(nameBuff,"/tmp/myTmpFile-XXXXXX",22);
	
	if(mkstemp(nameBuff) < 0)
	{
		err_quit("mkstemp error : %s", strerror(errno));
	}

	unlink(nameBuff);
	
	strcpy(cliaddr.sun_path, nameBuff);
	//printf("Temporary file created : %s\n", cliaddr.sun_path);
	
	Bind(sockfd, (SA*) &cliaddr, sizeof(cliaddr));
	templen = sizeof(temp);
	Getsockname(sockfd, (SA *) &temp, &templen);
	//printf("Client Bound to %s\n", Sock_ntop((SA *) &temp, templen));
	
	printf("Hi, \nPlease choose the server node from vm1...vm10");
	printf("\nIf you want to select vm1, type '1'\n");
	if(scanf("%d", &choice) == 1)
	{
		if((choice < 1) || (choice > 10))
		{
			printf("Incorrect entry!!\nPlease choose a vm number from 1 to 10\n");
			unlink(nameBuff);
			err_quit("usage : <vmNumber>\n");
		}
	}
	else 
	{
		printf("Please choose a vmNumber from 1 to 10\n");
		unlink(nameBuff);
		err_quit("usage : <vmNumber>\n");
	}

	sprintf(choiceChar, "%d", choice);
	strcpy(serverVM, "vm");
	strcat(serverVM, choiceChar);
	//printf("Server vm is %s\n", serverVM);
	GetHostByName(serverVM);
	strcpy(msgData, "Client requesting time service from server");
	destPort = 45000;
send_msg:
	msg_send_return = msg_send(sockfd, destinationIP, destPort, msgData, flag);
	if (msg_send_return == 1)
	{
		odr_notreach = 1;
		printf("ODR not reachable in the local node %s.\nWill retry to reach local ODR till MAX_RETRIES.\n", clientVM);
	}
	flag = 0; //resetting back to normal after 1st retry
	FD_ZERO(&rset);
	FD_SET(sockfd, &rset);
	timeout.tv_sec = 15;
	timeout.tv_usec = 0;
	maxfdp1 = sockfd + 1;
	if (debug)//for debugging only with flag
		selreturn = select(maxfdp1, &rset, NULL, NULL, NULL);
	else 
		selreturn = select(maxfdp1, &rset, NULL, NULL, &timeout);
	if( selreturn < 0 ) //Error handling
	{	
		fprintf(stdout, "Select error: %s\n", strerror(errno));
		err_quit("Quitting the program due to error in select function\n");
	}
	if(FD_ISSET(sockfd, &rset)) //checking if unix socket is readable now
	{
		//printf("Socket is readable now due to message from server. Calling msg_recv.\n");
		msg_recv(sockfd);
	}
	if(selreturn == 0)
	{
		if(odr_notreach == 0)
			printf("Client at node %s: timeout due to no response from server %s\n", clientVM, serverVM);
		else if (odr_notreach == 1)
		{
			printf("Client at node %s: timeout due to local ODR not reachable.\n", clientVM);
			odr_notreach = 0;
		}
		if(retries < MAX_RETRIES)
		{
			if (retries == 0)
			{
				printf("Setting force route discovery flag in the message due to timeout\n");
				flag = 1;
			}	
			retries++;
			printf("Resending time request to server %s at %s\n", serverVM, getCurrentTime());
			printf("Retry count now: %d\n", retries);
			goto send_msg;
		}
		else if (retries >= MAX_RETRIES)
		{
			printf("MAX_RETRIES to server %s reached.\nExiting the program!!\n\n",serverVM);
			exit(0);
		}
	}
	unlink(nameBuff);
}

void GetHostByName(char vm[])
{
	struct hostent *hptr;
	hptr = malloc (sizeof (struct hostent));
	if((hptr = gethostbyname(vm)) == NULL)
			err_quit("gethostbyname error for host: %s", vm);
		if(inet_ntop(hptr->h_addrtype, *(hptr->h_addr_list), destinationIP, sizeof(destinationIP)) == NULL )
			err_quit("inet_ntop error");
	fprintf(stdout,"You have selected %s as the server, the canonical IP address of the server is %s\n", vm, destinationIP);
	//free(hptr);
}

void GetClientHostname()
{
	inet_pton(AF_INET, sourceIP, &clientIpaddr);
	clienthptr = malloc (sizeof (struct hostent));
	clienthptr = gethostbyaddr(&clientIpaddr, sizeof(clientIpaddr), AF_INET);
	strcpy(clientVM,clienthptr->h_name);
	//printf("Client VM in GetClientHostname: %s\n", clientVM);
	//free(clienthptr);
}

int msg_send(int sockfd, char destIP[], int destPort, char msgData[], int flag)
{
	char sockChar[10], portChar[10], flagChar[10], dataPayload[512];
	int sendret;

	sprintf(sockChar, "%d", sockfd);
	sprintf(portChar, "%d", destPort);
	sprintf(flagChar, "%d", flag);

	/*strcpy(dataPayload, sockChar);
	strcat(dataPayload, ",");*/
	strcpy(dataPayload, destIP);
	strcat(dataPayload, ",");
	strcat(dataPayload, portChar);
	strcat(dataPayload, ",");
	strcat(dataPayload, sourceIP);
	strcat(dataPayload, ",");
	strcat(dataPayload, msgData);
	strcat(dataPayload, ",");
	strcat(dataPayload, flagChar);

	printf("The complete dataPayload about to send to server is %s\n", dataPayload);
	printf("\n!!!Client at node %s sending request to server %s!!!\n", clientVM, serverVM);
	sendret = sendto(sockfd, dataPayload, strlen(dataPayload), 0, (SA *) &odraddr, sizeof(odraddr));
	if (sendret < 0)
	{
		if (errno == ECONNREFUSED)
		{
			return 1;
		}
		else
		{
			printf("Sendto error: %s\nExiting the program.\n", strerror(errno));
			exit(1);
		}
	}
	else
		return 0;
}

void getlocalhost()
{
	struct hwa_info	*hwa;
	struct sockaddr	*sa;
	printf("\n");
	for (hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next)
	{
		if(!strcmp(hwa->if_name, "eth0")) //storing my eth0 MAC in a char array
		{
			sa = hwa->ip_addr;
			strcpy(sourceIP,Sock_ntop_host(sa, sizeof(*sa)));
			//printf("Client Canonical: %s\n", sourceIP);
			break;
		}
	}
	free_hwa_info(hwa);
}

void msg_recv(int sockfd)
{
	int odrlen, i = 0;
	char *timebuff;
	char recvline[MAXLINE];
	odrlen = sizeof(odraddr);
	char *token;
	printf("Waiting to recieve message from server..\n\n");
	Recvfrom(sockfd, recvline, MAXLINE, 0, (SA *) &odraddr, &odrlen);
	//timebuff = getCurrentTime();
	//printf("Message received from server %s : %s\n", Sock_ntop((SA *) &odraddr, odrlen), recvline);
	token = strtok(recvline, ",");

	while(token != NULL)
	{
		if(i==0)
		{
			strcpy(destIp, token);
		}
		else if(i == 1)
		{
			destport = atoi(token);
		}
		else if(i == 2)
		{
			strcpy(srcIp, token);
		}
		else if(i == 3)
		{
			srcport = atoi(token);
		}	
		else if(i == 4)
		{
			strcpy(msgData, token);
		}
        token = strtok(NULL, ",");
        i++;
	}
	printf("Client at node %s: Received message from server %s: %s\n", clientVM, serverVM, msgData);
	//printf("Message from server %s: %s\n", srcIp, msgData);
}

char* getCurrentTime () 
{
	time_t ticks;
	ticks = time(NULL);
    snprintf(buff, sizeof(buff), "%.24s\r\n", ctime(&ticks));
    return buff;
} //end fucntion
