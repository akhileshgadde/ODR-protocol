#include "hw_addrs.h"
#include "unp.h"
#include <stdint.h>
#include <sys/time.h>
#include <netpacket/packet.h>
#include <net/ethernet.h> 
#define ODR_PATH "/tmp/odr8132"
#define ODR_PROTO 0x6849
#define broadcast 0xff
#define MAX_ITEMS 20
#define odr_stale_timer 600

char my_mac_addr[IF_HADDR] = {0};
char vm_name[50];
char *vm_name1, *vm_name2;
static int send_data_flag = 0; 
int route_stale_timer = 0;
char sun_path_name[108];
uint32_t ts = 0;
//char comp_mac_addr[IF_HADDR] = {0x00, 0x0c, 0x29, 0x49, 0x3f, 0x5c};
struct sockaddr_ll pf, pfrecv, socket_address;

struct msg_header //all numeric values to be in network byte order
{
	//struct ethhdr eh;
	int type;
	char srcIp[20];
	int srcport;
	char destIp[20];
	char msgData[100];
	int destport;
	int hopcount;
	int b_id;
	int rrep_sent_flag;
	int route_force_flag;
}msg_park[100];

struct routing_table
{
	char destIp[20];
	int outIfIndex;
	unsigned char next_macAddr[IF_HADDR];
	int hopcount;
	uint32_t timestamp;
	int b_id;
}route_table[100];

struct hardwarelist
{
	int hwsockfd;
	int hwIndex;
	unsigned char mac[ETH_ALEN];
}hwList[10];

static int clientPort = 50000;

struct odrTable
{
	int port;
	char sun_path[108];
	uint32_t timestamp;
}odr_table[100];

int port_count = 0;
static int interfaceCount = 0, receiveIfIndex = -1, broadcast_id = 0;
int route_count = 0;
char localIP[20];
int usockfd;
int park_count = 0;

void bindHWAddrs();
int getHopCount (char destIp[]);
char* getsunpathOdrTable (int port);
uint32_t rtt_ts1();
uint32_t staletimer(char Ipaddr[]);
void msg_send(char destIp[], int destport, char srcIp[], int srcport, char msgData[], char sun_path_name[]);
void msg_recv();
void readSockets();
void sendPfPacket(int psockfd, unsigned char src_mac[], unsigned char dest_mac[], int index, struct msg_header mh);
void fillPacket(char destIp[], int destport, char srcIp[], int srcport, int reRouteFlag, char msgData[], int type, int parkPacketFlag);
void broadcastMsg(struct msg_header mh);
void recvfrompf (int sockfd);
void add_port(struct odrTable *p,struct odrTable a,int * port_count);
int checkOdrTable(char recvdSunPath[]);
void print_odrtable();
uint32_t odr_staletimer (char recvdSunPath[]);
void delete_odrtable(char recvdSunPath[]);
void add_route(struct routing_table *route_table, struct routing_table rt, int *route_count );
int checkRouteTable(char destIp[], int b_id, int hopcount, int type);
int checkDestIpRouteTable(char destIp[]);
void update_route(struct routing_table rt, int type);
int getBroadcastId (char destIp[]);
void delete_route_table(char destIp[]);
void generateRREP(struct msg_header mh);
void generateData(struct msg_header mh);
void sendReply(struct msg_header mh);
void print_routing_table();
void parkPacket(struct msg_header *msg_park, struct msg_header mh, int *park_count );
void checkParkingTable(char destIp[]);
void delete_park_table(char destIp[]);
char* GetHostname(char ip[]);

int main (int argc, char **argv)
{
	int psockfd;
	struct sockaddr_un odraddr, cliaddr;
	struct odrTable ot;
	//check for input parameters
	if (argc != 2)
		err_quit("usage: ./ODR <staleness parameter in sec>");
	route_stale_timer = atoi(argv[1]);
	//printf("Entered stale timer is : %d\n", route_stale_timer);
	usockfd = Socket(AF_LOCAL, SOCK_DGRAM, 0);
	bzero(&odraddr, sizeof(odraddr));
	odraddr.sun_family = AF_LOCAL;
	strcpy(odraddr.sun_path, ODR_PATH);
	ot.port = 45000; 
	strcpy(ot.sun_path, "/tmp/server8132");
	ot.timestamp = rtt_ts1();
	add_port(odr_table, ot, &port_count); //adding server port details to a table
	unlink(ODR_PATH);
	Bind(usockfd, (SA *) &odraddr, sizeof(odraddr));
	//printf("Bind successful\n");
	bindHWAddrs();
	readSockets();
	exit(0);
}

