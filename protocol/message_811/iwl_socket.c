#include <stdio.h> //printf
#include <string.h> //memset
#include <stdlib.h> //exit(0);
#include <arpa/inet.h>
#include <sys/types.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <math.h>
#include <stddef.h>
#include "iwl_socket.h"
#include <pthread.h>
#include "jsmn.h"
#include <sys/stat.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include <time.h>
#include <errno.h>

#include <signal.h>//avoid broken pipe.

#define TRY_TIME 100
#define QUEUE_LEN 20

char server_ip[30];
int server_port;

pthread_t sendthread;
pthread_t recvthread;

int lenCount = 0;

char equipment_id[25];
volatile int client_sockfd = -1;
//int client_sockfd = -1;
int alarm_sockfd = -1;
time_t timer;

char* receiver_ssid;
char* transmitter_ssid;
FILE* result;

FILE* logalarm;
FILE* logsend;
FILE* logrecv;

int head = 0;
int tail = 0;
int handler_queue[QUEUE_LEN];
int lock_or_not = 0;

void callBack();

void enqueue(int operation)
{
	int tmp_tail = tail + 1;
	if (head == tail + 1)
	{
		printf("the queue is already full!!!\n");
		return;
	}
	if (tmp_tail >= QUEUE_LEN)
	{
		tmp_tail = 0;
	}
	if (tmp_tail != head)
	{
		// must ensure the queue is not full.
		handler_queue[tail] = operation;
		tail = tmp_tail;
	}
	printf("head: %d,   tail: %d,   operation:%d\n", head, tail, operation);
}

int dequeue()
{
	if (head == tail)
	{
		// means the queue is empty
		return 0;
	}
	else
	{
		int tmp_head = head + 1;
		int result = handler_queue[head];
		if (tmp_head >= QUEUE_LEN)
		{
			tmp_head = 0;
		}
		printf("enter dequeue\n");
		head = tmp_head;
		return result;
	}
}

void get_equipment_id()
{
	FILE* ssid;
	ssid = fopen("/etc/ssid", "r");
	if (ssid == NULL)
	{
		fprintf(result,"open file ssid error:%d\n",errno);
		fflush(result);
		return;
	}
	fgets(equipment_id, 24, ssid);
	int i, slen = sizeof(equipment_id);
	for (i = 0; i < slen; i++)
	{
		if (!((equipment_id[i] >= 'a' && equipment_id[i] <= 'z') || (equipment_id[i] >= 'A' && equipment_id[i] <= 'Z') || (equipment_id[i] >= '0' && equipment_id[i] <= '9') || (equipment_id[i] == '_')))
		{
			equipment_id[i] = '\0';
			break;
		}
	}
	fclose(ssid);
	fprintf(result, "ssid: %s\n", equipment_id);
	fflush(result);
}

unsigned long get_file_size(const char* path)  
{
	unsigned long filesize = -1;      
	struct stat statbuff;  
	if(stat(path, &statbuff) < 0)
	{
		return filesize;  
	}
	else
	{
		filesize = statbuff.st_size;  
	}
	return filesize;  
}

void get_server()
{
	FILE* threshold = fopen("/etc/config/modify","r+");
	if (threshold == NULL)
	{
		printf("open file threshold error:%d\n",errno);
		return;
	}
	char buf[1024];
	memset(buf, 0, 1024 * sizeof(char));

	memset(server_ip, 0, 30 * sizeof(char));
	char port[10];
	memset(port, 0, 10 * sizeof(char));
	
	while (fgets(buf, 1024, threshold) != NULL)
	{
			if (strstr(buf, "ip") != NULL)
			{
				int i;
				bool start = false;
				char* temp = strstr(buf, "ip");
				int k = 0;
				for (i = 0; i < 1024 && temp[i] != '\0'; ++i)
				{
					if (temp[i] == ' ')
					{
						start = true;
						continue;
					}

					if (start == true && temp[i] != ' ')
					{
						server_ip[k] = temp[i];
						k++;
					}
				}
			}

			if (strstr(buf, "port") != NULL)
			{
				int i;
				bool start = false;
				char* temp = strstr(buf, "port");
				int k = 0;
				for (i = 0; i < 1024 && temp[i] != '\0'; ++i)
				{
					if (temp[i] == ' ')
					{
						start = true;
						continue;
					}

					if (start == true && temp[i] != ' ')
					{
						port[k] = temp[i];
						k++;
					}
				}
			}
			
	}

printf("%s\t;%s\n",server_ip,port);
	int i;
	for( i = 0;i< strlen(server_ip)-1;i++)
	{
		server_ip[i] = server_ip[i+1];
		if( !((server_ip[i] >='0' && server_ip[i]<='9') || server_ip[i]=='.' ))
		{
			server_ip[i] = '\0';
		}
	}

	for( i = 0;i< strlen(port)-1;i++)
	{
		port[i] = port[i+1];
		if( !(port[i] >='0' && port[i]<='9'))
		{
			port[i] = '\0';
		}
	}

	server_port = atoi(port);

printf("server_ip = %s\t,server_port = %d\n",server_ip, server_port);

	fclose(threshold);
}




