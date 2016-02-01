/*
Client Program
--------------

- Does the client compile and run? yes

- Do login and logout work? yes

- Does the client accept user commands? yes

Please specify which of the following features work

    - Join: works

    - Leave: works

    - Say: works

    - Switch: works

    - List: works

    - Who: works


- Does the client send Keep Alive message when needed (for extra credit)? no

- Does the client send Keep Alive message when not needed (for extra credit)? no

- Can the client handle messages that are out of order(e.g. /leave before a /join)? yes/no

- Can the client redisplay the prompt and user input when a message from the server is received while the user is typing (for extra credit)? yes
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "raw.h"
#include "raw.c"
typedef int bool;
#define true 1
#define false 0
#include "duckchat.h"


void joinchannel(int number_channel_joins, struct channel_info all_channels[CHANNEL_MAX], char channel_name[CHANNEL_MAX]){
    strcpy(all_channels[number_channel_joins].ch_channel, channel_name);
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

void deleteChannel(int number_channels, struct channel_info all_channels[CHANNEL_MAX], char channel_name[CHANNEL_MAX]){
    int i;
    struct channel_info temp;
    strcpy(temp.ch_channel, channel_name);
    if (number_channels != 1){
        for (i = 0 ; i < (number_channels-1) ; i ++){
            if (strcmp(temp.ch_channel, all_channels[i].ch_channel) == 0){
                strcpy(all_channels[i].ch_channel, all_channels[i+1].ch_channel);
                strcpy(temp.ch_channel, all_channels[i+1].ch_channel);
            }
        }
    }
}

//when error happend
void diep(char *s)
{
    perror(s);
    exit(1);
}

//back spaces
#define BACKSPACES 64
int counter;
void printBackspaces(int n)
{
    int pos = 0;
    for ( ; pos < n; ++ pos)
        putchar('\b');
}


int main(int argc, char *argv[]){
    // all request type

    char* req_join = "/join";
    char* req_leave = "/leave";
    char* req_logout = "/exit";
    char* req_list = "/list";
    char* req_who = "/who";
    char* req_switch = "/switch";
    char Userchannel[32] = "Common";
    // check arguement
	if (argc != 4){
		printf("Usage: %s <Host name> <Port number> <Username>\n",argv[0]);
		exit(0);
	}
    if (strlen(argv[3]) > 32 ){
        diep("Username Maximum Size is 32\n");
    }
	int port_number = atoi(argv[2]);
	char* username = argv[3];

    // get host name and convert to IP address
    char host_name[32];
    struct hostent *hostname;
    struct in_addr **addr_list;
    if ((hostname = gethostbyname(argv[1])) == NULL){
        diep("gethostbyname");
    }
    addr_list = (struct in_addr **)hostname->h_addr_list;
    strcpy(host_name, inet_ntoa(*addr_list[0]));


	// create socket and set up socket
	int s;
	struct sockaddr_in si_socket;
	if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
        diep("socket");
	memset((char *) &si_socket, 0, sizeof(si_socket));
    si_socket.sin_family = AF_INET;
    si_socket.sin_port = htons(port_number);
    if (inet_aton(host_name, &si_socket.sin_addr)==0) {
    	diep("inet_aton");
    }
    size_t si_socket_Size = sizeof(si_socket);

    bool Not_Exit = true;

    //login
    struct request_login new_login_struct;
    new_login_struct.req_type = REQ_LOGIN;
    strcpy(new_login_struct.req_username, username);
    size_t loginSize = sizeof(new_login_struct);
    sendto(s, &new_login_struct, loginSize , 0, (struct sockaddr*)&si_socket, si_socket_Size);
    
    //join common
    struct request_join new_joincommon_struct;
    new_joincommon_struct.req_type = REQ_JOIN;
    strcpy(new_joincommon_struct.req_channel, Userchannel);
    size_t joinSize = sizeof(new_joincommon_struct);
    sendto(s, &new_joincommon_struct, joinSize , 0, (struct sockaddr*)&si_socket, si_socket_Size);

    // receive struct
    struct sockaddr_storage fromAddr;
    socklen_t fromAddrLen = sizeof(fromAddr);


    //build readfds
    fd_set fds;
    struct timeval timeout;

    // 3 instruct
    char instruct[32];
    char instruct_name[32];


    // store all channel joined
    struct channel_info all_channel_joined[CHANNEL_MAX];
    strcpy(all_channel_joined[0].ch_channel, Userchannel);
    int number_channel_joined = 1;
    
    
    // sent socket and receive socket while not exit
    while (Not_Exit){
        char buf[64];
        char *p = buf;
        // Create a descriptor set containing two sockets
        FD_ZERO(&fds);
        FD_SET(s, &fds);
        FD_SET(0, &fds);
        int max = s + 1;

         //Set time limit
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;

        raw_mode();
        select(max, &fds, NULL, NULL, &timeout);
        if (FD_ISSET(s,&fds)){
            //receive response
            struct text checkType;
            size_t checkTypeSize = sizeof(checkType);
            ssize_t numBytes = recv(s, &checkType, checkTypeSize,MSG_PEEK);
            if (numBytes < 0){
                diep("rece() failed");
            }
              
            else if ((size_t)numBytes != checkTypeSize){
                diep("recv() erro ,received more than a response number of bytes");
            }
            if (checkType.txt_type == TXT_LIST){
                struct text_list listResp;
                size_t listRespSize = sizeof(listResp);
                recvfrom(s, &listResp, listRespSize, 0, (struct sockaddr *) &fromAddr, &fromAddrLen);      
                int i;
                printBackspaces(BACKSPACES);
                printf("Existing channels:\n");
                for (i = 0; i < listResp.txt_nchannels; ++i) {
                    struct channel_info channels = listResp.txt_channels[i];
                    printf("\t%s\n", channels.ch_channel);
                }
            }
           
            else if (checkType.txt_type == TXT_SAY){
                struct text_say sayResp;
                strcpy(sayResp.txt_channel, Userchannel);
                size_t sayRespSize = sizeof(sayResp);
                recvfrom(s, &sayResp, sayRespSize, 0, (struct sockaddr *) &fromAddr, &fromAddrLen);
                printBackspaces(BACKSPACES);
                printf("[%s][%s]: %s\n", sayResp.txt_channel, sayResp.txt_username, sayResp.txt_text);
            }

            else if (checkType.txt_type == TXT_WHO){
                struct text_who whoResp;
                size_t whoRespSize = sizeof(whoResp);
                recvfrom(s,&whoResp,whoRespSize,0, (struct sockaddr *) &fromAddr, &fromAddrLen);
                printBackspaces(BACKSPACES);
                printf("Users on channel %s:\n", whoResp.txt_channel);
                for (counter = 0; counter < whoResp.txt_nusernames; ++counter){
                    struct user_info user = whoResp.txt_users[counter];
                    printf(" %s\n", user.us_username);
                }
            }
            else if (checkType.txt_type == TXT_ERROR){
                struct text_error errorResp;
                size_t errorRespSize = sizeof(errorResp);
                recvfrom(s, &errorResp, errorRespSize, 0, (struct sockaddr *) &fromAddr, &fromAddrLen);
                printBackspaces(BACKSPACES);
                printf("Error From Server: %s\n", errorResp.txt_error);
            }
            fflush(stdout);
        }
        cooked_mode();
        if (FD_ISSET(0,&fds)){
            //clean in and out then get next line
            fflush(stdin);
            fflush(stdout);
            putchar('>');
            
            fgets(buf, sizeof(buf), stdin);
            // get command and channel name  or  text message
            // saved in char instruct and instruct_name
            int index_line = 0;
            if (*p != '/'){
                if (number_channel_joined != 0){
                    struct request_say userSpeak;
                    userSpeak.req_type = REQ_SAY;
                    strcpy(userSpeak.req_channel,Userchannel);
                    
                    int buf_i = 0;
                    char buf_send[64];
                    memset(buf_send, '\0', sizeof(buf_send));
                    strcpy(buf_send, buf);
                    for(; buf_i<64; buf_i++) {
                        if(buf_send[buf_i] == '\0')
                            buf_send[buf_i-1] = '\0';
                    } 
                    
                    strcpy(userSpeak.req_text, buf_send);
                    size_t saySize = sizeof(userSpeak);
                    sendto(s, &userSpeak, saySize , 0, (struct sockaddr*)&si_socket, si_socket_Size);
                }
                else{
                    printf("You have not joined any channel yet\n");
                }
            }
            else{
                while(*p != '\0'){
                    char word[32] = {0};
                    int i = 0;
                    while(*p != ' ' && *p != '\0' &&  *p != '\n'){
                      if(i == sizeof(word) - 1)
                         break;
                      word[i++] = *p;
                      p++;
                    }
                    word[i] = '\0';
                    p++;
                    if (index_line == 0){
                        strncpy(instruct, word, 32);
                        index_line++;
                    }
                    else if(index_line == 1){
                        strncpy(instruct_name, word, 32);
                        index_line = 0;
                    }
                }

                if (strcmp(instruct, req_join) == 0){
                    if (strlen(instruct_name) == 0){
                        printf("*Unknown command\n");
                    }
                    else if (ifjoinedchannel(number_channel_joined, all_channel_joined, instruct_name)){
                        printf("already joined channel %s\n", instruct_name);
                    }
                    else{
                        struct request_join new_join_struct;
                        new_join_struct.req_type = REQ_JOIN;
                        strcpy(new_join_struct.req_channel, instruct_name);
                        strcpy(Userchannel ,instruct_name);
                        size_t joinSize = sizeof(new_join_struct);
                        joinchannel(number_channel_joined, all_channel_joined, instruct_name);
                        number_channel_joined += 1;
                        sendto(s, &new_join_struct, joinSize , 0, (struct sockaddr*)&si_socket, si_socket_Size);
                    }
                }
                else if (strcmp(instruct, req_leave) == 0){
                    if (strlen(instruct_name) == 0){
                        printf("*Unknown command\n");
                    }
                    else if (ifjoinedchannel(number_channel_joined, all_channel_joined, instruct_name) == false){
                        printf ("you did not join channel %s\n", instruct_name);
                    }
                    else{
                        if (ifjoinedchannel(number_channel_joined, all_channel_joined, instruct_name)){
                            struct request_leave new_leave_struct;
                            new_leave_struct.req_type = REQ_LEAVE;
                            strcpy(new_leave_struct.req_channel, instruct_name);
                            size_t leaveSize = sizeof(new_leave_struct);
                            deleteChannel(number_channel_joined, all_channel_joined, instruct_name);
                            number_channel_joined = number_channel_joined - 1;
                            sendto(s, &new_leave_struct, leaveSize , 0, (struct sockaddr*)&si_socket, si_socket_Size);
                            if (number_channel_joined != 0){
                                strcpy(Userchannel, all_channel_joined[0].ch_channel);
                            }
                        }
                    }
                }
                else if (strcmp(instruct, req_list) == 0){
                    struct request_list new_list_struct;
                    new_list_struct.req_type = REQ_LIST;
                    size_t listSize = sizeof(new_list_struct);
                    sendto(s, &new_list_struct, listSize , 0, (struct sockaddr*)&si_socket,si_socket_Size);
                    
                }
                else if (strcmp(instruct, req_switch) == 0){
                    if (strlen(instruct_name) == 0){
                        printf("*Unknown command\n");
                    } 
                    else if (ifjoinedchannel(number_channel_joined, all_channel_joined, instruct_name)){
                        strcpy(Userchannel, instruct_name);
                    }
                    else{
                        printf("Not joined channel %s\n", instruct_name);
                    }
                }
                else if (strcmp(instruct, req_who) == 0){
                    if (strlen(instruct_name) == 0){
                        printf("*Unknown command\n");
                    }
                    else{
                        struct request_who new_who_struct;
                        new_who_struct.req_type = REQ_WHO;
                        strcpy(new_who_struct.req_channel, instruct_name);
                        size_t whoSize = sizeof(new_who_struct);
                        sendto(s, &new_who_struct, whoSize , 0, (struct sockaddr*)&si_socket, si_socket_Size);
                    }
                }
                else if (strcmp(instruct, req_logout) == 0){
                    struct request_logout new_logout_struct;
                    new_logout_struct.req_type = REQ_LOGOUT;
                    size_t logoutSize = sizeof(new_logout_struct);
                    sendto(s, &new_logout_struct, logoutSize , 0, (struct sockaddr*)&si_socket, si_socket_Size);
                    Not_Exit  = false;
                }
                else{
                    printf("*Unknown command\n");
                }
            }
        }
    }
    cooked_mode();
    close(s);

	return 0;
}