void bindHWAddrs()
{
	struct hwa_info	*hwa, *hwahead;
	struct sockaddr	*sa;
	struct sockaddr_ll sll, sll2;
	char   *ptr;
	int    i, prflag,comp=1;
	int slllen = sizeof(sll2);
	printf("\n");
	for (hwahead = hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) 
	{
		//printf("Interface before checking: %s\n", hwa->if_name);
		if(strcmp(hwa->if_name, "lo")) //excluding loopback
		{
			if(!strcmp(hwa->if_name, "eth0")) //storing my eth0 ip in a char array(local IP)
			{
				if ( (sa = hwa->ip_addr) != NULL)
					strcpy(localIP,Sock_ntop_host(sa, sizeof(*sa)));
			}	
			if(strcmp(hwa->if_name, "eth0")) //exclusing eth0 for binding
			{
				printf("%s :%s", hwa->if_name, ((hwa->ip_alias) == IP_ALIAS) ? " (alias)\n" : "\n");
				memcpy(hwList[interfaceCount].mac, hwa->if_haddr, IF_HADDR);
				if ( (sa = hwa->ip_addr) != NULL)
					printf("         IP addr = %s\n", Sock_ntop_host(sa, sizeof(*sa)));
						
				prflag = 0;
				i = 0;
				do {
					if (hwa->if_haddr[i] != '\0') {
						prflag = 1;
						break;
					}
				} while (++i < IF_HADDR);

				if (prflag) {
					printf("         HW addr = ");
					ptr = hwa->if_haddr;
					i = IF_HADDR;
					do {
						printf("%.2x%s", *ptr++ & 0xff, (i == 1) ? " " : ":");
					} while (--i > 0);
					printf("\n");
				}

				//printf("\n         interface index = %d\n\n", hwa->if_index);
				hwList[interfaceCount].hwsockfd = Socket(PF_PACKET, SOCK_RAW, htons(ODR_PROTO));
				hwList[interfaceCount].hwIndex = hwa->if_index;
				//printf("\nInterface sockfd %d for interface index: %d\n", hwList[interfaceCount].hwsockfd,hwList[interfaceCount].hwIndex);
				bzero(&sll, sizeof(struct sockaddr_ll));
				sll.sll_family = AF_PACKET;
				sll.sll_ifindex = hwList[interfaceCount].hwIndex;
				sll.sll_protocol = htons(ODR_PROTO);
				sll.sll_halen    = ETH_ALEN;
				Bind(hwList[interfaceCount].hwsockfd, (SA *) &sll, sizeof(sll));
				//printf("Socket successfully bound for index %d\n", hwList[interfaceCount].hwIndex);
				interfaceCount++;
			}
		}
	}
	free_hwa_info(hwahead);
}

void readSockets() //to check if unix socket or PF_PACKET is readable now
{
	int maxfdp1, i, n, x = 10;
	fd_set rset;
	void* recvbuf = (void *) malloc (ETH_FRAME_LEN);
	int size = sizeof(pfrecv);

	for(;;)
	{
		//printf("In select\n\n\n\n\n\n\n\n");
		FD_ZERO(&rset);
		for(i=0; i<interfaceCount; i++)
		{
			FD_SET(hwList[i].hwsockfd, &rset);
			//printf("Setting sockfd:%d\n", hwList[i].hwsockfd);
		}
		FD_SET(usockfd, &rset);

		maxfdp1 = max(usockfd, hwList[interfaceCount-1].hwsockfd) + 1;
		//printf("maxfdp1: %d\n",maxfdp1);

		if(select(maxfdp1, &rset, NULL, NULL, NULL) <0 )
		{	
			if(errno == EINTR)
				continue;
			else
			{
				fprintf(stdout, "Error is : %s\n", strerror(errno));
				err_quit("Error in select function");
			}
		}

		if(FD_ISSET(usockfd, &rset))
		{
			//printf("Unix socket is readable now\n");
			msg_recv();
		}
			

		for(i=0; i<interfaceCount; i++)
			if(FD_ISSET(hwList[i].hwsockfd, &rset))
			{
				//printf("PF_PACKET socket is readable now.\n%d interface is readable now\n", hwList[i].hwIndex);
				recvfrompf(hwList[i].hwsockfd);
				break;
			}
	}
}

void msg_recv() //to read from unix socket (client/server)
{
	int clientlen, i = 0, destport, srcport, hopcount = 0, parkPacketFlag;
	//void* recvline = (void *) malloc (ETH_FRAME_LEN);
	//void* data = recvline + 14;
	//struct msg_header *msghdr;
	//msghdr = (struct msg_header *) data;
	char recvline[MAXLINE], destIp[20], srcIp[20], msgData[100], reRouteFlag, recvdSunPath[108];
	struct sockaddr_un cliaddr;
	struct odrTable ot;
	clientlen = sizeof(cliaddr);
	char *token;
	//printf("Waiting to recieve message from client...\n");
	Recvfrom(usockfd, recvline, MAXLINE, 0, (SA *) &cliaddr, &clientlen);
	printf("Message received from client/server %s : %s\n", Sock_ntop((SA *) &cliaddr, clientlen), recvline);
	strcpy(recvdSunPath, Sock_ntop((SA *) &cliaddr, clientlen));

	//split the parameters
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
			strcpy(msgData, token);
			//printf("msgData is %s\n", msgData);
		}
		else if(i == 4)
		{
			reRouteFlag = atoi(token);
			//printf("Flag in msg_recv: %d\n", reRouteFlag);
		}
		
        token = strtok(NULL, ",");
        i++;
	}

	//add code for ttl field in ODR table
	if (((odr_staletimer(recvdSunPath)) >= odr_stale_timer))
	{
		printf("Deleting %s sun_path from ODR table due to ttl expiry\n", recvdSunPath);
		delete_odrtable(recvdSunPath);
	}
	//check port table
	srcport = checkOdrTable(recvdSunPath);
	parkPacketFlag = 0;
	//add code for checking time_to_live field and checking for the same for odr table entries
	if(srcport == 0) //no entry in odr port table
	{
		strcpy(ot.sun_path, recvdSunPath);
		ot.port = clientPort++;
		ot.timestamp = rtt_ts1();
		add_port(odr_table, ot, &port_count);
		srcport = ot.port;
		parkPacketFlag = 1;
	}
	
	//add code for stale timer for destIp and remove that entry from routing table.

	if(checkDestIpRouteTable(destIp) && ((strcmp(localIP, destIp))))
	{
		if (((staletimer(destIp)) >= route_stale_timer) || (reRouteFlag == 1))
		{
			if(reRouteFlag == 1) //add code for deletion of existing entry in routing table
				printf("New RREQ being generated due to force route discovery flag set for %s\n", GetHostname(destIp));
			else
				printf("Stale timer expired for %s.Deleting route.\n", GetHostname(destIp));
			delete_route_table(destIp);
			printf("After deleting %s from routing table\n", destIp);
			//print_routing_table();
		}
	} 

	if(!(strcmp(localIP, destIp))) // check if destined to local server. Will ignore flag case here(no coding needed).
	{
		strcpy(sun_path_name, getsunpathOdrTable(destport));
		//printf("Sun_path_name of server to send from ODR: %s\n",sun_path_name);
		msg_send(destIp, destport, localIP, srcport, msgData, sun_path_name);
		//send packet to local server and go back to select
	}
	//check destination address in routing table
	else if((checkDestIpRouteTable(destIp))) //destIP present in routing table
	{
		//hop count from routing table
		//hopcount = getHopCount(destIp);
		//printf("Destination IP is in routing table for %s\n", GetHostname(destIp));
		fillPacket(destIp, destport, srcIp, srcport, reRouteFlag, msgData, 2, parkPacketFlag); //fill packet for type 2
		//sendpfpacket()
	}
	else if(!(checkDestIpRouteTable(destIp))) //destination  address not present in routing table or force route discovery flag set.
	{
		//code for parking packet here

		//printf("Destination Ip is not in routing table for %s\n", GetHostname(destIp));
		//fill packet for RREQ
		send_data_flag = 1;
		broadcast_id++; //increment broadcast_id for next RREQ
		fillPacket(destIp, destport, srcIp, srcport, reRouteFlag, msgData, 0, parkPacketFlag); // for RREQ
		//printf("sockfd before calling sendPfPacket:%d\n",hwList[0].hwsockfd);
		//sendPfPacket(hwList[0].hwsockfd);
	}
}

