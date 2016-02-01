
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <time.h>
#include "duckchat.h"
#include <unistd.h>
#include <signal.h>
//#include "raw.h"
#define ERR_NO_NUM -1
#define ERR_NO_MEM -2
#define ID_RANGE 999999999

int alarm_stop = false;
unsigned int alarm_period = 10;

void diep(char *s) {
    perror(s);
    exit(1);
}

void delete_channel_and_port(int channeltree_index, int server_index, struct channel_tree all_servers[CHANNEL_MAX]){
	int x;
	char temp_server[SERVER_MAX]; 
	strcpy(temp_server, all_servers[channeltree_index].serverInf[server_index + 1]);
	int temp_port = all_servers[channeltree_index].portNum[server_index + 1];
	for (x = channeltree_index; x < all_servers[channeltree_index].serverNum - 1; x++) {
		strcpy(all_servers[channeltree_index].serverInf[x], temp_server);
		all_servers[channeltree_index].portNum[x] = temp_port;
		strcpy(temp_server, all_servers[channeltree_index].serverInf[x + 1]);
		temp_port = all_servers[channeltree_index].portNum[x+1];
		all_servers[channeltree_index].timerMark[x] = all_servers[channeltree_index].timerMark[x+1];
	}
	all_servers[channeltree_index].serverNum = all_servers[channeltree_index].serverNum - 1;
}

bool ifjoinedchannel(int number_channel_joins, struct channel_info all_channels[CHANNEL_MAX], char channel_name[CHANNEL_MAX]){
    int j;
    for (j = 0; j < number_channel_joins; j++){
        if (strcmp(channel_name, all_channels[j].ch_channel) == 0){
            return true;
        }
    }
    return false;
}

void append(char* repeatName, char add_index) {
    int len = strlen(repeatName);
    repeatName[len] = add_index;
    repeatName[len+1] = '\0';
}

void deleteChannel(int number_channels, struct channel_info all_channels[CHANNEL_NUM], char channel_name[CHANNEL_MAX]) {
    int i;
    struct channel_info temp;
    strcpy(temp.ch_channel, channel_name);
    if (number_channels != 1){
        for (i = 0 ; i < (number_channels-1) ; i ++){
            if (strcmp(temp.ch_channel, all_channels[i].ch_channel) == 0) {
            	//printf("%s\n", all_channels[i].ch_channel);
               
                strcpy(all_channels[i].ch_channel, all_channels[i+1].ch_channel);
                strcpy(temp.ch_channel, all_channels[i+1].ch_channel);
               
                //printf("~~~~~~~~~~~~~~~~~~~~~~DELETE~~~~~~~~~~~\n");
            }
        }
    }
}

/*
the judegaAddrEqual function code for comparing socket address cites online: 
http://www.opensource.apple.com/source/postfix/postfix-197/postfix/src/util/sock_addr.c
*/
bool judgeAddrEqual(const struct sockaddr *addr1, const struct sockaddr *addr2) {
    if (addr1 == NULL || addr2 == NULL)
        return addr1 == addr2;
    else if (addr1->sa_family != addr2->sa_family)
        return false;
    else if (addr1->sa_family == AF_INET) {
        struct sockaddr_in *ipv4Addr1 = (struct sockaddr_in *) addr1;
        struct sockaddr_in *ipv4Addr2 = (struct sockaddr_in *) addr2;
        return ipv4Addr1->sin_addr.s_addr == ipv4Addr2->sin_addr.s_addr
        && ipv4Addr1->sin_port == ipv4Addr2->sin_port;
    } else if (addr1->sa_family == AF_INET6) {
        struct sockaddr_in6 *ipv6Addr1 = (struct sockaddr_in6 *) addr1;
        struct sockaddr_in6 *ipv6Addr2 = (struct sockaddr_in6 *) addr2;
        return memcmp(&ipv6Addr1->sin6_addr, &ipv6Addr2->sin6_addr,
                      sizeof(struct in6_addr)) == 0 && ipv6Addr1->sin6_port 
        == ipv6Addr2->sin6_port;
    } else
        return false;
}

void ErrorSend(char ERROR_INF[SAY_MAX], int ssokcet, struct sockaddr_storage clientAddr) {
	struct text_error send_error;
	size_t text_error_size = sizeof(send_error); 
	send_error.txt_type = TXT_ERROR;
	strcpy(send_error.txt_error, ERROR_INF);
		if(sendto(ssokcet, &send_error, text_error_size, 0,
			(struct sockaddr *)&clientAddr, sizeof(clientAddr)) < 0)
			perror("Send Error Inf Failed");
}

/*check whether channel exists*/
bool checkChannel(struct channel_info channels[CHANNEL_NUM], struct channel_info waitChannel, int index) {
	int i = 0;
	bool channelMark = false;
	for(; i < index; i++) {
		if(!strcmp(channels[i].ch_channel, waitChannel.ch_channel)) {
			channelMark = true;
			break;
		}
	}
	return channelMark;
}

/*record user inf and channel inf*/
int userNum = 0;
int channelNum = 0;
int channelNum_S2S = 0;
int currentUser_index = 0;
int currentTree_index = 0;
int id_index = 0;
int TESTID = 0;
/*initiate ssokcet*/
int ssokcet = 0;
/*stores channel list*/
struct channel_info channelTotal[CHANNEL_NUM];
struct channel_info channelS2S[CHANNEL_NUM];
struct channel_tree S2S_tree[CHANNEL_NUM];
int uidlist[ID_NUM];
/*local ip and port*/
char middleHost[HOST_MAX];
int portNum = 0;

char *otherServer[1000];
int otherNum[1000];

int timeGolbal = 0;

