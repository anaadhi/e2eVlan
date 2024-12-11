#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>

#define SRC_PORT 50055

int parse_2d_array(const char* str, struct sockaddr_in* result_array);

int stun_init(char* stun_server_ip, int stun_server_port,  char* result);

void stun_keep_alive(int sockfd, struct sockaddr_in* peers,int npeers);

void pingAll(int sockfd, struct sockaddr_in* peers, int npeers);
