#include "unp.h"

#define SERVER_PATH "/tmp/server8132"
#define ODR_PATH "/tmp/odr8132"
static time_t ticks;
static char buff[MAXLINE];
struct sockaddr_un servaddr, odraddr;
char destIp[20], srcIp[20], hostname[10], msgData[MAXLINE];
char srchostname[10], desthostname[10];
int srcport, destport; 
void timeserver ();
void GetHostname(char addr[], int srcflag);
void msg_recv(int sockfd);
void msg_send(int sockfd, char destIp[], int destport, char sourceIp[], char buf[], int flag);
int flag =0, srcflag = 0;

int main(int argc, char const *argv[])
{
	int sockfd;

	sockfd = Socket(AF_LOCAL, SOCK_DGRAM, 0);
	unlink(SERVER_PATH);
	bzero(&servaddr, sizeof(servaddr));
	bzero(&odraddr, sizeof(odraddr));
	servaddr.sun_family = AF_LOCAL;
	odraddr.sun_family = AF_LOCAL;
	strcpy(servaddr.sun_path, SERVER_PATH);
	strcpy(odraddr.sun_path, ODR_PATH);
	Bind(sockfd, (SA *) &servaddr, sizeof(servaddr));
	//printf("Server bound to socket\n");
recv:
	msg_recv(sockfd);
	timeserver();
	msg_send(sockfd, srcIp, srcport, destIp, buff, flag);
	goto recv;
	exit(0);
}

void timeserver () 
{
	ticks = time(NULL);
    snprintf(buff, sizeof(buff), "%.24s\r\n", ctime(&ticks));
} //end fucntion

void msg_recv(int sockfd)
{
	int clientlen, i = 0;
	char recvline[MAXLINE];
	struct sockaddr_un cliaddr;
	clientlen = sizeof(cliaddr);
	char *token;
	printf("Server waiting to receive request from clients....\n");
	Recvfrom(sockfd, recvline, MAXLINE, 0, (SA *) &cliaddr, &clientlen);
	//printf("Message received from %s : %s\n", Sock_ntop((SA *) &cliaddr, clientlen), recvline);
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
	//printf("Message from client at node %s: %s\n", srchostname, msgData);
}

void msg_send(int sockfd, char destIp[], int destport, char sourceIp[], char buff[], int flag)
{
	char dataPayload[512], portChar[10], flagChar[10];

	sprintf(portChar, "%d", destport);
	sprintf(flagChar, "%d", flag);

	strcpy(dataPayload, destIp);
	strcat(dataPayload, ",");
	strcat(dataPayload, portChar);
	strcat(dataPayload, ",");
	strcat(dataPayload, sourceIp);
	strcat(dataPayload, ",");
	strcat(dataPayload, buff);
	strcat(dataPayload, ",");
	strcat(dataPayload, flagChar);
	printf("The complete dataPayload before sending to ODR: %s\n", dataPayload);
	GetHostname(destIp, 1); //client hostname
	GetHostname(sourceIp, 0); //Server hostname
	//printf("srchostname: %s\n", srchostname);
	//printf("desthostname: %s\n", desthostname);
	printf("Server at node %s responding to request from client node %s\n", desthostname, srchostname);
	Sendto(sockfd, (void *) dataPayload, sizeof(dataPayload), 0, (SA *)&odraddr, sizeof(odraddr));
	printf("Message sent to ODR successfully and going back to blocking in recvfrom..\n\n");
}

void GetHostname(char addr[], int srcflag)
{
	struct hostent *hptr;
	struct in_addr Ipaddr;
	inet_pton(AF_INET, addr, &Ipaddr);
	hptr = gethostbyaddr(&Ipaddr, sizeof(Ipaddr), AF_INET);
	if (srcflag == 1)
	{
		strcpy(srchostname,hptr->h_name);
	}	
	else if (srcflag == 0)
	{
		strcpy(desthostname,hptr->h_name);
	}
	//printf("Ip address in GetClientHostname: %s\n", addr);
	//printf("Hostname in GetClientHostname: %s\n", hostname);
	//return hostname;
}