/*timer function*/
void on_alarm(int signal) {
	if (alarm_stop) return;
	else alarm(alarm_period);
	/*soft-state*/
	//printf("time!!!!!!!!!!!!!!!!!!!!!\n");
	int i =0;
	int sendto_i = 0;
	int mark_i = 0;
	if(channelNum_S2S == 0) {
		timeGolbal = 0;
	}
	else
		timeGolbal++;
	/*local channel tree mark ++*/
	for(i=0; i<channelNum_S2S; i++) {
		for(mark_i=0; mark_i<S2S_tree[i].serverNum; mark_i++) {
			S2S_tree[i].timerMark[mark_i]++;
		}
	}

	/*chech whether need to delete*/
	for(i=0; i<channelNum_S2S; i++) {
		for(mark_i=0; mark_i<S2S_tree[i].serverNum; mark_i++) {
			if(S2S_tree[i].timerMark[mark_i] >= 12) {
				char host_name[32];
			    struct hostent *hostname;
			    struct in_addr **addr_list;
			    if ((hostname = gethostbyname(S2S_tree[i].serverInf[mark_i])) == NULL){
			        diep("gethostbyname");
			    }
			    addr_list = (struct in_addr **)hostname->h_addr_list;
			    strcpy(host_name, inet_ntoa(*addr_list[0]));		
		    
				printf("%s:%d delete %s:%d due to the 2 minutes timeout\n", 
					middleHost, portNum, host_name, S2S_tree[i].portNum[mark_i]);
				if ((mark_i+1) == S2S_tree[i].serverNum){
					S2S_tree[i].serverNum--;
				}
				else{
					delete_channel_and_port(i, mark_i, S2S_tree);
				}
			}
		}
	}

	/*send Join to all of ites neigbor*/
	if(timeGolbal >= 6) {
		for(i=0; i<channelNum_S2S; i++) {
			struct S2S_join testJoin;
			size_t testJoin_size = sizeof(testJoin);
			testJoin.req_type = REQ_S2S_JOIN;
			strcpy(testJoin.req_channel, S2S_tree[i].channelName.ch_channel);

		    for(sendto_i=0; sendto_i<S2S_tree[i].serverNum; sendto_i++) {	                        	
				char host_name[32];
			    struct hostent *hostname;
			    struct in_addr **addr_list;
			    if ((hostname = gethostbyname(otherServer[sendto_i])) == NULL){
			        diep("gethostbyname");
			    }
			    addr_list = (struct in_addr **)hostname->h_addr_list;
			    strcpy(host_name, inet_ntoa(*addr_list[0]));
			    
			    /*send test join S2S */	   
		    	struct sockaddr_in other_socket;
		    	memset((char *) &other_socket, 0, sizeof(other_socket));
			    other_socket.sin_family = AF_INET;
			    other_socket.sin_port = htons(otherNum[sendto_i]);

			    if (inet_aton(host_name, &other_socket.sin_addr)==0) {
			    	diep("inet_aton");
			    }
			    size_t si_other_socket = sizeof(other_socket);

			    sendto(ssokcet, &testJoin, testJoin_size , 0, (struct sockaddr*)&other_socket, si_other_socket);
			    printf("%s:%d %s:%d send S2S Test Join %s\n", middleHost, portNum, host_name, 
			    	otherNum[sendto_i], S2S_tree[i].channelName.ch_channel);          	                        	                        	
			}			
		}
		timeGolbal = 0;
	}		
}