void init_socket()
{
	if(client_sockfd != -1 )
	{
		int ans = close(client_sockfd);
		if(ans == -1)
		{
			time(&timer);
			fprintf(result, "in init:close client_sockfd failed: %d ,time: %ld\n",errno,timer );
			fflush(result);	
		}

	}

	struct sockaddr_in remote_addr;
	memset(&remote_addr, 0, sizeof(remote_addr));
	remote_addr.sin_family = AF_INET;
	//remote_addr.sin_addr.s_addr = inet_addr("120.27.111.23");
	//remote_addr.sin_port = htons(7777);
	remote_addr.sin_addr.s_addr = inet_addr(server_ip);
	remote_addr.sin_port = htons(server_port);

	while(1)
	{
		if ((client_sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
		{
			continue;
		}
		else
		{
			break;
		}
	}
	
	while (1)
	{
		if (connect(client_sockfd, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) < 0)
		{
			continue;
		}
		else
		{
			break;
		}
	}

	struct timeval timeout = {10, 0};
	//setsockopt(client_sockfd, IPPROTO_TCP, SO_SNDTIMEO, (char *)&timeout, sizeof(struct timeval));
	//setsockopt(client_sockfd, IPPROTO_TCP, SO_RCVTIMEO, (char *)&timeout, sizeof(struct timeval));

	
	if(setsockopt(client_sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(struct timeval))<0)
	{
		time(&timer);
		fprintf(result, "setsockopt snd failed: %d ,time: %ld\n",errno,timer );
		fflush(result);
	}

	if(setsockopt(client_sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(struct timeval))<0)
	{
		time(&timer);
		fprintf(result, "setsockopt rcv failed: %d ,time: %ld\n",errno,timer );
		fflush(result);
	}

	time(&timer);
	fprintf(result, "connect to server ,time: %ld\n", timer);
	fflush(result);
}


void send_heart_beat(int client_sockfd, char* equipment_id)
{
	char message[]  = "{ \"type\" : \"heart_beat\", \"equipment_id\":\"" ;
	char end[] = "\"}";
	int real_len = strlen(message) + strlen(equipment_id) + strlen(end) + 2;
	char* real = (char*)malloc(real_len * sizeof(char));
	memset(real, 0, real_len * sizeof(char));
	strcpy(real, message);
	strcat(real, equipment_id);
	strcat(real, end);

	int len = send(client_sockfd, real, strlen(real), 0);
	if (len < 0 && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN))
	{
		time(&timer);
		fprintf(logsend, "send heart_beat error: %d, len:%d, time: %ld\n",errno,len,timer);
	}
	else if (len <= 0 ) 
	{
		time(&timer);
		fprintf(logsend, "send heart_beat error:connect closed!!: %d, time: %ld\n",errno,timer);
		init_socket();
	}
	else
	{
		time(&timer);
		fprintf(logsend, "send heart_beat succ ,time:%ld\n",timer);
	}

	fflush(logsend);
	free(real);
}

void send_hb_in_seperate_thread(int client_sockfd, char* equipment_id)
{
	send_heart_beat(client_sockfd, equipment_id);
	if (lock_or_not == 1)
	{
		dequeue();
		callBack();
	}
}
struct sockaddr_in addr;
void init_alarm_socket()
{
	while(1)
	{
		if ((alarm_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		{
			continue;
		}
		else
		{
			break;		
		}
	}
	//address can be reused.(bind error)
	int opt = 1;
	while(1)
	{
		if( setsockopt(alarm_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0 )
		{
			continue;
		}
		else
		{
			break;
		}
	}
	struct timeval timeout = {3, 0};//3s
	if(setsockopt(alarm_sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout))<0)
	{
		time(&timer);
		fprintf(logalarm, "alarm setsockopt snd failed: %d, time: %ld\n",errno,timer);
		fflush(logalarm);
	}
	if(setsockopt(alarm_sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout))<0)
	{
		time(&timer);
		fprintf(logalarm, "alarm setsockopt rcv failed: %d, time: %ld\n",errno,timer);
		fflush(logalarm);
	}



	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(5656);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	while(1)
	{
		if(bind(alarm_sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
		{
			perror("bind");
			continue;
		}
		else
		{
			break;
		}
	}
}



void  socketRec()
{
	char buffer[20];
	memset(buffer, 0, sizeof(buffer));
	socklen_t addr_len = sizeof(struct sockaddr_in);
	/*int rlen =*/ recvfrom(alarm_sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&addr, &addr_len);
	/*fprintf(logalarm,"after recvfrom in socketRec\n");
	fflush(logalarm);
	if(rlen < 0 && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN))
	{
		time(&timer);
		fprintf(logalarm,"alarm recv error:%d, rlen1: %d, time: %ld\n",errno,rlen,timer);
		fflush(logalarm);
	}
	else if( rlen <= 0 )
	{
		time(&timer);
		fprintf(logalarm,"alarm recv error:%d, rlen2: %d, time: %ld\n",errno,rlen,timer);
		fflush(logalarm);
	}*/
	
	if (strlen(buffer) != 0)
	{
		send_warning_msg(client_sockfd, equipment_id);
	}
}

void send_warning_msg(int client_sockfd, char* equipment_id)
{
	char message[] = "{ \"type\" : \"warning_msg\", \"equipment_id\":\"";
	char end[] = "\"}";
	int real_len = strlen(message) + strlen(equipment_id) + strlen(end) + 2;
	char* real = (char*)malloc(real_len * sizeof(char));
	memset(real, 0, real_len * sizeof(char));
	strcpy(real, message);
	strcat(real, equipment_id);
	strcat(real, end);
	int len = send(client_sockfd, real, strlen(real), 0);
	if (len < 0 &&(errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN))
	{
		time(&timer);
		fprintf(logalarm, "send warning msg error: %d, len: %d, time: %ld\n",errno,len,timer);
	}
	else if (len <= 0)
	{
		time(&timer);
		fprintf(logalarm, "send warning msg error:connect closed!! %d,time: %ld\n",errno, timer);
		init_socket();
	}
	else
	{
		time(&timer);
		fprintf(logalarm, "send warning msg succ, time: %ld\n",timer);
	}
	fflush(logalarm);
	free(real);
}

//---no use---
void send_error_msg(int client_sockfd, char* equipment_id, char* error_msg)
{
	char message[] = "{ \"type\" : \"equip_error_msg\", \"error_msg\":\"";
	char end[] = "\"}";
	int real_len = strlen(message) + strlen(equipment_id) + strlen(end) + 2;
	char* real = (char*)malloc(real_len * sizeof(char));
	memset(real, 0, real_len * sizeof(char));
	strcpy(real, message);
	strcat(real, equipment_id);
	strcat(real, end);

	int len = send(client_sockfd, real, strlen(real), 0);
	if (len < 0 && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN))
	{
		time(&timer);
		fprintf(result, "send error msg error: %d, len: %d, time: %ld\n",errno,len,timer);
	}
	else if (len <= 0)
	{
		time(&timer);
		fprintf(result, "send error msg error:connect closed!! %d, time: %ld\n",errno, timer);
		init_socket();
	}
	else
	{
		time(&timer);
		fprintf(result, "send error msg succ, time: %ld\n", timer);
	}
	fflush(result);
	free(real);
}

void send_reply_for_alarm_got(int client_sockfd, char* equipment_id)
{
	char message[] = "{ \"type\" : \"alarm_got\", \"equipment_id\":\"";
	char end[] = "\"}";
	int real_len = strlen(message) + strlen(equipment_id) + strlen(end) + 2;
	char* real = (char*)malloc(real_len * sizeof(char));
	memset(real, 0, real_len * sizeof(char));
	strcpy(real, message);
	strcat(real, equipment_id);
	strcat(real, end);
	int len = send(client_sockfd, real, strlen(real), 0);
	if (len < 0 && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN))
	{
		time(&timer);
		fprintf(logrecv, "send alarm got reply msg error!!: %d, len: %d, time: %ld\n",errno,len,timer);
	}
	else if (len <= 0)
	{
		time(&timer);
		fprintf(logrecv, "send alarm got reply msg error:connect closed!! %d, time: %ld\n",errno,timer);
		init_socket();
	}
	else
	{
		time(&timer);
		fprintf(logrecv, "send alarm got msg reply succ, time: %ld\n",timer);
	}
	fflush(logrecv);
	free(real);
}

void send_reply_for_transmit_info(int client_sockfd, char* transmitter_ssid, char* receiver_ssid)
{
	char message[] = "{ \"type\" : \"transmit_info_success\", \"transmitter_ssid\":\"";
	char mid[] = "\", \"receiver_ssid\" :\"";
	char end[] = "\"}";
	int real_len = strlen(message) + strlen(transmitter_ssid) + strlen(mid) + strlen(receiver_ssid) + strlen(end) + 2;
	char* real = (char*)malloc(real_len * sizeof(char));
	memset(real, 0, real_len * sizeof(char));
	strcpy(real, message);
	strcat(real, transmitter_ssid);
	strcat(real, mid);
	strcat(real, receiver_ssid);
	strcat(real, end);
	int len = send(client_sockfd, real, strlen(real), 0);
	if (len < 0 && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN))
	{
		time(&timer);
		fprintf(logrecv, "send transmit info  reply msg error:%d, len: %d, time: %ld\n",errno,len,timer);
	}
	else if (len <= 0)
	{
		time(&timer);
		fprintf(logrecv, "send transmit info  reply msg error:connect closed!!  %d, time: %ld\n",errno,timer);
		init_socket();
	}
	else
	{
		time(&timer);
		fprintf(logrecv, "send transmit info reply msg succ, time: %ld\n",timer);
	}
	fflush(logrecv);
	free(real);
}

void send_reply_for_communication_info(int client_sockfd, char* transmitter_ssid, char* router_ssid)
{
	char message[] = "{ \"type\" : \"communication_info_success\", \"transmitter_ssid\":\"";
	char mid[] = "\", \"router_ssid\" :\"";
	char end[] = "\"}";
	int real_len = strlen(message) + strlen(transmitter_ssid) + strlen(mid) + strlen(router_ssid) + strlen(end) + 2;
	char* real = (char*)malloc(real_len * sizeof(char));
	memset(real, 0, real_len * sizeof(char));
	strcpy(real, message);
	strcat(real, transmitter_ssid);
	strcat(real, mid);
	strcat(real, router_ssid);
	strcat(real, end);
	int len = send(client_sockfd, real, strlen(real), 0);
	if (len < 0 && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN))
	{
		time(&timer);
		fprintf(logrecv, "send communication info  reply msg error: %d, len: %d, time: %ld\n",errno,len,timer);
	}
	else if (len <= 0)
	{
		time(&timer);
		fprintf(logrecv, "send communication info  reply msg error:connect closed!!: %d, time: %ld\n",errno,timer);
		init_socket();
	}
	else
	{
		time(&timer);
		fprintf(logrecv, "send communication info reply msg succ, time: %ld\n",timer);
	}
	fflush(logrecv);
	free(real);
}