void fillPacket(char destIp[], int destport, char srcIp[], int srcport, int reRouteFlag, char msgData[], int type, int parkPacketFlag)
{
	struct msg_header mh;

	//printf("In FillPacket for type: %d destination: %s\n", type, GetHostname(destIp));
	mh.type = type;
	strcpy(mh.srcIp, srcIp);
	mh.srcport = srcport;
	strcpy(mh.destIp, destIp);
	strcpy(mh.msgData, msgData);
	mh.destport = destport;
	mh.hopcount = 0;
	//mh.payloadsize = msghdr->payloadsize;
	//mh.b_id = broadcast_id;
	mh.rrep_sent_flag = 0; // code for handling rrep_sent flag case
	mh.route_force_flag = reRouteFlag;
	if(mh.type == 0) //RREQ
	{
		mh.b_id = broadcast_id;
		receiveIfIndex = 0;
		mh.type = 2;
		if(parkPacketFlag)
			parkPacket(msg_park, mh, &park_count);
		//printf("Park packet count is : %d\n", park_count);
		mh.type = 0;
		broadcastMsg(mh);
	}
	else if(mh.type == 2) //Type 2 data message
	{
		sendReply(mh);
	}

}

void broadcastMsg(struct msg_header mh) //for braodacasting RREQ
{
	int i=0;
	unsigned char dest_mac[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	//unsigned char dest_mac[ETH_ALEN] ={0x00,0x0c,0x29,0xbb,0x12,0xb4};
	//printf("In broadcastMsg for Destination: %s\n", GetHostname(mh.destIp));
	//printf("Value of interfaceCount before comparision: %d\n", interfaceCount);
	//printf("Value of receiveIfIndex before comparision: %d\n", receiveIfIndex);
	for(i=0;i<interfaceCount;i++)
	{
		//printf("Value of i in comparision: %d\n", i);
		//printf("Value of interfaceCount in comparision: %d\n", interfaceCount);
		//printf("Value of receiveIfIndex in comparision: %d\n", receiveIfIndex);
		if (hwList[i].hwIndex == receiveIfIndex) //not broadcasting on received interface index
		{
			//printf("Received if_index equal: %d\n", receiveIfIndex);
			receiveIfIndex = 0;
			continue;
		}
		//printf("Broadcasting message from index %d\n", hwList[i].hwIndex);
		sendPfPacket(hwList[i].hwsockfd, hwList[i].mac, dest_mac, hwList[i].hwIndex, mh);
	}

}

void sendPfPacket(int psockfd, unsigned char src_mac[], unsigned char dest_mac[], int index, struct msg_header mh)
{
	//printf("In sendPfPacket for Destination %s\n", GetHostname(mh.destIp));
	int s=0,j; /*socketdescriptor*/
	int length = 0; /*length of the received frame*/ 
	void* buffer = (void *)malloc(ETH_FRAME_LEN);
	char *src_vm, *dest_vm, *local_vm;
	/*pointer to ethernet header*/
	unsigned char* etherhead = buffer;
	//unsigned char src_mac[ETH_ALEN];
	//strcpy(src_mac, my_eth[3].mac);
	/*other host MAC address*/
	//unsigned char dest_mac[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
		
	/*userdata in ethernet frame*/
	unsigned char* data = buffer + 14;
		
	/*another pointer to ethernet header*/
	struct ethhdr *eh = (struct ethhdr *)etherhead;
	struct msg_header *msghdr; //= (struct msg_header *) (buffer + 14);
	msghdr = (struct msg_header*) data;
	int send_result = 0;

	//printf("Message type in sendPfPacket %d\n", mh.type);
	//printf("Index in sendPfPacket %d\n", index);
	//filling my message structure
	msghdr->type = mh.type;
	strcpy(msghdr->srcIp, mh.srcIp);
	msghdr->srcport = mh.srcport;
	strcpy(msghdr->destIp, mh.destIp);
	msghdr->destport = mh.destport;
	msghdr->hopcount = mh.hopcount;
	strcpy(msghdr->msgData, mh.msgData);
	//msghdr->payloadsize = 10;
	msghdr->b_id = mh.b_id;
	msghdr->rrep_sent_flag = mh.rrep_sent_flag;
	msghdr->route_force_flag = mh.route_force_flag;

	socket_address.sll_family = PF_PACKET;	
	socket_address.sll_protocol = htons(ODR_PROTO);	
	socket_address.sll_ifindex  = index;
	/*address length*/
	socket_address.sll_halen    = ETH_ALEN;		
	// NULL address for sockaddress_ll structure
	socket_address.sll_addr[0]  = 0x00;		
	socket_address.sll_addr[1]  = 0x00;		
	socket_address.sll_addr[2]  = 0x00;
	socket_address.sll_addr[3]  = 0x00;
	socket_address.sll_addr[4]  = 0x00;
	socket_address.sll_addr[5]  = 0x00;
	/*MAC - end*/
	socket_address.sll_addr[6]  = 0x00;/*not used*/
	socket_address.sll_addr[7]  = 0x00;/*not used*/

	/*set the frame header*/
	memcpy((void*)buffer, (void*)dest_mac, ETH_ALEN);
	memcpy((void*)(buffer+ETH_ALEN), (void*)src_mac, ETH_ALEN);
	eh->h_proto = htons(ODR_PROTO);
	/*send the packet*/
	//printf("Size of buffer in sendpfpacket: %zd\n", strlen(buffer));
	//strcpy(src_vm, GetHostname(mh.srcIp));
	//strcpy(dest_vm, GetHostname(mh.destIp));
	//strcpy(local_vm, GetHostname(localIP));
	//printf("LocalIP: %s\n", localIP);
	//printf("destIP: %s\n", mh.destIp);
	//printf("SourceIp: %s\n", mh.srcIp);
	printf("ODR at node %s: ", GetHostname(localIP));
	printf("Sending Frame Header src %s, dest addr  ", GetHostname(localIP));
	printf("%02x:%02x:%02x:%02x:%02x:%02x\n",dest_mac[0], dest_mac[1], dest_mac[2], dest_mac[3], dest_mac[4], dest_mac[5], dest_mac[6]);
	printf("ODR msg type %d src %s ", mh.type, GetHostname(mh.srcIp));
	printf("dest %s\n", GetHostname(mh.destIp));

	send_result = sendto(psockfd, (void *) buffer, ETH_FRAME_LEN, 0,
		      (SA *)&socket_address, sizeof(socket_address));
	if (send_result < 0) 
	{ 
		printf("Failed to send message to neighboring node ODR.\nSendto failed: Error: %s\n", strerror(errno));
		printf("Continue to run ODR..\n");
		return;
	}
}

void sendReply(struct msg_header mh)
{
	int i, index;
	//unsigned char dest_mac[ETH_ALEN] = {0x00,0x0c,0x29,0xbb,0x12,0xbe};
	unsigned char dest_mac[ETH_ALEN];// = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	unsigned char src_mac[ETH_ALEN];

	//printf("In sendReply: sending packet of type: %d for destination: %s\n", mh.type, GetHostname(mh.destIp));
	for(i=0; i < route_count; i++)
	{
		if(!(strcmp(mh.destIp, route_table[i].destIp)))
		{
			memcpy(dest_mac, route_table[i].next_macAddr, IF_HADDR);
			index = route_table[i].outIfIndex;
			//printf("destIp is %s\n", mh.destIp);
			//printf("Destination mac_addr: %02x:%02x:%02x:%02x:%02x:%02x\n",dest_mac[0], dest_mac[1], dest_mac[2], dest_mac[3], dest_mac[4], dest_mac[5], dest_mac[6]);
			//printf("Sending reply from %d interface\n\n", index);
			break;
		}
	}

	for(i=0; i<interfaceCount; i++)
	{
		if(index == hwList[i].hwIndex)
		{
			sendPfPacket(hwList[i].hwsockfd, hwList[i].mac, dest_mac, hwList[i].hwIndex, mh);
			break;
		}
	}
}

void recvfrompf (int sockfd) //AF_PACKET socket is readable now
{
	int i;
	//void* buffer = (void *)malloc(1518);
	//unsigned char* etherhead = buffer;
	struct msg_header mh;
	struct routing_table rt;
	void* recvbuf = (void *) malloc (ETH_FRAME_LEN);
	int size = sizeof(pfrecv), n = 1;
	void* data = recvbuf + 14;
	//struct ethhdr *eh = (struct ethhdr *)etherhead;
	unsigned char src_mac[6];
	unsigned char dest_mac[6];
	struct msg_header *msghdr;
	msghdr = (struct msg_header *) data;

	Recvfrom(sockfd, recvbuf, ETH_FRAME_LEN, 0, (SA *) &pfrecv, &size);
	printf("\nODR at node %s received a message: msg_type %d from source ", GetHostname(localIP), msghdr->type);
	printf("%s\n", GetHostname(msghdr->srcIp));
	receiveIfIndex = pfrecv.sll_ifindex; //received interface index
	//printf("Message received(PF_PACKET) on index: %d\n", receiveIfIndex);
	mh.type = msghdr->type;
	strcpy(mh.srcIp, msghdr->srcIp);
	mh.srcport = msghdr->srcport;
	strcpy(mh.destIp, msghdr->destIp);
	mh.destport = msghdr->destport;
	mh.hopcount = msghdr->hopcount;
	strcpy(mh.msgData, msghdr->msgData);
	//mh.payloadsize = msghdr->payloadsize;
	mh.b_id = msghdr->b_id;
	mh.rrep_sent_flag = msghdr->rrep_sent_flag;
	mh.route_force_flag = msghdr->route_force_flag;

	//routing table initialization entries  - used only if adding to routing table
	strcpy(rt.destIp, mh.srcIp);
	rt.outIfIndex = pfrecv.sll_ifindex;
	memcpy(rt.next_macAddr, pfrecv.sll_addr, IF_HADDR);
	//printf("Mac in add_route is %02x:%02x:%02x:%02x:%02x:%02x\n",rt.next_macAddr[0], rt.next_macAddr[1], rt.next_macAddr[2], rt.next_macAddr[3], rt.next_macAddr[4], rt.next_macAddr[5], rt.next_macAddr[6]);
	rt.hopcount = mh.hopcount;
	rt.timestamp = rtt_ts1();
	rt.b_id = mh.b_id;

	if ((mh.srcIp == localIP))
	{
		//printf("My RREQ looped back to me. So ignoring it.\n");
		return;
	}
	//check type
	n = checkRouteTable(mh.srcIp, mh.b_id, mh.hopcount, mh.type); //checking for source IP

	if(n > 0) //executes if route not available
	{
		
		if(n == 1) //no entry in routing table
		{
			//printf("checkRouteTable returned 1\n");
			printf("Adding route for %s\n", mh.srcIp);
			add_route(route_table, rt, &route_count); //add to routing table	
		}
		else if(n == 2) //update routing table entry
		{
			//printf("checkRouteTable returned 2\n");
			printf("Updating existing route for %s\n", mh.srcIp);
			update_route(rt, mh.type);	
		}
	}

	if(n == 0 && mh.type == 0) //ignoring duplicate RREQs that do not give us efficient path
	{
		printf("Received RREQ from source %s, but no efficient route. Ignoring it.\n", GetHostname(mh.srcIp));
		return;	
	}

	/*Checking for destination IP staleness now*/
	if(checkDestIpRouteTable(mh.destIp) && (strcmp(localIP, mh.destIp)))
	{
		//if (mh.type ==0) //only for RREQS
		//{
			if (((staletimer(mh.destIp)) >= route_stale_timer) || (mh.route_force_flag == 1))
			{
					if(mh.type == 0)
					{
						if(mh.route_force_flag == 1) //add code for deletion of existing entry in routing table
							printf("New RREQ being generated due to force route discovery flag set for destination %s\n", GetHostname(mh.destIp));
						else
							printf("Stale timer expired for destination %s.Deleting route.\n", mh.destIp);
						delete_route_table(mh.destIp);
						//printf("After deleting %s from routing table\n", mh.destIp);
						//print_routing_table();
					}
			}
		//}
		if ((mh.type == 1) && (mh.route_force_flag == 1)) //RREP
		{
			printf("Updating existing route for destination %s due to force discovery flag set\n", GetHostname(mh.srcIp));
			update_route(rt, mh.type);
		}
	}
	//add code for checking destIp staleness parameter or if force_flag set, just delete that entry from routing table

	if(!(strcmp(localIP, msghdr->destIp))) //I am the destination
	{
		if(mh.type == 0)
		{
			printf("RREQ received from %s ", GetHostname(mh.srcIp)); 
			printf("to %s successfully\n", GetHostname(mh.destIp));
			printf("Initiating RREP from %s", GetHostname(mh.destIp));
			printf("to %s\n", GetHostname(mh.srcIp));
			generateRREP(mh);
			//generate route reply function
		}
		else if(mh.type == 1)
		{
			//strcpy(vm_name1, GetHostname(mh.srcIp));
			//strcpy(vm_name2, GetHostname(mh.destIp));
			printf("Route successfully established between %s ", GetHostname(mh.srcIp));
			printf("and %s\n", GetHostname(mh.destIp));
			//printf("Routing Table before sending data\n");
			//print_routing_table();
			checkParkingTable(mh.srcIp);
			//generateData(mh);//check this funtion
			//send data packet to server
		}
		else if(mh.type == 2)
		{
			printf("Data msg received from node: %s\n", GetHostname(mh.srcIp));
			//printf("Destination port %d\n", mh.destport);
			strcpy(sun_path_name, getsunpathOdrTable(mh.destport));
			//printf("Sun_path_name to send from ODR: %s\n",sun_path_name);
			msg_send(mh.destIp, mh.destport, mh.srcIp, mh.srcport, mh.msgData, sun_path_name); //check this function 
			//differentiate between client and server messages
			//check odrTable and get the sun_path name
		}
	}
		
	else //I am not the destination
	{
		//printf("I'm not the destination\n", n);
		if((checkDestIpRouteTable(mh.destIp))) //entry in routing table exists
		{
			if (mh.type == 0)  //route to destination aready available in my routing table
			//add code for RREP generate and send to client	
			{
				printf("\n******Route to destination %s already available********\n", GetHostname(mh.destIp));
				strcpy(mh.msgData, "RREP packet");
				if (mh.rrep_sent_flag == 0)
					generateRREP(mh);
				mh.rrep_sent_flag = 1;
				strcpy(mh.msgData, "RREQ packet");
				printf ("Broadcasting RREQ due to rrep_sent_flag\n");
				broadcastMsg(mh);
			}

			else if ((mh.type == 1) || (mh.type == 2)) //Data or RREP
			{
				//printf("Forwarding packet to destination %s\n", GetHostname(mh.destIp));
				mh.hopcount = mh.hopcount + 1; //incr hop count
				sendReply(mh);
				checkParkingTable(mh.destIp);
			}
			
		}
		else //no route for destination in routing table 
		{
			if(mh.type == 0) //RREQ
			{
				//printf("Broadcasting RREQ for destination:%s due to no route in routing table\n", GetHostname(mh.destIp));
				mh.hopcount += 1;
				broadcastMsg(mh);	
			}
			else if((mh.type == 1) || (mh.type == 2)) //RREP
			{
				// code for park message
				printf("Parking: msg_type %d packet for destination %s\n", mh.type, GetHostname(mh.destIp));
				parkPacket(msg_park, mh, &park_count);
				//Generate new RREQ for destination IP, store message and send it out after receiving RREP
				strcpy(mh.srcIp, localIP);
				mh.type = 0;
				strcpy(mh.msgData, "RREQ");
				mh.b_id = ++broadcast_id;
				//printf("Increasing broadcast_id after parking: %d\n", broadcast_id);
				mh.rrep_sent_flag = 0;
				mh.route_force_flag = 0;
				mh.hopcount = 0;
				printf("After parking: Broadcasting RREQ for destination:%s due to no route in routing table\n", GetHostname(mh.destIp));
				broadcastMsg(mh);
			}
		}
	}
}
	//printf("Eth type: %x\n", ntohs(msghdr->eh.h_proto));
	//printf("Message header type: %d\n",msghdr->type);
	//printf("srcIp address: %s\n", msghdr->srcIp);

int checkRouteTable(char destIp[], int b_id, int hopcount, int type)
{
	int i = 0;
	//printf("In checkRouteTable for %s\n", GetHostname (destIp));
	for(i=0; i<route_count; i++)
	{
		if(!(strcmp(destIp, route_table[i].destIp))) //destIP in routing table
		{
			if(type == 0)
			{
				if((b_id > route_table[i].b_id) || ((b_id == route_table[i].b_id) && (hopcount<route_table[i].hopcount)))
					return 2; //update the routing table with new entries
				else 
					return 0;
			}
			else if(type == 1)
			{
				if(hopcount < route_table[i].hopcount)
					return 2;
				else 
					return 0;
			}
			else if(type == 2) //data packet - just update timestamp of entry
				return 2;
		}
	}
	return 1;
}

int is_mac_equal(char* addr1, char* addr2)
{
	int k=0;
	printf("addr1:");

	for (k=0; k < 6; k++)
		if(addr1[k] != addr2[k])
			return 1;
	return 0;
}

void add_port(struct odrTable *odr_table,struct odrTable a,int * port_count)
{
	//printf("ODR_TABLE: Adding port\n");
   if ( *port_count < MAX_ITEMS )
   {
      odr_table[*port_count] = a;
      *port_count += 1;
   }
   print_odrtable();
}

int checkOdrTable(char recvdSunPath[])
{
	//printf("In checkOdrTable for file: %s\n", recvdSunPath);
	int i = 0;
	for(i=0;i<port_count;i++)
	{
		if(!(strcmp(recvdSunPath, odr_table[i].sun_path)))
		{
			
			return odr_table[i].port;
		}
	}
	return 0;
}

uint32_t odr_staletimer (char recvdSunPath[])
{
	//printf("In odr_stale_timer for %s\n", recvdSunPath);
	int i = 0, flag =0;
	uint32_t ts;
	struct timeval	tv;
	Gettimeofday(&tv, NULL);
	for(i=0;i<port_count;i++)
	{
		if(!(strcmp(recvdSunPath, odr_table[i].sun_path)))
		{
			ts = (tv.tv_sec - odr_table[i].timestamp);
			flag = 1;
			break;
		}
	}
	if (flag == 0)
		return 0;
	return ts;
}

void delete_odrtable(char recvdSunPath[])
{
   //printf("In delete_odrtable for file: %s\n", recvdSunPath);
   int position = -1, i;
   int last_index = port_count - 1;
   //printf("last_index in delete_odrtable: %d\n", last_index);
   for (i=1;i<port_count;i++) //find position of the element to be deleted
   {
   		if(!(strcmp(recvdSunPath, odr_table[i].sun_path)))
   		{
   			position = i;
   			break;
   		}
   }
   //printf("Position in delete_odrtable: %d for port#: %d\n", position, odr_table[i].port);
   if ((port_count > 0) && (position < port_count) && (position > -1))
   {
      for (i = position; i < last_index; i++)
      {
         odr_table[i] = odr_table[i + 1];
      }
      port_count -= 1;
   }
   print_odrtable();
}

void print_odrtable()
{
	int i=0;
	printf("\n!!!!!!!!!!!SUNPATHTABLE entries!!!!!!!!!!!!\n");
	printf("Port#\t\tSun_path_name\t\tTimestamp\n");
	for (i=0;i<port_count;i++)
	{
		printf("%d\t\t", odr_table[i].port);
		printf("%-20s\t", odr_table[i].sun_path);
		printf("%u\n", odr_table[i].timestamp);
	}
	printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
}


char* getsunpathOdrTable (int port)
{
	//printf("In getsunpathOdrTable\n");
	int i = 0;
	for(i=0;i<port_count;i++)
	{
		if(port == odr_table[i].port)
			return odr_table[i].sun_path;
	}
	//return 0;
}

void add_route(struct routing_table *route_table, struct routing_table rt, int *route_count )
{
	//printf("In add_route\n");
	//printf("destIp is %s\n", rt.destIp);
	//printf("Index is %d\n", rt.outIfIndex);
	//printf("Mac in add_route is %02x:%02x:%02x:%02x:%02x:%02x\n",rt.next_macAddr[0], rt.next_macAddr[1], rt.next_macAddr[2], rt.next_macAddr[3], rt.next_macAddr[4], rt.next_macAddr[5], rt.next_macAddr[6]);
	if(*route_count < MAX_ITEMS)
	{
		route_table[*route_count] = rt;
		*route_count += 1;
	}
	printf("Routing table after add_route for %s\n", rt.destIp);
	print_routing_table();
}

void update_route(struct routing_table rt, int type)
{
	int i;
	//printf("In update_route\n");
	//printf("destIp is %s\n", rt.destIp);
	//printf("Index is %d\n", rt.outIfIndex);
	//printf("Mac in update_route is %02x:%02x:%02x:%02x:%02x:%02x\n",rt.next_macAddr[0], rt.next_macAddr[1], rt.next_macAddr[2], rt.next_macAddr[3], rt.next_macAddr[4], rt.next_macAddr[5], rt.next_macAddr[6]);
	for(i=0;i<route_count;i++)
	{
		if(!(strcmp(rt.destIp, route_table[i].destIp)))
		{
			route_table[i].outIfIndex = rt.outIfIndex;
			memcpy(route_table[i].next_macAddr, rt.next_macAddr, IF_HADDR);
			route_table[i].hopcount = rt.hopcount;
			route_table[i].timestamp = rt.timestamp;
			if(type == 0)
				route_table[i].b_id = rt.b_id;
			else if(type == 1)
				route_table[i].b_id = 0;
		}
	}
	printf("Routing table after update_route for %s\n", rt.destIp);
	print_routing_table();
}

void print_routing_table()
{
	int i;
	printf("####################################Routing Table entries##################################\n");
	printf("No\tDestIp\t\t\tNextMACaddress\t\tIF_Index\tHopcount\tBroadcast_id\tTimestamp\n");
	for(i=0;i<route_count;i++)
	{
		printf("%d.    ", i);
		printf("%s\t\t", route_table[i].destIp);
		printf("%02x:%02x:%02x:%02x:%02x:%02x\t",route_table[i].next_macAddr[0], route_table[i].next_macAddr[1], route_table[i].next_macAddr[2], 
				route_table[i].next_macAddr[3], route_table[i].next_macAddr[4], route_table[i].next_macAddr[5]);
		printf("%8d\t", route_table[i].outIfIndex);
		printf("%8d\t", route_table[i].hopcount);
		printf("%12d\t", route_table[i].b_id);
		printf("%u\n", route_table[i].timestamp);
	}
	printf("############################################################################################\n");

}

void delete_route_table(char destIp[])
{
	
	//printf("In delete_route_table for destIp: %s\n", destIp);
   int position = -1, i;
   int last_index = route_count - 1;
   //printf("last_index: %d\n", last_index);
   for (i=0;i<route_count;i++) //find position of the element to be deleted
   {
   		if(!(strcmp(destIp, route_table[i].destIp)))
   		{
   			position = i;
   			break;
   		}
   }
   //printf("Position in delete_route_table:%d for Ipaddr: %s\n", position, destIp);
   if ((route_count > 0) && (position < route_count) && (position > -1))
   {
      for (i = position; i < last_index; i++)
      {
         route_table[i] = route_table[i + 1];
      }
      route_count -= 1;
   }
   print_routing_table();
}

uint32_t rtt_ts1()
{
	struct timeval	tv;
	Gettimeofday(&tv, NULL);
	ts = (tv.tv_sec);
	return ts;
}

uint32_t staletimer(char Ipaddr[])
{	
	int i;
	struct timeval	tv;
	Gettimeofday(&tv, NULL);
	for(i=0;i<route_count;i++)
	{
		if(!(strcmp(Ipaddr, route_table[i].destIp)))
		{
			ts = (tv.tv_sec - route_table[i].timestamp);
			return ts;
		}
	}
	//printf("Returned stale timer: %u\n", ts);
	return 0;
}

void generateRREP(struct msg_header mh)
{
	char tempIp[20];
	int tempport;
	mh.type = 1;
	//printf("In generate RREP for destination: %s\n", GetHostname (mh.srcIp));
	strcpy(tempIp, mh.srcIp);
	tempport = mh.srcport;
	strcpy(mh.srcIp, mh.destIp);
	mh.srcport = mh.destport;
	strcpy(mh.destIp, tempIp);
	mh.destport = tempport;
	if (checkDestIpRouteTable(mh.destIp)) // for handling RREPS generated by intermediate nodes
	{
		mh.hopcount = getHopCount(mh.destIp);
	}
	else
		mh.hopcount = 0;
	//mh.payloadsize = msghdr->payloadsize;
	mh.b_id = 0; //b_id zero for all RREPs.
	//mh.rrep_sent_flag = 0; //not needed in RREP message.
	//mh.route_force_flag = route_force_flag;
	sendReply(mh);
}

void generateData(struct msg_header mh)
{
	char tempIp[20];
	int tempport;
	mh.type = 2;
	printf("In generateData\n");
	strcpy(tempIp, mh.srcIp);
	tempport = mh.srcport;
	strcpy(mh.srcIp, mh.destIp);
	mh.srcport = mh.destport;
	strcpy(mh.destIp, tempIp);
	mh.destport = tempport;
	mh.hopcount = 0;
	//mh.payloadsize = msghdr->payloadsize;
	//mh.b_id = 0;
	mh.rrep_sent_flag = 0;
	//mh.route_force_flag = reRouteFlag;
	sendReply(mh);
}

int checkDestIpRouteTable(char destIp[])
{
	int i;
	//printf("In checkDestIpRouteTable for %s\n", destIp);
	for(i=0; i<route_count; i++)
	{
		if(!(strcmp(destIp, route_table[i].destIp)))
			return 1;
	}

	return 0;
}

int getHopCount (char destIp[])
{
	int i;
	for(i=0; i<route_count; i++)
	{
		if(!(strcmp(destIp, route_table[i].destIp)))
			return i;
	}
}

int getBroadcastId (char destIp[])
{
	int i;
	for(i=0; i<route_count; i++)
	{
		if(!(strcmp(destIp, route_table[i].destIp)))
			return route_table[i].b_id;
	}
}

void msg_send(char destIp[], int destport, char srcIp[], int srcport, char msgData[], char sun_path_name[]) //sending message to server
{
	char srcportChar[10], destportChar[10], flagChar[10], dataPayload[512];
	struct  sockaddr_un toaddr;
	int sendret = 0;
	
	bzero(&toaddr, sizeof(toaddr));
	toaddr.sun_family = AF_LOCAL;
	strcpy(sun_path_name, getsunpathOdrTable(destport));
	strcpy(toaddr.sun_path, sun_path_name);

	sprintf(srcportChar, "%d", srcport);
	sprintf(destportChar, "%d", destport);
	
	//printf("In msg_send with data to client/server: %s\n", msgData);
	/*strcpy(dataPayload, sockChar);
	strcat(dataPayload, ",");*/
	strcpy(dataPayload, destIp);
	strcat(dataPayload, ",");
	strcat(dataPayload, destportChar);
	strcat(dataPayload, ",");
	strcat(dataPayload, srcIp);
	strcat(dataPayload, ",");
	strcat(dataPayload, srcportChar);
	strcat(dataPayload, ",");
	strcat(dataPayload, msgData);
	
	//printf("The complete dataPayload about to send to server/client is %s\n", dataPayload);

	sendret = sendto(usockfd, dataPayload, 200, 0, (SA *) &toaddr, sizeof(toaddr));
	if (sendret < 0)
	{
		if (errno == ECONNREFUSED)
		{
			printf("Message sending to server failed. Server not available.\nDiscarding the received message\n");
			print_odrtable();
			return;
		}
		else
		{
			printf("Sendto error: %s\nExiting the program.\n", strerror(errno));
			exit(1);
		}
	}
	
	if(destport != 45000)
	{
		delete_odrtable(sun_path_name);	
		//printf("Deleting entry for %s after sending to server\n", sun_path_name);
	}
}

void parkPacket(struct msg_header *msg_park, struct msg_header mh, int *park_count )
{
	//printf("In add_route\n");
	//printf("destIp is %s\n", rt.destIp);
	//printf("Index is %d\n", rt.outIfIndex);
	//printf("Mac in add_route is %02x:%02x:%02x:%02x:%02x:%02x\n",rt.next_macAddr[0], rt.next_macAddr[1], rt.next_macAddr[2], rt.next_macAddr[3], rt.next_macAddr[4], rt.next_macAddr[5], rt.next_macAddr[6]);
	//printf("In parkPacket for destination %s\n", mh.destIp);
	if(*park_count < MAX_ITEMS)
	{
		msg_park[*park_count] = mh;
		*park_count += 1;
	}
	//printf("Parking table after parking\n");
	//print_parking_table();
}

void print_parking_table()
{
	int i;
	for(i=0;i<park_count;i++)
	{
		printf("");
	}
}

void delete_park_table(char destIp[])
{
   //printf("In delete_park_table for destination: %s\n", destIp);
   int position = -1, i;
   int last_index = park_count - 1;
   //printf("last_index in delete_odrtable: %d\n", last_index);
   for (i=0;i<park_count;i++) //find position of the element to be deleted
   {
   		if(!(strcmp(destIp, msg_park[i].destIp)))
   		{
   			position = i;
   		}
   
   		//printf("Position in delete_odrtable: %d for port#: %d\n", position, odr_table[i].port);
	    if ((park_count > 0) && (position < park_count) && (position > -1))
	    {
		    for (i = position; i < last_index; i++)
		    {
		       msg_park[i] = msg_park[i + 1];
		    }
		    park_count -= 1;
		    last_index = park_count - 1;
		    position = -1;
	    }
	}
}

void checkParkingTable(char destIp[])
{
	int i;
	for(i=0;i<park_count;i++)
	{
		if(!(strcmp(destIp, msg_park[i].destIp)))
		{
			sendReply(msg_park[i]);
			delete_park_table(msg_park[i].destIp);
		}
	}
}

char* GetHostname(char ip[])
{
	struct hostent *hptr;
	struct in_addr Ipaddr;
	inet_pton(AF_INET, ip, &Ipaddr);
	hptr = malloc (sizeof (struct hostent));
	hptr = gethostbyaddr(&Ipaddr, sizeof(Ipaddr), AF_INET);
	strcpy(vm_name,hptr->h_name);
	return vm_name;
	//printf("Client VM in GetClientHostname: %s\n", clientVM);
	//free(clienthptr);
}
