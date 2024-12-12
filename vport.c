#include "tap_utils.h"
#include "stun_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <pthread.h>

typedef struct vport_t{
    int tapfd;
    int sockfd;
    struct sockaddr_in server_addr;
} vport_t;

typedef struct mapv_t {
    uint8_t etherd[ETH_ALEN];
    struct sockaddr_in dest;
} mapv_t;

mapv_t map[10];
int maplen = 0;
uint8_t broadcast_addr[ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};


int vport_init(vport_t *vport, char* server_addr, int server_port);
void *forward_data_to_switch(void* vport_raw);
void *forward_data_to_tap(void* vport_raw);




int addToMap(const struct ether_header *hdr,struct sockaddr_in dest){
    for(int i = 0; i < maplen; i++){
        int flag = 1;
        for(int j = 0; j < ETH_ALEN; j++){
            if(hdr->ether_shost[j] != map[i].etherd[j]){
                flag = 0;
                break;
            }
                
        }
        if(flag){
            return 1;
        }
    }

    
    for(int j = 0; j < ETH_ALEN; j++){
        map[maplen].etherd[j] = hdr->ether_shost[j];
    }
    map[maplen].dest = dest;
    maplen++;
    return 0;
}

struct sockaddr_in getAddressFromMap(const struct ether_header *hdr){
    

    for(int i = 0; i < maplen; i++){
        int flag = 1;
        for(int j = 0; j < ETH_ALEN; j++){
            if(hdr->ether_dhost[j] != map[i].etherd[j]){
                flag = 0;
                break;
            }
                
        }
        if(flag){
            return map[i].dest;
        }
    }

    return map[0].dest;
}

int main(int argc,const char *argv[])
{
    
    if(argc < 3){
        printf("Usage ./vport {server_ip} {server_port}\n");
        return -1;
    }
    char server_ip[INET_ADDRSTRLEN];
    strcpy(server_ip, argv[1]);
    int server_port = atoi(argv[2]);

    struct sockaddr_in peers[20];
    char peersstring[100];

    printf("server ip %s server port %d \n",server_ip, server_port);

    int sockfd = stun_init(server_ip, server_port, peersstring);
    int total_peers = parse_2d_array(peersstring, peers);
    

    // open connection
    printf("total peers %d\n", total_peers);
    for(int i = 0; i < 3; i++){
        pingAll(sockfd, peers, total_peers);
    }
    
    
    vport_t vport;
    vport.sockfd = sockfd;
    if(vport_init(&vport, server_ip, server_port) < 0)
        return -1;


    
    pthread_t up_forwarder;
    if(pthread_create(&up_forwarder, NULL,forward_data_to_tap,&vport) != 0){
        printf("Unable to create thread 1\n");
        return -1;
    }

    pthread_t down_forwarder;
    if(pthread_create(&down_forwarder, NULL, forward_data_to_switch, &vport) != 0){
        printf("Unable to create thread 2\n");
        return -1;
    }

    if(pthread_join(up_forwarder, NULL) != 0 || pthread_join(down_forwarder, NULL) != 0 ){
        printf("Unable to join thread\n");
        return -1;
    }

    
    return 0;
}

void *forward_data_to_tap(void* vport_raw){
    vport_t *vport = (vport_t *) vport_raw;
    char ether_data[ETHER_MAX_LEN];
    struct sockaddr_in src_addr;
    while(1){
        socklen_t switch_addr_len = sizeof(src_addr);
        int ether_data_sz = recvfrom(vport->sockfd,ether_data,sizeof(ether_data), 0, (struct sockaddr *)&src_addr,&switch_addr_len);
        

        if(ether_data_sz > 0){
            if(ether_data_sz < 14){
                printf("Received keep alive\n");
                continue;
            }
            const struct ether_header *hdr = (const struct ether_header *) ether_data;
            ssize_t sendsz = write(vport->tapfd, ether_data, ether_data_sz);
            if(sendsz != ether_data_sz){
                printf("Unable to write data\n");
            }

            

            printf("[VPort] Forward to TAP device:"
             " dhost<%02x:%02x:%02x:%02x:%02x:%02x>"
             " shost<%02x:%02x:%02x:%02x:%02x:%02x>"
             " type<%04x>"
             " datasz=<%d>\n",
             hdr->ether_dhost[0], hdr->ether_dhost[1], hdr->ether_dhost[2], hdr->ether_dhost[3], hdr->ether_dhost[4], hdr->ether_dhost[5],
             hdr->ether_shost[0], hdr->ether_shost[1], hdr->ether_shost[2], hdr->ether_shost[3], hdr->ether_shost[4], hdr->ether_shost[5],
             ntohs(hdr->ether_type),
             ether_data_sz);
        }
    }

}

void broadcast_data(char *etherdata, int ether_data_sz, int sockfd){

    for(int i = 0 ; i < maplen; i++){
        struct sockaddr_in dest = map[i].dest;
        ssize_t sentsz = sendto(sockfd, etherdata, ether_data_sz, 0, (struct sockaddr *)&dest, sizeof(dest));
    }
}

void *forward_data_to_switch(void *vport_raw){
    vport_t *vport = (vport_t *) vport_raw;
    char etherdata[ETHER_MAX_LEN];

    while(1){
        int ether_data_sz = read(vport->tapfd, etherdata, sizeof(etherdata));
        printf("read ethernet data of size %d\n", ether_data_sz);
        if(ether_data_sz > 0){
            
            assert(ether_data_sz >= 14);
            const struct ether_header *hdr = (const struct ether_header *) etherdata;
            

            if (memcmp(hdr->ether_dhost, broadcast_addr, ETH_ALEN) == 0) {
                broadcast_data(etherdata, ether_data_sz, vport->sockfd);
            } else {
                struct sockaddr_in dest = getAddressFromMap(hdr);
                ssize_t sentsz = sendto(vport->sockfd, etherdata, ether_data_sz, 0, (struct sockaddr *)&dest, sizeof(dest));
                if(sentsz != ether_data_sz){
                    printf("Unable to send data to server\n");
                    // return;
                }
            }

            printf("[VPort] Sent to VSwitch:"
             " dhost<%02x:%02x:%02x:%02x:%02x:%02x>"
             " shost<%02x:%02x:%02x:%02x:%02x:%02x>"
             " type<%04x>"
             " datasz=<%d>\n",
             hdr->ether_dhost[0], hdr->ether_dhost[1], hdr->ether_dhost[2], hdr->ether_dhost[3], hdr->ether_dhost[4], hdr->ether_dhost[5],
             hdr->ether_shost[0], hdr->ether_shost[1], hdr->ether_shost[2], hdr->ether_shost[3], hdr->ether_shost[4], hdr->ether_shost[5],
             ntohs(hdr->ether_type),
             ether_data_sz);
        }
    }
}

int vport_init(vport_t *vport, char* server_addr, int server_port){
    char adapter[IFNAMSIZ] = "tap0";
    int fd = tap_alloc(adapter);

    if(fd < 0){
        printf("Unable to allocate a tap device\n");
        return -1;
    }
    vport->tapfd = fd;

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(server_port);
    if(inet_pton(AF_INET, server_addr, &server.sin_addr) != 1){
        printf("Invalid ip address\n");
        return -1;
    }

    vport->server_addr = server;

    return 0;

}