void edit_config_for_transmit(struct Msg m)
{
	FILE* wireless = fopen("/etc/config/wireless", "r+");
	unsigned long size = get_file_size("/etc/config/wireless");
	if (wireless == NULL)
	{
		fprintf(logrecv,"open file error:%d\n",errno);
		fflush(logrecv);
		return;
	}
	char buf[1024];
	char* all;
	// past
	// memset(&buf, 0, 1024 * sizeof(char));
	// 7-19
	memset(buf, 0, 1024 * sizeof(char));
	int all_len = size + strlen(m.ssid) + strlen(m.passwd) + strlen(m.auth_mode) + strlen(m.encryp_type) + 2;
	all = (char*)malloc(all_len * sizeof(char));
	memset(all, 0, all_len * sizeof(char));
	int isIface = false;
	bool isReceiver = false;


	while (fgets(buf, 1024, wireless) != NULL)
	{
		if (isIface == true)
		{
			if (strstr(buf, "ra0") != NULL)
			{
				isReceiver = true;
				isIface = false;
			}
		}

		else if(isReceiver == true)
		{
			if (strstr(buf, "ssid") != NULL)
			{
				int i;
				bool start = false;
				char* temp = strstr(buf, "ssid");
				int k = 0;
				for (i = 0; i < 1024 && temp[i] != '\0'; ++i)
				{
					if (temp[i] == ' ')
					{
						start = true;
						continue;
					}

					if (start == true && temp[i] != ' ')
					{
						transmitter_ssid[k] = temp[i];
						k++;
					}
				}
			}
			else if (strstr(buf, "ApCliSsid") != NULL)
			{
				memset(buf,0, 1024 * sizeof(char));
				strcpy(buf, "	option ApCliSsid	");
				strcat(buf, m.ssid);
				strcat(buf, "\n");
			}
			else if (strstr(buf, "ApCliPassWord") != NULL)
			{
				memset(buf,0, 1024 * sizeof(char));
				strcpy(buf, "	option ApCliPassWord	");
				strcat(buf, m.passwd);
				strcat(buf, "\n");
			}
			else if (strstr(buf, "ApCliAuthMode") != NULL)
			{
				memset(buf,0, 1024 * sizeof(char));
				strcpy(buf, "	option ApCliAuthMode	");
				strcat(buf, m.auth_mode);
				strcat(buf, "\n");
			}
			else if (strstr(buf, "ApCliEncrypType") != NULL)
			{
				memset(buf,0, 1024 * sizeof(char));
				strcpy(buf, "	option ApCliEncrypType	");
				strcat(buf, m.encryp_type);
				strcat(buf, "\n");
			}
		}
		if (strstr(buf, "wifi-iface") != NULL)
		{
			isIface = true;
		}
		strcat(all, buf);
		memset(buf, 0, 1024 * sizeof(char));
	}
	fclose(wireless);
	FILE* wireless2 = fopen("/etc/config/wireless", "w+");
	if (wireless2 == NULL)
	{
		fprintf(logrecv,"open file wireless2 error:%d\n",errno);
		fflush(logrecv);
		return;
	}
	fprintf(wireless2, "%s\n", all);
	fflush(wireless2);	
	free(all);
	fclose(wireless2);
	time(&timer);
	fprintf(logrecv, "edit transmit config succ,time: %ld\n",timer);
	fflush(logrecv);
}

