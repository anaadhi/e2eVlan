#include <stun_utils.h>

int parse_2d_array(const char* str, struct sockaddr_in* result_array){
    char ip[20];
    int iplen = 0;
    char port[10];
    int portlen = 0;
    int mode = 0;
    int cnt = 0;
    for(int i = 1; str[i] != '\0'; i++){
        char curr = str[i];
        if(mode == 0 && curr == '"'){
            mode = 1;
            continue;
        }
        else if(mode == 1 && curr == '"'){
            ip[iplen++] = '\0';
            mode = 2;
            continue;
        }
        else if(mode == 2 && curr == ']'){
            cnt++;
            port[portlen++] = '\0';
            printf("got ip address %s\n", ip);
            printf("got port %d\n", atoi(port));
            iplen = 0;
            portlen = 0;
            mode = 0;
            continue;
        }

        if(mode == 1){
            ip[iplen++] = curr;
        } 
        if(mode == 2 && curr >='0' && curr <= '9'){
            port[portlen++] = curr;
        }
    }

    return cnt;
}

void pingAll(int sockfd, struct sockaddr_in* peers, int npeers){
    const char data[1] = "";
    for(int i = 0; i < npeers;i++){
        sendto(sockfd, data, sizeof(data), 0, (const struct sockaddr *)&peers[i], sizeof(peers[i]));
        sleep(0.1);
    }
}

void stun_keep_alive(int sockfd, struct sockaddr_in* peers,int npeers){
    while(1){
        printf("Sent keep alive");
        pingAll(sockfd, peers, npeers);
        sleep(10);
    }
}


int stun_init(char* stun_server_ip, int stun_server_port, char* result){

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if(sockfd < 0)
        return -1;


    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    if(inet_pton(AF_INET, stun_server_ip, &server_addr.sin_addr) != 1){
        perror("Invalid ip address\n");
        exit(1);
    }
    server_addr.sin_port = htons(stun_server_port);
    server_addr.sin_family = AF_INET;
    
    struct sockaddr_in srcaddr;
    memset(&srcaddr, 0, sizeof(srcaddr));
    srcaddr.sin_family = AF_INET;
    srcaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    srcaddr.sin_port = htons(SRC_PORT);

    if(bind(sockfd, (struct sockaddr *) &srcaddr, sizeof(srcaddr)) < 0){
        perror("bind");
        exit(1);
    }

    // send connection status to server
    char init[20] = "hello world!";
    ssize_t sentsz = sendto(sockfd, init, sizeof(init), 0, (const struct sockaddr *)&server_addr, sizeof(server_addr));


    int addrlen = sizeof(server_addr);
    int recvsz = recvfrom(sockfd, result, sizeof(result), 0, (struct sockaddr *)&server_addr, &addrlen);

    printf("received %s\n", result);
    

    return sockfd;
}