int main(int argc, char *argv[]) {
	int some = argc%2;
	if (some == 0){
		printf("Usage: %s <Host address> <Port number>...\n",argv[0]);
		exit(0);
	}
	int input_i = 3;
	int other_index = 0;
	char *server = argv[1];
	portNum = atoi(argv[2]);
	/*
	char *otherServer[1000];
	int otherNum[1000];*/
	while(input_i+2 <= argc) {
		otherServer[other_index] = argv[input_i];
		input_i++;
		otherNum[other_index] = atoi(argv[input_i]);
		other_index++;
		input_i++;
	}
	
	/*build socketAddress structure*/
	struct sockaddr_in serveradd; 
	struct hostent *hostname;
	struct in_addr **ip_address;
	//char middleHost[HOST_MAX];
	/*define udp connection*/
	memset((char *)&serveradd, 0, sizeof(serveradd));

	serveradd.sin_family = AF_INET;
	serveradd.sin_port = htons(portNum);
	
	hostname = gethostbyname(server);
	if(hostname == NULL) {
		perror("IP Not Found");
    	exit(0);
	}
	ip_address = (struct in_addr **)hostname -> h_addr_list;
	strcpy(middleHost, inet_ntoa(*ip_address[0]));

	if(inet_aton(middleHost , &serveradd.sin_addr) == 0) {
		perror("Store IP Failed");
		exit(0);
	}
	
	/*bulid socket*/ 
	ssokcet = socket(AF_INET, SOCK_DGRAM, 0);
	if (ssokcet < 0) {
		perror("Create Socket Failed");
		exit(0);
	}

	/*record user inf and channel inf
	int userNum = 0;
	int channelNum = 0;
	int channelNum_S2S = 0;
	int currentUser_index = 0;
	int currentTree_index = 0;
	int id_index = 0;
	int TESTID = 0;*/

	/*stores incoming client sockets list*/
	struct socket_clients socketTotal[USER_NUM];
	struct socket_clients client_socket;
	client_socket.socket = ssokcet;
	client_socket.userChannelNum = 0;
	/*stores channel list
	struct channel_info channelTotal[CHANNEL_NUM];
	struct channel_info channelS2S[CHANNEL_NUM];
	struct channel_tree S2S_tree[CHANNEL_NUM];
	int uidlist[ID_NUM];*/
	
	/*set time seed*/
	time_t t;
    srand ((unsigned)time (&t));
	/*bind the connection*/
	if (bind(ssokcet, (struct sockaddr *)&serveradd, sizeof(serveradd)) < 0) 
		perror("Bind Failed");

	fd_set fds;
    struct timeval timeout;
    /*set time out starter*/
   	signal(SIGALRM, on_alarm);
  	alarm(alarm_period);

	while(1) {	

        FD_ZERO(&fds);
        FD_SET(ssokcet, &fds);
        int max = ssokcet + 1;

        //Set time limit
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;

        select(max, &fds, NULL, NULL, &timeout);

        if(FD_ISSET(ssokcet,&fds)) {
	        /*error mark*/
			bool ERROR_ID = false;
			char ERROR_INF[SAY_MAX];
			/*bulid socktaddr storeage for client*/
			struct sockaddr_storage clientAddr;
			socklen_t clientAddr_size = sizeof(clientAddr);
			client_socket.clientAddr = clientAddr;

			struct request recvRequest;
			size_t recv_size = sizeof(recvRequest);

			if(recv(ssokcet, &recvRequest, recv_size, MSG_PEEK) < 0) 
				perror("Receive Failed");
			else {
				/* Login Check */
				if(recvRequest.req_type == REQ_LOGIN) {

					struct request_login server_login;
					size_t login_size = sizeof(server_login);
					if(recvfrom(ssokcet, &server_login, login_size, 0, 
						(struct sockaddr *)&clientAddr, &clientAddr_size) < 0)	
						perror("Receive Login Failed");	
					//printf("%d\n", clientAddr.userChannelNum);	
					else {
						/*check whether username exceed limitaion*/
						if(strlen(server_login.req_username) > USERNAME_MAX) {
							strcpy(ERROR_INF, "Username Maximum Size is 32\n");
							ErrorSend(ERROR_INF, ssokcet, clientAddr);
						}
						else{
							/*get source ip and portnum*/
							struct sockaddr_in *coming_addr = (struct sockaddr_in *)&clientAddr;
							int ipAddr = coming_addr->sin_addr.s_addr;
							int coming_port = ntohs(coming_addr->sin_port);
							char coming_ip[HOST_MAX];
							inet_ntop( AF_INET, &ipAddr, coming_ip, INET_ADDRSTRLEN );
							/*find out whether the user's address stores still in server*/
							bool testAddress = false;
							int si = 0;
							for(si=0; si<userNum; si++) {
								struct sockaddr_storage testLoginAddr = socketTotal[si].clientAddr;
								if(judgeAddrEqual((struct sockaddr *)&testLoginAddr, (struct sockaddr *)&clientAddr)) {
									currentUser_index = si;
									testAddress = true;
									break;
								}
							}
							/*find out whether requseted username repeats*/
							int existedName_Firstindex = 0;
							int existedName_number = 48;
							bool testName = false;
							for(si=0; si<userNum; si++) {
								if(!strcmp(socketTotal[si].userName, server_login.req_username)) {
									existedName_Firstindex = si;
									testName = true;
									//existedName_number++;
								}
							}

							/*if address is not existed*/
							if(!testAddress) {
								/*if name is not used*/
								if(!testName) {
									/*If they have the same name as another user, just replace the other user with the new one. 
									This means that commands from the old user will no longer be valid because the server won't see them as being logged in.*/
									strcpy(client_socket.userName, server_login.req_username);
									client_socket.clientAddr = clientAddr;
									/*add this user to record list*/
									socketTotal[userNum] = client_socket;
									userNum++;
									printf("%s:%d %s:%d recv Request login %s\n", middleHost, portNum, 
										coming_ip, coming_port,client_socket.userName);
								}
								else {
									strcpy(client_socket.userName, server_login.req_username);
									client_socket.clientAddr = clientAddr;
									/*add this user to record list*/
									socketTotal[currentUser_index] = client_socket;
									//printf("%d\n", socketTotal[tem_userNum].userChannelNum);	
									//userNum++			
									printf("%s:%d %s:%d recv Request login %s(new one)\n", middleHost, portNum, 
										coming_ip, coming_port,client_socket.userName);				
									//printf("server: %s(new one) login\n", client_socket.userName);
									printf("server: %s(old one) forced logged out\n", client_socket.userName);

									/* modified */
									int ui=0;
									int i=0;
									int ci=0;
									bool check_channle_exist = false;
									int delet_index = 0;
									/*list stores waiting deleting channles*/
									struct channel_info channelDeleting[CHANNEL_NUM];
									for(i=0; i<channelNum; i++) {
										for(ui=0; ui<userNum; ui++) {
											for(ci=0; ci<socketTotal[ui].userChannelNum; ci++) {
												if(!strcmp(channelTotal[i].ch_channel, socketTotal[ui].joinChannels[ci].ch_channel)) {
													check_channle_exist = true;
												}
											}
										}
										if(!check_channle_exist) {
											channelDeleting[delet_index] = channelTotal[i];
											delet_index++;
											printf("server: noboday in channel %s, delete it\n", channelTotal[i].ch_channel);
											
										}
										check_channle_exist = false;
									}
									/* delete the non-existed channels*/
									for(i=0; i<delet_index+1; i++) {
										deleteChannel(channelNum, channelTotal, channelDeleting[i].ch_channel);
									}
									channelNum = channelNum - delet_index;
								}
							}
							/*if address is existed*/
							else {
								if(!testName) {
									strcpy(client_socket.userName, server_login.req_username);
									client_socket.clientAddr = clientAddr;
									/*add this user to record list*/
									socketTotal[userNum] = client_socket;
									userNum++;
									printf("%s:%d %s:%d recv Request login %s\n", middleHost, portNum, 
										coming_ip, coming_port, client_socket.userName);
									//printf("server: %s login\n", client_socket.userName);
									}
								else {
									strcpy(client_socket.userName, server_login.req_username);
									client_socket.clientAddr = clientAddr;
									/*add this user to record list*/
									socketTotal[currentUser_index] = client_socket;
									//socketTotal[currentUser_index].userChannelNum = 0;
									printf("%s:%d %s:%d recv Request login %s\n", middleHost, portNum, 
										coming_ip, coming_port,client_socket.userName);
									//printf("server: %s login again\n", client_socket.userName);
								}
							}
							/*strcpy(client_socket.userName, server_login.req_username);
							client_socket.clientAddr = clientAddr;
							socketTotal[userNum] = client_socket;
							userNum++;
							printf("server: %s has login\n", client_socket.userName);*/
						}
					}
				}
				
				/* Logout Down*/
				else if(recvRequest.req_type == REQ_LOGOUT) {
					
					struct request_logout user_logout;
					size_t logout_size = sizeof(user_logout);
					if (recvfrom(ssokcet, &user_logout, logout_size, 0, 
						(struct sockaddr *)&clientAddr, &clientAddr_size) < 0) {
						perror("Logout Failed");
					}
					
					else {
						/*get source ip and portnum*/
						struct sockaddr_in *coming_addr = (struct sockaddr_in *)&clientAddr;
						int ipAddr = coming_addr->sin_addr.s_addr;
						int coming_port = ntohs(coming_addr->sin_port);
						char coming_ip[HOST_MAX];
						inet_ntop( AF_INET, &ipAddr, coming_ip, INET_ADDRSTRLEN );

						bool haha5 = false;
						int o = 0;
						for(; o<userNum; o++) {
							struct sockaddr_storage haslogin = socketTotal[o].clientAddr;
							if(judgeAddrEqual((struct sockaddr *)&haslogin, (struct sockaddr *)&clientAddr)) {
								haha5 = true;
								break;
							}
						}
						/*look up to who is going to logout*/
						if (haha5){
							int si = 0;
							for(; si<userNum; si++) {
								struct sockaddr_storage logoutAddr = socketTotal[si].clientAddr;
								if(judgeAddrEqual((struct sockaddr *)&logoutAddr, (struct sockaddr *)&clientAddr)) {
									currentUser_index = si;
									break;
								}
							}
							char userName_print[USERNAME_MAX];
							strcpy(userName_print, socketTotal[currentUser_index].userName);
							/*delete the user from socket list*/
							int ui = currentUser_index;
							int userNum_tem = userNum;
							/*judge whether user number has reach the max to avoid overflow*/
							if (userNum == USER_NUM) 
								userNum_tem--;
							/*delete action*/
							for(; ui<userNum_tem; ui++) {
								socketTotal[ui] = socketTotal[ui+1];
							}
							userNum--;
							printf("%s:%d %s:%d recv Request logout %s\n", middleHost, portNum, 
										coming_ip, coming_port,client_socket.userName);
							//printf("server: %s logout\n", userName_print);
							int i=0;
							int ci=0;
							bool check_channle_exist = false;
							int delet_index = 0;
							/*list stores waiting deleting channles*/
							struct channel_info channelDeleting[CHANNEL_NUM];
							for(i=0; i<channelNum; i++) {
								for(ui=0; ui<userNum; ui++) {
									for(ci=0; ci<socketTotal[ui].userChannelNum; ci++) {
										if(!strcmp(channelTotal[i].ch_channel, socketTotal[ui].joinChannels[ci].ch_channel)) {
											check_channle_exist = true;
										}
									}
								}
								if(!check_channle_exist) {
									channelDeleting[delet_index] = channelTotal[i];
									delet_index++;
									printf("server: noboday in channel %s, delete it\n", channelTotal[i].ch_channel);
									
								}
								check_channle_exist = false;
							}
							/* delete the non-existed channels*/
							for(i=0; i<delet_index+1; i++) {
								deleteChannel(channelNum, channelTotal, channelDeleting[i].ch_channel);
							}
							channelNum = channelNum - delet_index;
						}
					}	
				}

				/* Join Down */
				else if(recvRequest.req_type == REQ_JOIN) {
					
					struct request_join server_join;
					size_t join_size = sizeof(server_join);
					if(recvfrom(ssokcet, &server_join, join_size, 0,
						(struct sockaddr *)&clientAddr, &clientAddr_size) < 0)
						perror("Receive Failed");
					else {
						/*get source ip and portnum*/
						struct sockaddr_in *coming_addr = (struct sockaddr_in *)&clientAddr;
						int ipAddr = coming_addr->sin_addr.s_addr;
						int coming_port = ntohs(coming_addr->sin_port);
						char coming_ip[HOST_MAX];
						inet_ntop( AF_INET, &ipAddr, coming_ip, INET_ADDRSTRLEN );

						bool haha4 = false;
						int o = 0;
						for(; o<userNum; o++) {
							struct sockaddr_storage haslogin = socketTotal[o].clientAddr;
							if(judgeAddrEqual((struct sockaddr *)&haslogin, (struct sockaddr *)&clientAddr)) {
								haha4 = true;
								break;
							}
						}
						/*check whether channel length exceed limitaion*/
						if(strlen(server_join.req_channel) > USERNAME_MAX) {
							strcpy(ERROR_INF, "Channel Maximum Size is 32\n");
							ErrorSend(ERROR_INF, ssokcet, clientAddr);
						}
						else{
							if (haha4){
								struct channel_info req_join_channel;
								strcpy(req_join_channel.ch_channel, server_join.req_channel);
								/* look up the user who is going to join the channel*/
								int si = 0;
								for(si = 0; si<userNum; si++) {
									struct sockaddr_storage joinSocket = socketTotal[si].clientAddr;
									if(judgeAddrEqual((struct sockaddr *)&clientAddr, (struct sockaddr *)&joinSocket)) {
										currentUser_index = si;
										break;
									}
								}
								/*check whether this user has joined request channel*/
								int userChannelNum_tem = socketTotal[currentUser_index].userChannelNum;
								bool mark_server = checkChannel(channelTotal, req_join_channel, channelNum);
								bool mark_user = checkChannel(socketTotal[currentUser_index].joinChannels, req_join_channel, userChannelNum_tem);
								
								if(mark_server) {
									if(mark_user) {
										strcpy(ERROR_INF, "Request Channel Already join");
										ErrorSend(ERROR_INF, ssokcet, clientAddr);
									}
									else {
										int tar_index = socketTotal[currentUser_index].userChannelNum;
										socketTotal[currentUser_index].joinChannels[tar_index] = req_join_channel;
										socketTotal[currentUser_index].userChannelNum = tar_index+1;
										printf("%s:%d %s:%d recv Request join %s %s\n", middleHost, portNum,
											coming_ip, coming_port, socketTotal[currentUser_index].userName, req_join_channel.ch_channel);
									}
								}
								else {
									/*check whether S has joined requested channel tree*/
									bool server_s2s = false;
									int tree_index = 0;
									for(; tree_index<channelNum_S2S; tree_index++) {
										if(!strcmp(S2S_tree[tree_index].channelName.ch_channel, req_join_channel.ch_channel)) {
											server_s2s = true;
											break;
										}
									}
									/* S join channel on local*/	
									channelTotal[channelNum] = req_join_channel;									
									int tar_index = socketTotal[currentUser_index].userChannelNum;
									socketTotal[currentUser_index].joinChannels[tar_index] = req_join_channel;
									socketTotal[currentUser_index].userChannelNum = tar_index+1;
									channelNum++;
									
									printf("%s:%d %s:%d recv Request join %s %s\n", middleHost, portNum, coming_ip, coming_port,
										socketTotal[currentUser_index].userName, req_join_channel.ch_channel);
									
									/* S join S2S tree */									
									if(!server_s2s) {										
										S2S_tree[channelNum_S2S].channelName = req_join_channel;
										S2S_tree[channelNum_S2S].serverNum = 0;	
										int join_i = 0;																			
										for(; join_i<other_index; join_i++) {
											S2S_tree[channelNum_S2S].portNum[join_i] = otherNum[join_i];
											S2S_tree[channelNum_S2S].serverInf[join_i] = otherServer[join_i];
											S2S_tree[channelNum_S2S].timerMark[join_i] = 0;
											//printf("!!!!!!!!!!!tree: %s: %d\n", S2S_tree[channelNum_S2S].serverInf[join_i], S2S_tree[channelNum_S2S].portNum[join_i]);
										}									
										
										/* S2S Join request send*/
				                        struct S2S_join new_S2S_join;
				                        new_S2S_join.req_type = REQ_S2S_JOIN;
				                        strcpy(new_S2S_join.req_channel, server_join.req_channel);
				                        size_t S2SjoinSize = sizeof(new_S2S_join);
				                        int sendto_i = 0;
				                        for(; sendto_i<other_index; sendto_i++) {
				                        	char host_name[32];
										    struct hostent *hostname;
										    struct in_addr **addr_list;
										    if ((hostname = gethostbyname(otherServer[sendto_i])) == NULL){
										        diep("gethostbyname");
										    }
										    addr_list = (struct in_addr **)hostname->h_addr_list;
										    strcpy(host_name, inet_ntoa(*addr_list[0]));

				                        	struct sockaddr_in other_socket;
				                        	memset((char *) &other_socket, 0, sizeof(other_socket));
										    other_socket.sin_family = AF_INET;
										    other_socket.sin_port = htons(otherNum[sendto_i]);

										    if (inet_aton(host_name, &other_socket.sin_addr)==0) {
										    	diep("inet_aton");
										    }
										    size_t si_other_socket = sizeof(other_socket);

										    sendto(ssokcet, &new_S2S_join, S2SjoinSize , 0, (struct sockaddr*)&other_socket, si_other_socket);
										    printf("%s:%d %s:%d send S2S Join %s\n", middleHost, portNum, host_name, 
										    	otherNum[sendto_i], req_join_channel.ch_channel);
										    S2S_tree[channelNum_S2S].serverNum++;										    
				                        }
				                        channelS2S[channelNum_S2S] = req_join_channel;
										channelNum_S2S++;
									}					                       
								}
							}
						}		
					}
				}

				/* List Down */
				else if(recvRequest.req_type == REQ_LIST) {
					struct request_list recieve_list;
					size_t list_size = sizeof(recieve_list);
					if(recvfrom(ssokcet, &recieve_list, list_size, 0,
						(struct sockaddr *)&clientAddr, &clientAddr_size) < 0)
						perror("Receive List Failed");
					else {	
						bool haha = false;
						int o = 0;
						for(; o<userNum; o++) {
							struct sockaddr_storage haslogin = socketTotal[o].clientAddr;
							if(judgeAddrEqual((struct sockaddr *)&haslogin, (struct sockaddr *)&clientAddr)) {
								haha = true;
								break;
							}
						}
						if (haha){
												/*look up to who is going to lists*/
							int si = 0;
							for(; si<userNum; si++) {
								struct sockaddr_storage listAddr = socketTotal[si].clientAddr;
								if(judgeAddrEqual((struct sockaddr *)&listAddr, (struct sockaddr *)&clientAddr)) {
									currentUser_index = si;
									break;
								}
							}
							char userName_print[USERNAME_MAX];
							strcpy(userName_print, socketTotal[currentUser_index].userName);
							struct text_list send_list;
							size_t send_list_size = sizeof(send_list);
							send_list.txt_type = TXT_LIST;
							send_list.txt_nchannels = channelNum;
							/*add all existed channel*/
							int ci = 0;
							for(; ci < channelNum; ci++)
								send_list.txt_channels[ci] = channelTotal[ci];
							/*send back to client*/
							if(sendto(ssokcet, &send_list, send_list_size, 0,
								(struct sockaddr *)&clientAddr, sizeof(clientAddr)) < 0)
								perror("Send List Failed");
							else 
								printf("server: %s lists channels\n", socketTotal[currentUser_index].userName);
						}
					}
				}

				/* Who Down */
				else if(recvRequest.req_type == REQ_WHO) {
					
					struct request_who recieve_who;
					size_t who_size = sizeof(recieve_who);
					
					if(recvfrom(ssokcet, &recieve_who, who_size, 0,
						(struct sockaddr *)&clientAddr, &clientAddr_size) < 0)
						perror("Receive Who Request Failed");
					else{
						bool haha1 = false;
						int o = 0;
						for(; o<userNum; o++) {
							struct sockaddr_storage haslogin = socketTotal[o].clientAddr;
							if(judgeAddrEqual((struct sockaddr *)&haslogin, (struct sockaddr *)&clientAddr)) {
								haha1 = true;
								break;
							}
						}
						/*look up to who is going to lists who list*/
						if (haha1){
							int si = 0;
							for(; si<userNum; si++) {
								struct sockaddr_storage whoAddr = socketTotal[si].clientAddr;
								if(judgeAddrEqual((struct sockaddr *)&whoAddr, (struct sockaddr *)&clientAddr)) {
									currentUser_index = si;
									break;
								}
							}
							struct text_who send_who;
							send_who.txt_type = TXT_WHO;
							size_t send_who_size = sizeof(send_who);
							/* Judge whether the request channel is exists*/
							struct channel_info waitChannel;
							strcpy(waitChannel.ch_channel, recieve_who.req_channel);
							bool mark_channle = checkChannel(channelTotal, waitChannel, channelNum);
							if(mark_channle) {
								/* Look for people who are in the request channel*/
								int ui = 0;
								int ci = 0;
								int index_userInchannel = 0;
								/* add joined users into list */
								for(ui=0; ui<userNum; ui++) {
									for(ci=0 ; ci<socketTotal[ui].userChannelNum; ci++) {
										if(!strcmp(recieve_who.req_channel, socketTotal[ui].joinChannels[ci].ch_channel)) {
											struct user_info joinedUser;
											strcpy(joinedUser.us_username, socketTotal[ui].userName);
											send_who.txt_users[index_userInchannel] = joinedUser;
											index_userInchannel++;
										}
									}
								}
								send_who.txt_nusernames = index_userInchannel;
								send_who_size = sizeof(send_who);
								if(sendto(ssokcet, &send_who, send_who_size, 0,
									(struct sockaddr *)&clientAddr, sizeof(clientAddr)) < 0)
									perror("Send Who List Failed");
								else 
									printf("server: %s lists users in channel %s\n", socketTotal[currentUser_index].userName, recieve_who.req_channel);
							}
							else {
								strcpy(ERROR_INF, "cannot find requested channel in server");
								ErrorSend(ERROR_INF, ssokcet, clientAddr);
							}
						}					
					}
				}

				/* Leave Down*/
				else if(recvRequest.req_type == REQ_LEAVE) {
					struct request_leave recive_leave;
					size_t leave_size = sizeof(recive_leave);
					if(recvfrom(ssokcet, &recive_leave, leave_size, 0,
						(struct sockaddr *)&clientAddr, &clientAddr_size) < 0)
						perror("Receive Who Request Failed");
					else{
						bool haha2 = false;
						int o = 0;
						for(; o<userNum; o++) {
							struct sockaddr_storage haslogin = socketTotal[o].clientAddr;
							if(judgeAddrEqual((struct sockaddr *)&haslogin, (struct sockaddr *)&clientAddr)) {
								haha2 = true;
								break;
							}
						}
						/*find the user who going to leave channel*/
						if (haha2){
							int ui = 0;
							for(; ui<userNum; ui++) {
								struct sockaddr_storage leaveSocket = socketTotal[ui].clientAddr;
								if(judgeAddrEqual((struct sockaddr *)&clientAddr, (struct sockaddr *)&leaveSocket)) {
									currentUser_index = ui;
									break;
								}
							}
							/*find leaving channel index in user channel list*/
							int ci = 0;
							int indexOfchannel_user = 0;
							for(; ci<socketTotal[currentUser_index].userChannelNum; ci++) {
								if(!strcmp(recive_leave.req_channel, socketTotal[currentUser_index].joinChannels[ci].ch_channel)) {
									indexOfchannel_user = ci;
									break;
								}
		 					}
		 					/*int tem_userChannelNum = socketTotal[currentUser_index].userChannelNum;
		 					socketTotal[currentUser_index].userChannelNum = tem_userChannelNum - 1;*/
		 					/*delete action on users channel*/
		 					int cci = 0;
		 					int limit = socketTotal[currentUser_index].userChannelNum - 1;
							for(cci=indexOfchannel_user; cci<limit; cci++) {
								strcpy(socketTotal[currentUser_index].joinChannels[cci].ch_channel, socketTotal[currentUser_index].joinChannels[cci+1].ch_channel);
							}
							socketTotal[currentUser_index].userChannelNum = limit;
							
		 					printf("server: %s leaves channel %s\n", socketTotal[currentUser_index].userName, recive_leave.req_channel);
		 					/*check if any user in required channel*/
		 					int uui = 0;
		 					bool mark_has_user = false;
		 					for(uui=0; uui<userNum; uui++) {
		 						if(ifjoinedchannel(socketTotal[uui].userChannelNum, socketTotal[uui].joinChannels, recive_leave.req_channel)) {
		 							mark_has_user = true;
									//printf("find channel no one in it!~~~~~~~~~~~~~~~\n");
		 						}
		 						/*
								for(cci=0; cci<socketTotal[uui].userChannelNum; cci++) {

									if(!strcmp(recive_leave.req_channel, socketTotal[uui].joinChannels[cci].ch_channel)) {
										mark_has_user = true;
										printf("find channel no one in it!~~~~~~~~~~~~~~~\n");
										break;
									}
								}*/
							}
							/* noboday in that channel, delete channle from server channel list*/
							if(!mark_has_user) {
								deleteChannel(channelNum, channelTotal, recive_leave.req_channel);
								channelNum--;
								printf("server: noboday in channel %s, delete it\n", recive_leave.req_channel);
							}
						}
					}
				}
				
				/* Say Down */
				else if(recvRequest.req_type == REQ_SAY) {
					
					struct request_say user_say;
					size_t say_size = sizeof(user_say);
					if(recvfrom(ssokcet, &user_say, say_size, 0,
						(struct sockaddr *)&clientAddr, &clientAddr_size) < 0)
						perror("Receive Message Failed");
					else {
						/*get source ip and portnum*/
						struct sockaddr_in *coming_addr = (struct sockaddr_in *)&clientAddr;
						int ipAddr = coming_addr->sin_addr.s_addr;
						int coming_port = ntohs(coming_addr->sin_port);
						char coming_ip[HOST_MAX];
						inet_ntop( AF_INET, &ipAddr, coming_ip, INET_ADDRSTRLEN );

						bool haha3 = false;
						int o = 0;
						for(; o<userNum; o++) {
							struct sockaddr_storage haslogin = socketTotal[o].clientAddr;
							if(judgeAddrEqual((struct sockaddr *)&haslogin, (struct sockaddr *)&clientAddr)) {
								haha3 = true;
								break;
							}
						}
						if (haha3){
							/*check whether saying length exceed limitaion*/
							if(strlen(user_say.req_text) > SAY_MAX) {
								strcpy(ERROR_INF, "Message Maximum Size is 64\n");
								ErrorSend(ERROR_INF, ssokcet, clientAddr);
							}
							else{

								/*local deliver*/

								struct text_say text_send;
								size_t text_send_size = sizeof(text_send);
								text_send.txt_type = TXT_SAY;

								int si = 0;
								/*find the speaking user*/
								for(; si<userNum; si++) {
									struct sockaddr_storage saySocket = socketTotal[si].clientAddr;
									if(judgeAddrEqual((struct sockaddr *)&clientAddr, (struct sockaddr *)&saySocket)) {
										currentUser_index = si;
										break;
									}
								}
								bool if_in_channel = false;
								int user_ci = 0;
								for(; user_ci<socketTotal[currentUser_index].userChannelNum; user_ci++) {
									if(!strcmp(user_say.req_channel, socketTotal[currentUser_index].joinChannels[user_ci].ch_channel)) {
										if_in_channel = true;
										break;
									}
								}

								if(if_in_channel) {
									strcpy(text_send.txt_channel, user_say.req_channel);
									strcpy(text_send.txt_username, socketTotal[currentUser_index].userName);
									strcpy(text_send.txt_text, user_say.req_text);
									int ui = 0;
									int ci = 0;
									/*send message to anyone on the channel*/
									for(; ui<userNum; ui++) {
										for(ci=0; ci<socketTotal[ui].userChannelNum; ci++) {
											if(!strcmp(user_say.req_channel, socketTotal[ui].joinChannels[ci].ch_channel)) {
												struct sockaddr_storage tem_clientAddr = socketTotal[ui].clientAddr;
												if(sendto(ssokcet, &text_send, text_send_size, 0,
													(struct sockaddr *)&tem_clientAddr, sizeof(tem_clientAddr)) < 0)
													perror("Send Message Failed");
											}
										}
									}

									printf("%s:%d %s:%d recv Request say %s %s \" %s \" \n", middleHost, portNum, 
										coming_ip, coming_port, socketTotal[currentUser_index].userName, user_say.req_channel, user_say.req_text);

									/*S2S deliver*/	

									struct S2S_say s2s_say;
									size_t s2s_say_size = sizeof(s2s_say);
									s2s_say.req_type = REQ_S2S_SAY;
									strcpy(s2s_say.req_channel, user_say.req_channel);
									strcpy(s2s_say.req_text, user_say.req_text);
									strcpy(s2s_say.username, socketTotal[currentUser_index].userName);
									/*random UID*/
									int random_id = 0;
									//srand (time (NULL));
									random_id = rand()%ID_RANGE;
									s2s_say.id = random_id;
									/*find the request channel tree*/
									int tree_i = 0;
									for(; tree_i<channelNum_S2S; tree_i++) {
										if(!strcmp(S2S_tree[tree_i].channelName.ch_channel, user_say.req_channel)) {
											currentTree_index = tree_i;
											break;
										}
									}
									/*deliver say message to all server in channle tree*/
									int sendto_i = 0;
									int tree_number = S2S_tree[currentTree_index].serverNum;	
									for(; sendto_i<tree_number; sendto_i++) {
			                        	char host_name[32];
									    struct hostent *hostname;
									    struct in_addr **addr_list;
									    if ((hostname = gethostbyname(S2S_tree[currentTree_index].serverInf[sendto_i])) == NULL){
									        diep("gethostbyname");
									    }
									    addr_list = (struct in_addr **)hostname->h_addr_list;
									    strcpy(host_name, inet_ntoa(*addr_list[0]));

			                        	struct sockaddr_in other_socket;
			                        	memset((char *) &other_socket, 0, sizeof(other_socket));
									    other_socket.sin_family = AF_INET;
									    other_socket.sin_port = htons(S2S_tree[currentTree_index].portNum[sendto_i]);

									    if (inet_aton(host_name, &other_socket.sin_addr)==0) {
									    	diep("inet_aton");
									    }
									    size_t si_other_socket = sizeof(other_socket);

									    sendto(ssokcet, &s2s_say, s2s_say_size , 0, (struct sockaddr*)&other_socket, si_other_socket);
									    printf("%s:%d %s:%d send S2S say %s %s \"%s\" \n", middleHost, portNum, host_name, 
									    	S2S_tree[currentTree_index].portNum[sendto_i], s2s_say.username, 
									    	s2s_say.req_channel, s2s_say.req_text);										    
			                        }	
								}
								else {
									strcpy(ERROR_INF, "you are not in the requested channel");
									ErrorSend(ERROR_INF, ssokcet, clientAddr);
									printf("server: %s cannot send message in unjoined channel %s\n", socketTotal[currentUser_index].userName, user_say.req_channel);
								}								
							}
						}
					}
				}

				/*S2S Join*/
				else if(recvRequest.req_type == REQ_S2S_JOIN) {
					struct S2S_join s2s_join;
					size_t s2s_join_size = sizeof(s2s_join);
					if(recvfrom(ssokcet, &s2s_join, s2s_join_size, 0,
						(struct sockaddr *)&clientAddr, &clientAddr_size) < 0)
						perror("Receive Failed");
					else {
						struct channel_info req_s2s_channel;
						strcpy(req_s2s_channel.ch_channel, s2s_join.req_channel);

						/*check whether S has joined requested channel tree*/
						bool mark_server_S2S = false;
						int tree_index = 0;
						for(; tree_index<channelNum_S2S; tree_index++) {
							if(!strcmp(S2S_tree[tree_index].channelName.ch_channel, req_s2s_channel.ch_channel)) {
								mark_server_S2S = true;
								currentTree_index = tree_index;
								break;
							}
						}
						/*get the source ip address and port number*/
						struct sockaddr_in *coming_addr = (struct sockaddr_in *)&clientAddr;
						int ipAddr = coming_addr->sin_addr.s_addr;
						int coming_port = ntohs(coming_addr->sin_port);
						char coming_ip[HOST_MAX];
						inet_ntop( AF_INET, &ipAddr, coming_ip, INET_ADDRSTRLEN );
						printf("%s:%d %s:%d recv S2S Join %s\n", middleHost, portNum, coming_ip, coming_port, req_s2s_channel.ch_channel);
						
						/*case 1: local has the channle tree*/
						if(mark_server_S2S) {
							printf("%s: %d has already in request channel tree: %s\n", middleHost, portNum, s2s_join.req_channel);
							/*check whether the localtree include the coming server!!!*/
							bool mark_localhasIP = false;
							int treeI = 0;
							for(; treeI<S2S_tree[currentTree_index].serverNum; treeI++) {
								/*get local record server ip address*/
								char i_host_name[32];
							    struct hostent *i_hostname;
							    struct in_addr **i_addr_list;
							    if ((i_hostname = gethostbyname(S2S_tree[currentTree_index].serverInf[treeI])) == NULL){
							        diep("gethostbyname");
							    }
							    i_addr_list = (struct in_addr **)i_hostname->h_addr_list;
							    strcpy(i_host_name, inet_ntoa(*i_addr_list[0]));
							 	/*judge progress*/
								if(!strcmp(i_host_name, coming_ip) && S2S_tree[currentTree_index].portNum[treeI] == coming_port) {
									mark_localhasIP = true;
									break;
								}
							}
							/*if local tree not contains coming ip, add it in*/
							if(!mark_localhasIP) {
								int this_length = S2S_tree[currentTree_index].serverNum;
								S2S_tree[currentTree_index].portNum[this_length] = coming_port;
								strcpy(S2S_tree[currentTree_index].serverInf[this_length], coming_ip);
								S2S_tree[currentTree_index].serverNum++;
							}
						}

						/*case 2: local did not has channel tree*/
						else {
							/*add the coming request channle to channelS2S*/
							S2S_tree[channelNum_S2S].channelName = req_s2s_channel;								
							S2S_tree[channelNum_S2S].serverNum = 0;	
							int join_i = 0;									
							for(join_i=0; join_i<other_index; join_i++) {
								S2S_tree[channelNum_S2S].portNum[join_i] = otherNum[join_i];
								S2S_tree[channelNum_S2S].serverInf[join_i] = otherServer[join_i];
								S2S_tree[channelNum_S2S].timerMark[join_i] = 0;
							}														
							/* S2S Join request forwards*/	                        
	                        int sendto_i = 0;
	                        for(; sendto_i<other_index; sendto_i++) {	                        	
                        		char host_name[32];
							    struct hostent *hostname;
							    struct in_addr **addr_list;
							    if ((hostname = gethostbyname(otherServer[sendto_i])) == NULL){
							        diep("gethostbyname");
							    }
							    addr_list = (struct in_addr **)hostname->h_addr_list;
							    strcpy(host_name, inet_ntoa(*addr_list[0]));
							    
							    /*judge whether repeating */							    
							    if(!strcmp(host_name, coming_ip) && otherNum[sendto_i] == coming_port) {
							    	S2S_tree[channelNum_S2S].serverNum++;					
							    }
							    else {
							    	struct sockaddr_in other_socket;
		                        	memset((char *) &other_socket, 0, sizeof(other_socket));
								    other_socket.sin_family = AF_INET;
								    other_socket.sin_port = htons(otherNum[sendto_i]);

								    if (inet_aton(host_name, &other_socket.sin_addr)==0) {
								    	diep("inet_aton");
								    }
								    size_t si_other_socket = sizeof(other_socket);

								    sendto(ssokcet, &s2s_join, s2s_join_size , 0, (struct sockaddr*)&other_socket, si_other_socket);
								    printf("%s:%d %s:%d send S2S Join %s\n", middleHost, portNum, host_name, otherNum[sendto_i], req_s2s_channel.ch_channel);
							    	S2S_tree[channelNum_S2S].serverNum++;
							    }		                        	                        	                        	
							}
							channelS2S[channelNum_S2S] = req_s2s_channel;
							channelTotal[channelNum] = req_s2s_channel;
							channelNum++;
							channelNum_S2S++;							
						}
						int channel_i = 0;
						int mark_i = 0;
						for(; channel_i<channelNum_S2S; channel_i++) {
							if(!strcmp(S2S_tree[channel_i].channelName.ch_channel, s2s_join.req_channel)) {
								for(; mark_i<S2S_tree[channel_i].serverNum; mark_i++) {
									char i_host_name[32];
								    struct hostent *i_hostname;
								    struct in_addr **i_addr_list;
								    if ((i_hostname = gethostbyname(S2S_tree[channel_i].serverInf[mark_i])) == NULL){
								        diep("gethostbyname");
								    }
								    i_addr_list = (struct in_addr **)i_hostname->h_addr_list;
								    strcpy(i_host_name, inet_ntoa(*i_addr_list[0]));
								 	/*judge progress*/
									if(!strcmp(i_host_name, coming_ip) && S2S_tree[channel_i].portNum[mark_i] == coming_port) {
										S2S_tree[channel_i].timerMark[mark_i] = 0;
										break;
									}	
								}									
							}
						}	
					}
				}

				/*S2S Say*/
				else if(recvRequest.req_type == REQ_S2S_SAY) {
					struct S2S_say rec_s2s_say;
					size_t say_size = sizeof(rec_s2s_say);
					if(recvfrom(ssokcet, &rec_s2s_say, say_size, 0,
						(struct sockaddr *)&clientAddr, &clientAddr_size) < 0)
						perror("Receive Failed");
					else {
						
						bool ifduplicate = false; 
						bool ifnoclient = true;
						bool ifisleaf = false;

						/*check if id is duplicate*/
						int i = 0;
						for(; i<id_index; i++) {
							if(uidlist[i] == rec_s2s_say.id) {
								ifduplicate = true;
								break;
							}
						}
						
						/*find the channel_tree index*/
						int tree_index = 0;
						for(; tree_index<channelNum_S2S; tree_index++) {
							if(!strcmp(S2S_tree[tree_index].channelName.ch_channel, rec_s2s_say.req_channel)) {
								currentTree_index = tree_index;
								break;
							}
						}

						/*check if no client to forward*/						
						int ui = 0;
						int ci = 0;
						for(; ui<userNum; ui++) {
							for(; ci<socketTotal[ui].userChannelNum; ci++) {
								if(!strcmp(rec_s2s_say.req_channel, socketTotal[ui].joinChannels[ci].ch_channel)) {
									ifnoclient = false;
									break; 
								}
							}
						}

						/*chech if it is a leaf of the tree*/
						if(S2S_tree[currentTree_index].serverNum < 2) 
							ifisleaf = true;

						/*get source ip and port number*/
						struct sockaddr_in *coming_addr = (struct sockaddr_in *)&clientAddr;
						int ipAddr = coming_addr->sin_addr.s_addr;
						int coming_port = ntohs(coming_addr->sin_port);
						char coming_ip[HOST_MAX];
						inet_ntop( AF_INET, &ipAddr, coming_ip, INET_ADDRSTRLEN );
						
						printf("%s:%d %s:%d recv Request say %s %s \" %s \" \n", middleHost, portNum, 
								coming_ip, coming_port, rec_s2s_say.username, rec_s2s_say.req_channel, rec_s2s_say.req_text);

						/*if send leave meassge*/					
						//printf("ifduplicate: %d, ifnoclient: %d, ifisleaf: %d\n", ifduplicate, ifnoclient, ifisleaf);
	
						if(ifduplicate || (ifnoclient && ifisleaf)) {
							struct S2S_leave s2s_leave;
							s2s_leave.req_type = REQ_S2S_LEAVE;
							strcpy(s2s_leave.req_channel, rec_s2s_say.req_channel);
							size_t s2s_leave_size = sizeof(s2s_leave);
							if(sendto(ssokcet, &s2s_leave, s2s_leave_size, 0,
									(struct sockaddr *)&clientAddr, sizeof(clientAddr)) < 0)
									perror("Send S2S_leave Failed");
							else 
								printf("%s:%d %s:%d send S2S Leave %s\n", middleHost, portNum, 
									coming_ip, coming_port, s2s_leave.req_channel);
							
							/*delete the tree from itself if it has no client and is a leaf*/							
							if(ifnoclient && ifisleaf) {
								int tem_i = 0;
								for(; tem_i<channelNum_S2S; tem_i++) {
									if(!strcmp(S2S_tree[tem_i].channelName.ch_channel, s2s_leave.req_channel)) {
										if((tem_i+1) == channelNum_S2S) 
											channelNum_S2S--;										
										else {
											int ii = tem_i;
											for(;ii<channelNum_S2S; ii++) {												
												S2S_tree[ii].channelName = S2S_tree[ii+1].channelName;
												S2S_tree[ii].serverNum = S2S_tree[ii+1].serverNum;
												int kii = 0;
												for(; kii<S2S_tree[ii+1].serverNum; kii++) {
													S2S_tree[ii].serverInf[kii] = S2S_tree[ii+1].serverInf[kii];
													S2S_tree[ii].portNum[kii] = S2S_tree[ii+1].portNum[kii];
													S2S_tree[ii].timerMark[kii] = S2S_tree[ii+1].timerMark[kii];
												}												
											}
											channelNum_S2S--;
										}
										break;
									}
								}
							}
						}

						/*if forward message to client and server*/
						else {
							uidlist[id_index] = rec_s2s_say.id;
							id_index++;
							/*local deliver*/
							struct text_say text_send;
							size_t text_send_size = sizeof(text_send);
							text_send.txt_type = TXT_SAY;
							strcpy(text_send.txt_channel, rec_s2s_say.req_channel);
							strcpy(text_send.txt_username, rec_s2s_say.username);
							strcpy(text_send.txt_text, rec_s2s_say.req_text);
							int ui = 0;
							int ci = 0;
							/*send message to anyone on the channel*/
							for(ui=0; ui<userNum; ui++) {
								for(ci=0; ci<socketTotal[ui].userChannelNum; ci++) {
									if(!strcmp(rec_s2s_say.req_channel, socketTotal[ui].joinChannels[ci].ch_channel)) {
										struct sockaddr_storage tem_clientAddr = socketTotal[ui].clientAddr;
										if(sendto(ssokcet, &text_send, text_send_size, 0,
											(struct sockaddr *)&tem_clientAddr, sizeof(tem_clientAddr)) < 0)
											perror("Send Message Failed");
									}
								}
							}
						
							/*S2S deliver*/
							int tree_i = 0;
							for(; tree_i<channelNum_S2S; tree_i++) {
								if(!strcmp(S2S_tree[tree_i].channelName.ch_channel, rec_s2s_say.req_channel)) {
									currentTree_index = tree_i;
									break;
								}
							}
							/*deliver say message to all other server in channle tree except coming server*/
							int sendto_i = 0;
							int tree_number = S2S_tree[currentTree_index].serverNum;	
							for(; sendto_i<tree_number; sendto_i++) {
	                        	char host_name[32];
							    struct hostent *hostname;
							    struct in_addr **addr_list;
							    if ((hostname = gethostbyname(S2S_tree[currentTree_index].serverInf[sendto_i])) == NULL){
							        diep("gethostbyname");
							    }
							    addr_list = (struct in_addr **)hostname->h_addr_list;
							    strcpy(host_name, inet_ntoa(*addr_list[0]));

						    	if(!strcmp(host_name, coming_ip) && S2S_tree[currentTree_index].portNum[sendto_i] == coming_port) {
						    		//printf("Say no need send\n");
							    }
							    else {
							    	struct sockaddr_in other_socket;
		                        	memset((char *) &other_socket, 0, sizeof(other_socket));
								    other_socket.sin_family = AF_INET;
								    other_socket.sin_port = htons(S2S_tree[currentTree_index].portNum[sendto_i]);

								    if (inet_aton(host_name, &other_socket.sin_addr)==0) {
								    	diep("inet_aton");
								    }
								    size_t si_other_socket = sizeof(other_socket);

								    sendto(ssokcet, &rec_s2s_say, say_size , 0, (struct sockaddr*)&other_socket, si_other_socket);
								    printf("%s:%d %s:%d send S2S say %s %s \"%s\" \n", middleHost, portNum, host_name, 
								    	S2S_tree[currentTree_index].portNum[sendto_i], rec_s2s_say.username, 
								    	rec_s2s_say.req_channel, rec_s2s_say.req_text);		
							    }
							}								    	   
						} 
					}
				} 

				/*S2S Leave*/
				else if(recvRequest.req_type == REQ_S2S_LEAVE) {
					
					struct S2S_leave s2s_leave;
					size_t s2s_leave_size = sizeof(s2s_leave);
					if(recvfrom(ssokcet, &s2s_leave, s2s_leave_size, 0,
						(struct sockaddr *)&clientAddr, &clientAddr_size) < 0)
						perror("Receive Failed");
					else {
						struct sockaddr_in *coming_addr = (struct sockaddr_in *)&clientAddr;
						int ipAddr = coming_addr->sin_addr.s_addr;
						int coming_port = ntohs(coming_addr->sin_port);
						char coming_ip[HOST_MAX];
						inet_ntop( AF_INET, &ipAddr, coming_ip, INET_ADDRSTRLEN );
						printf("%s:%d %s:%d recv S2S Leave %s\n",middleHost, portNum, coming_ip, coming_port, s2s_leave.req_channel);
						// check if this channel in channel list
						// check if server in that channel if in delete it
						int i, j;
						for (i = 0; i < channelNum_S2S; i++){
							if (strcmp(S2S_tree[i].channelName.ch_channel, s2s_leave.req_channel) == 0){
								for (j = 0; j < S2S_tree[i].serverNum; j++){
									/*transfer ip name to ip address*/
									char host_name_tem[32];
								    struct hostent *hostname_tem;
								    struct in_addr **addr_list_tem;
								    if ((hostname_tem = gethostbyname(S2S_tree[i].serverInf[j])) == NULL) {
								        diep("gethostbyname");
								    }
								    addr_list_tem = (struct in_addr **)hostname_tem->h_addr_list;
							   	 	strcpy(host_name_tem, inet_ntoa(*addr_list_tem[0]));
									

									if ((S2S_tree[i].portNum[j] == coming_port) && !(strcmp(coming_ip, host_name_tem))) {
										if ((j+1) == S2S_tree[i].serverNum){
											S2S_tree[i].serverNum = S2S_tree[i].serverNum - 1;

										}
										else{
											delete_channel_and_port(i, j, S2S_tree);
										}
									}
								}
							}
						}
					}
				}

				/* if type is null, Error happen*/
				else {
					struct text_error send_error;
					size_t text_error_size = sizeof(send_error); 
					send_error.txt_type = TXT_ERROR;
					strcpy(ERROR_INF, "Unknown Request Type");
					strcpy(send_error.txt_error, ERROR_INF);
						if(sendto(ssokcet, &send_error, text_error_size, 0,
							(struct sockaddr *)&clientAddr, sizeof(clientAddr)) < 0)
							perror("Send Error Inf Failed");
				}	
			}
        }
	}
	close(ssokcet);
	return 0;
}