void edit_config_for_receiver(struct Msg m)
{
	FILE* wireless = fopen("/etc/config/wireless", "r+");
	unsigned long size = get_file_size("/etc/config/wireless");
	if (wireless == NULL)
	{
		fprintf(logrecv,"open file wireless error: %d\n",errno);
		fflush(logrecv);
		return;
	}
	char buf[1024];
	char* all;
	memset(buf, 0, 1024 * sizeof(char));
	int all_len = size + strlen(m.ssid) + 2;
	all = (char*)malloc(all_len * sizeof(char));
	memset(all, 0, all_len * sizeof(char));
	int isIface = false;
	bool isTransmit = false;
	bool isReceiver = false;
	
	while (fgets(buf, 1024, wireless) != NULL)
	{
		if (isIface == true)
		{
			if (strstr(buf, "ra0") != NULL)
			{
				isReceiver = true;
				isIface = false;
			}
			else if (strstr(buf, "radio0") != NULL)
			{
				isTransmit = true;
				isIface = false;
			}
		}
		else if (isTransmit == true)
		{
			if (strstr(buf, "ssid") != NULL)
			{
				memset(buf,0, 1024*sizeof(char));
				strcpy(buf, "        option ssid ");
				strcat(buf, m.ssid);
				strcat(buf, "\n");
				isTransmit = false;
			}
		}
		else if(isReceiver == true)
		{
			if (strstr(buf, "ssid") != NULL)
			{
				int i;
				bool start = false;
				char* temp = strstr(buf, "ssid");
				int k = 0;
				for (i = 0; i < 1024 && temp[i] != '\0'; ++i)
				{
					if (temp[i] == ' ')
					{
						start = true;
						continue;
					}

					if (start == true && temp[i] != ' ')
					{
						receiver_ssid[k] = temp[i];
						k++;
					}
				}
			}
			/*****delete the ApCliSsid*/
			if (strstr(buf, "ApCliEnable") != NULL)
			{
				memset(buf,0, 1024*sizeof(char));
				strcpy(buf, " ");
			}

			if (strstr(buf, "ApCliSsid") != NULL)
			{
				memset(buf,0, 1024*sizeof(char));
				strcpy(buf, "");
			}
			if (strstr(buf, "ApCliPassWord") != NULL)
			{
				memset(buf,0, 1024*sizeof(char));
				strcpy(buf, "");
			}
			if (strstr(buf, "ApCliAuthMode") != NULL)
			{
				memset(buf,0, 1024*sizeof(char));
				strcpy(buf, " ");
			}
			if (strstr(buf, "ApCliEncrypType") != NULL)
			{
				memset(buf,0, 1024*sizeof(char));
				strcpy(buf, " ");
			}
			/*******the end of delete!!!!!!******/
		}
		if (strstr(buf, "wifi-iface") != NULL)
		{
			isIface = true;
		}
		strcat(all, buf);
		memset(buf, 0, 1024*sizeof(char));
	}
	
	fclose(wireless);
	FILE* wireless2 = fopen("/etc/config/wireless", "w+");
	if (wireless2 == NULL)
	{
		fprintf(logrecv,"open file wireless2 error:%d\n",errno);
		fflush(logrecv);
		return;
	}
	fprintf(wireless2, "%s\n", all);
	fflush(wireless2);	
	free(all);
	fclose(wireless2);

	FILE* network = fopen("/etc/config/network","r+");
	unsigned long size_n = get_file_size("/etc/config/network");
	bool isWWan = false;
	if (network == NULL)
	{
		fprintf(logrecv,"open file network error:%d\n",errno);
		fflush(logrecv);
		return;
	}
	char* all_n;
	memset(buf, 0, 1024 * sizeof(char));
	int all_n_len = 2 * size_n;
	all_n = (char*)malloc(all_n_len * sizeof(char));
	memset(all_n, 0, all_n_len * sizeof(char));
	
	while (fgets(buf, 1024, network) != NULL)
	{
		if (strstr(buf, "\'wwan\'") != NULL)
		{
			isWWan = true;
		}
		if(isWWan == true)		
		{
			if (strstr(buf, "ifname") != NULL)
			{
				memset(buf, 0, 1024 * sizeof(char));
				strcpy(buf, " ");
			}
		}
		strcat(all_n, buf);
		memset(buf, 0, 1024 * sizeof(char));
	}

	fclose(network);
	FILE* network2 = fopen("/etc/config/network", "w+");
	if (network2 == NULL)
	{
		fprintf(logrecv,"open file network2 error:%d\n",errno);
		fflush(logrecv);
		return;
	}

	fprintf(network2, "%s\n", all_n);
	fflush(network2);	
	free(all_n);
	fclose(network2);

	time(&timer);
	fprintf(logrecv, "edit receive config succ, time: %ld\n",timer);
	fflush(logrecv);
}

void receive_message_function()
{
	int len;
	char buf[BUFSIZ];
	memset(buf, 0, BUFSIZ * sizeof(char));
	char* heart_beat_got = "heart_beat_got";
	char* transmit_info = "transmit_info";
	char* communication_info = "communication_info";
	char* alarm = "alarm";

	struct Msg re;
	re.type = (char*) malloc(40 * sizeof(char));
	re.ssid = (char*) malloc(40 * sizeof(char));
	re.passwd = (char*) malloc(40 * sizeof(char));
	re.auth_mode = (char*) malloc(40 * sizeof(char));
	re.encryp_type = (char*) malloc(40 * sizeof(char));
	re.equipment_id = (char*) malloc(40 * sizeof(char));

	memset(re.type, 0, 40*sizeof(char));
	memset(re.ssid, 0, 40*sizeof(char));
	memset(re.passwd, 0, 40*sizeof(char));
	memset(re.auth_mode, 0, 40*sizeof(char));
	memset(re.encryp_type, 0, 40*sizeof(char));
	memset(re.equipment_id, 0, 40*sizeof(char));

	if (lock_or_not == 1)
		len = recv(client_sockfd, &buf, BUFSIZ, MSG_DONTWAIT);
	else
		len = recv(client_sockfd, &buf, BUFSIZ, 0);
	
	
	if( len == -1 )
	{
		lenCount++;
	}
	else 
	{
		lenCount = 0;	
	}
	

	if(len < 0 && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN))
	{
		//time(&timer);
		//fprintf(logrecv,"recv error: %d,  len1: %d, time: %ld\n", errno, len, timer);
		//fflush(logrecv);
		
		if(lenCount >= 9)
		{
			time(&timer);
			fprintf(logrecv,"will init socket: recv error: %d,  len1: %d, time: %ld\n", errno, len, timer);
			fflush(logrecv);
		
			init_socket();	
			lenCount = 0;
		}



		if (lock_or_not == 1)
		{
			dequeue();
			callBack();
		}
		return;
	}
	else if( len <= 0 )
	{
		time(&timer);
		fprintf(logrecv,"recv error: %d,  len2: %d, time: %ld\n", errno, len, timer);
		fflush(logrecv);
		init_socket();		
	
		if (lock_or_not == 1)
		{
			dequeue();
			callBack();
		}
		return;
	}
	

	buf[len] = '\0';
	time(&timer);
	fprintf(logrecv,"content: %s, time: %ld\n",buf,timer);
	fflush(logrecv);

/******recv multi messages!!!!!728*************/
	char delims[]="}{";
	char* bufdelim = NULL;
	bufdelim = strtok( buf, delims );
	while(bufdelim != NULL)
	{
		printf("bufdelim = %s\n",bufdelim);
		parse_json(bufdelim,re);
	
		if (strcmp(re.type, transmit_info) == 0)
		{
			edit_config_for_receiver(re);
			send_reply_for_transmit_info(client_sockfd, re.ssid, equipment_id);
			system("/root/restartnetwork.sh");
			fprintf(logrecv, "network restart succ\n");
			fflush(logrecv);
			init_socket();
			send_heart_beat(client_sockfd, equipment_id);				
		}
		else if (strcmp(re.type, communication_info) == 0)
		{
			edit_config_for_transmit(re);		
			send_reply_for_communication_info(client_sockfd, equipment_id, re.ssid);			
			system("/root/restartnetwork.sh");
			fprintf(logrecv, "network restart succ\n");
			fflush(logrecv);
			init_socket();
			send_heart_beat(client_sockfd, equipment_id);
		}
		else if (strcmp(re.type, alarm) == 0)
		{
			send_reply_for_alarm_got(client_sockfd, equipment_id);
			system("/root/beep.sh");
		}
		else if (strcmp(re.type, heart_beat_got) == 0)
		{
			fprintf(logrecv, "get heart_beat_got msg\n");
			fflush(logrecv);
		}
		else 
		{

		}

		bufdelim = strtok(NULL,delims);
	}
	free(re.type);
	free(re.ssid);
	free(re.passwd);
	free(re.auth_mode);
	free(re.encryp_type);
	free(re.equipment_id);

	if (lock_or_not == 1)
	{
		dequeue();
		callBack();
	}
}

/*****set token 60!!!!! 728 ********************/
void parse_json(char* str, struct Msg wr)
{
	jsmn_parser parser;
	jsmntok_t tokens[60];//728
	jsmn_init(&parser);
	int r = jsmn_parse(&parser, str, strlen(str), tokens, 60);//728
	int s = 0, t = 0, u = 0;
	int flag = -1;

	int type_len = 0, ssid_len = 0, pwd_len  = 0, auth_len = 0, encry_len = 0, equipment_len = 0;

	for (s = 0; s < r; s++) 
	{
			char* tmp = (char*)malloc(40 * sizeof(char));
			memset(tmp, 0, 40*sizeof(char));
			for (t = tokens[s].start, u = 0; t < tokens[s].end; t++, u++)
			{
				tmp[u] = str[t];
			}
			tmp[u] = '\0';
			if (s % 2 == 0) 
			{
				if (strcmp(tmp, "type") == 0)
					flag = 0;		     
				else if (strcmp(tmp, "ssid") == 0)
					flag = 1;
				else if(strcmp(tmp, "passwd") == 0)
					flag = 4;
				else if(strcmp(tmp, "auth_mode") == 0)
					flag = 5;
				else if(strcmp(tmp, "encryp_type") == 0)
					flag = 6;       
				else if(strcmp(tmp, "equipment_id") == 0)
					flag = 7;
				else
					flag = -1;
				free(tmp);
			}
			else
			{
				if (flag == 0)
				{
					strcpy(wr.type, tmp);
					type_len = strlen(wr.type);
					flag = -1;
				}
				else if (flag == 1)
				{
					strcpy(wr.ssid, tmp);
					ssid_len = strlen(wr.ssid);
					flag = -1;
				}
				else if (flag == 4)
				{
					strcpy(wr.passwd, tmp);
					pwd_len = strlen(wr.passwd);
					flag = -1;
				}
				else if (flag == 5)
				{
					strcpy(wr.auth_mode, tmp);
					auth_len = strlen(wr.auth_mode);
					flag = -1;
				}
				else if (flag == 6)
				{
					strcpy(wr.encryp_type, tmp);
					encry_len = strlen(wr.encryp_type);
					flag = -1;
				}
				else if (flag == 7)
				{
					strcpy(wr.equipment_id, tmp);
					equipment_len = strlen(wr.equipment_id);
					flag = -1;
				}
				free(tmp);
			}
	}
	wr.type[type_len] = '\0';
	wr.ssid[ssid_len] = '\0';
	wr.passwd[pwd_len] = '\0';
	wr.auth_mode[auth_len] = '\0';
	wr.encryp_type[encry_len] = '\0';
	wr.equipment_id[equipment_len] = '\0';
}



void sendHandler(int sig)
{
	time(&timer);
	fprintf(logsend,"sendthread interrupt: %d, time: %ld\n", sig, timer);
	fflush(logsend);
	pthread_exit((void *)0);
}

void recvHandler(int sig)
{
	time(&timer);
	fprintf(logrecv,"recvthread interrupt: %d, time: %ld\n", sig, timer);
	fflush(logrecv);
	pthread_exit((void *)0);
}

/*send heart beat test*/
void cycle_send()
{
	signal(SIGUSR1,sendHandler);
	pthread_detach(pthread_self());
	if (lock_or_not == 1)
	{
		while(1)
		{
			if (head == tail)
			{
				enqueue(1);
				send_hb_in_seperate_thread(client_sockfd, equipment_id);
			}
			else
			{
				enqueue(1);
			}
			sleep(60);
			
		}
	}
	else
	{
		while(1)
		{
			send_heart_beat(client_sockfd, equipment_id);
			sleep(60);
		}
	}
}
void iwl_create_sendheart_thread()
{

	int thread_id = -1;
	while (thread_id != 0)
	{
		thread_id = pthread_create(&sendthread, NULL, (void *)cycle_send, NULL);
		
	}
	fprintf(logsend, "create send heart_beat thread succ\n");
	fflush(logsend);
}
/*send heart beat test */


/**cycle receive test*/
void cycle_receive()
{
	signal(SIGUSR2, recvHandler);
	pthread_detach(pthread_self());
	if (lock_or_not == 1)
	{
		while(1)
		{
			if (head == tail)
			{
				enqueue(2);
				receive_message_function();	
				usleep(200000);
			}
			else
			{
				enqueue(2);
			}
		}
	}
	else
	{
		while(1)
		{
			receive_message_function();	
		}
	}
}
void iwl_create_recv_thread()
{
	int thread_id = -1;
	while (thread_id != 0)
	{
		thread_id = pthread_create(&recvthread, NULL, (void *)cycle_receive, NULL);
		
	}
	fprintf(logrecv, "create receive thread succ\n");
	fflush(logrecv);
}
/*receive msg test*/

void callBack()
{
	if (head != tail)
	{
		if (handler_queue[head] == 1)
		{
			send_hb_in_seperate_thread(client_sockfd, equipment_id);
		}
		else if (handler_queue[head] == 2)
		{
			receive_message_function();	
		}
	}
}

void interruptHandler(int sig)
{
	pthread_kill(sendthread, SIGUSR1);
	pthread_kill(recvthread, SIGUSR2);

	time(&timer);
	fprintf(result,"iwl_socket interrupt: %d, time: %ld\n", sig, timer);
	fflush(result);

	int res = close(client_sockfd);//socketfd
	if(res == -1)
		fprintf(result,"in interrupt:close client_sockfd failed: %d\n",errno);
	fflush(result);
	fclose(result);//file	
	fclose(logsend);
	fclose(logrecv);
	fclose(logalarm);

	free(receiver_ssid);//malloc
	free(transmitter_ssid);
	exit(0);
}


int main(int argc, char* argv[])
{
	
	//avoid broken pipe 7-19
	signal(SIGPIPE, SIG_IGN);
	//ctrl c
	signal(SIGINT, interruptHandler);
	//kill 
	signal(SIGTERM, interruptHandler);
	//power?	
	signal(SIGTSTP, interruptHandler);
	

	//terminate signal
	/*signal(SIGABRT, interruptHandler);
	signal(SIGALRM, interruptHandler);
	signal(SIGBUS, interruptHandler);
	signal(SIGCHLD, interruptHandler);
	signal(SIGCONT, interruptHandler);
	signal(SIGFPE, interruptHandler);
	signal(SIGHUP, interruptHandler);
	signal(SIGILL, interruptHandler);
	signal(SIGTRAP, interruptHandler);
	signal(SIGPOLL, interruptHandler);
	signal(SIGPROF, interruptHandler);
	signal(SIGQUIT, interruptHandler);
	signal(SIGSEGV, interruptHandler);
	signal(SIGSYS, interruptHandler);
	signal(SIGTSTP, interruptHandler);
	signal(SIGTTIN, interruptHandler);
	signal(SIGTTOU, interruptHandler);
	signal(SIGVTALRM, interruptHandler);
	signal(SIGXCPU, interruptHandler);
	signal(SIGXFSZ, interruptHandler);*/

	memset(handler_queue, 0, QUEUE_LEN * sizeof(int));
	receiver_ssid = (char *)malloc(100 * sizeof(char));
	memset(receiver_ssid, 0, 100*sizeof(char));
	transmitter_ssid = (char *)malloc(100 * sizeof(char));
	memset(transmitter_ssid, 0, 100*sizeof(char));
	int tran_or_rec = atoi(argv[1]);
	// lock_or_not = atoi(argv[2]);
	result = fopen("/mnt/result.txt", "a+");
	if (result == NULL)
	{
		return -1;
	}
	time(&timer);
	fprintf(result,"open log_result, time: %ld\n",timer);
	fflush(result);
	//send heart beat thread!
	logsend = fopen("/mnt/log_send_heart.txt","a+");
	if (logsend == NULL)
	{
		return -1;
	}
	time(&timer);
	fprintf(logsend,"open log_send_heart, time: %ld\n",timer);
	fflush(logsend);

	//recv msg func thread!
	logrecv = fopen("/mnt/log_recv.txt","a+");
	if (logrecv == NULL)
	{
		return -1;
	}
	time(&timer);
	fprintf(logrecv,"open log_recv, time: %ld\n",timer);
	fflush(logrecv);

	//alarm get thread for alarm!
	logalarm = fopen("/mnt/log_alarm.txt","a+");
	if (logalarm == NULL)
	{
		return -1;
	}
	time(&timer);
	fprintf(logalarm,"open logalarm, time: %ld\n", timer);
	fflush(logalarm);

	get_equipment_id();
	get_server();
	init_socket();

	iwl_create_sendheart_thread();
	iwl_create_recv_thread();

	if (tran_or_rec == 1)
	{
		while(1)
		{
			init_alarm_socket();
			socketRec();
			close(alarm_sockfd);
		}
		fclose(logalarm);
	}
	else 
	{
		while(1)
		{
			
		}
	}

	close(client_sockfd);
	free(receiver_ssid);
	free(transmitter_ssid);

	fclose(result);//file	
	fclose(logsend);
	fclose(logrecv);
	fclose(logalarm);
	
	return 0;
}
