/* 
 * File:   main.c
 * Author: user
 *
 * Created on June 10, 2015, 4:03 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include "server.h"

/*
 * 

Onderstaand de betreffende registeradressen, de teller zit op input 8:
 
Input register adres I/O 
16384 I/O 1
16385 I/O 2
16386 I/O 3
16387 I/O 4
16388 I/O 5
16389 I/O 6
16390 I/O 7
16391 I/O 8
16392 CNT


Output register adres
17408 I/O 1
17409 I/O 2
17410 I/O 3
17411 I/O 4
17412 Reset CNT
 
password = password

wireshark files

char peer0_0[] = {
0x00, 0x01,  transaction id dit is nummer one
0x00, 0x00, protocol id 
0x00, 0x06, length 6 bytes
0x00, unit id 
  
0x04, function code read registers
0x40, 0x00, refernce number==register number 16384
0x00, 0x09  word count to read starting at register number
 
  };

 *  response from the TCP slave
 * char peer1_0[] = {
 * 0x00, 0x01, transaction id dit is one hoort bij vraag 1 
 * 0x00, 0x00, protocol id is altijd 0 
 * 0x00, 0x15, lengte van response is 21 bytes
 * 0x00, unit id 
 
 * 0x04, function code read registers
 * 0x12, byte count van response 0x12==18 bytes
 * 0x00, 0x00, register 0 waarde 
 * 0x00, 0x00, register 1
 * 0x00, 0x00, reg 2
 * 0x00, 0x00, reg 3
 * 0x00, 0x00, ..
 * 0x00, 0x00, ..
 * 0x00, 0x00, ..
 * 0x00, 0x01, register 7 waarde (input 8, is hoog)
 * 0x00, 0x05, register 8 waarde (teller waarde 5)
 * };
 
 * 
 *  dit is de reset van de teller
 * command van de master
char peer0_1[] = {
 * 0x00, 0x02, transaction 2
 * 0x00, 0x00, protocol id
 * 0x00, 0x06, lengte van bericht 6 bytes
 * 0x00, unit id 
 * 0x06, functino code write single register
0x44, 0x04, 0x00, 0x00 };
 *  response van de Turck
char peer1_1[] = {
0x00, 0x02, 0x00, 0x00, 0x00, 0x06, 0x00, 0x06, 
0x44, 0x04, 0x00, 0x00 };
 * 
 * 
 * 
char peer0_2[] = {
0x00, 0x03, 0x00, 0x00, 0x00, 0x17, 0x00, 0x10, 
0x44, 0x00, 0x00, 0x08, 0x10, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00 };
char peer1_2[] = {
0x00, 0x03, 0x00, 0x00, 0x00, 0x06, 0x00, 0x10, 
0x44, 0x00, 0x00, 0x08 };
char peer0_3[] = {
0x00, 0x04, 0x00, 0x00, 0x00, 0x06, 0x00, 0x04, 
0x40, 0x00, 0x00, 0x09 };
char peer1_3[] = {
0x00, 0x04, 0x00, 0x00, 0x00, 0x15, 0x00, 0x04, 
0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x01, 0x00, 0x05 };

 * 
 *  
 */

const int portno = 502;
const char modbus_host[] = {192, 168, 1, 2};

const char lookup[] = {0,};

uint16_t read_register(uint16_t reg) {
    return 0;
}

const char hostname[] = "192.168.1.2";
const char poort[] = "502";

int get_ip_address(const char *host, const char *service) {
    struct addrinfo hints, *res, *p;
    int status;
    char ipstr[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(hostname, poort, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 2;
    }

    printf("IP addresses for %s:\n\n", host);

    for (p = res; p != NULL; p = p->ai_next) {
        void *addr;
        char *ipver;

        // get the pointer to the address itself,
        // different fields in IPv4 and IPv6:
        if (p->ai_family == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *) p->ai_addr;
            addr = &(ipv4->sin_addr);
            ipver = "IPv4";
        } else { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *) p->ai_addr;
            addr = &(ipv6->sin6_addr);
            ipver = "IPv6";
        }

        // convert the IP to a string and print it:
        inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
        printf("  %s: %s\n", ipver, ipstr);
    }

    freeaddrinfo(res); // free the linked list
    return 0;
}

int connect_to_host(const char *host, const char *port) {

    struct addrinfo hints, *res;
    int sockfd;
    int rv;
    // first, load up address structs with getaddrinfo():

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(host, port, &hints, &res);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        rv = -1;
    } else {
        // make a socket:

        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockfd == -1) {
            rv = -2;
            goto cleanup;
        }
        // connect!

        int xrv = connect(sockfd, res->ai_addr, res->ai_addrlen);
        if (xrv == -1) {
            rv = -3;
            goto cleanup;
        } else {
            rv = sockfd; /* success return sockfd */
        }
    }

cleanup:

    freeaddrinfo(res);
    return rv; /* sockfd or error code */
}

char request[] = {
    0x00, 0x01, // transaction id dit is nummer one
    0x00, 0x00, // protocol id 
    0x00, 0x06, // length 6 bytes
    0x00, //unit id 

    0x04, //function code read registers
    0x40, 0x00, //refernce number==register number 16384
    0x00, 0x09 //word count to read starting at register number

};

int make_modbus_req(uint8_t *buf, int buflen, uint16_t transaction_id, uint8_t unit_id, uint16_t reg_id, uint16_t reg_count) {
    if (buflen > 12) {
        buf[0] = (char) (transaction_id & 0xFF00) >> 8;
        buf[1] = (char) (transaction_id & 0x00FF);
        // protocol
        buf[2] = 0;
        buf[3] = 0;
        // length of msg
        buf[4] = 0;
        buf[5] = 6;
        // unit id
        buf[6] = unit_id;
        // function code
        buf[7] = 4;
        // register id
        buf[8] =  (reg_id & 0xFF00) >> 8;
        buf[9] =  (reg_id & 0x00FF);
        // register count
        buf[10] =  (reg_count & 0xFF00) >> 8;
        buf[11] =  (reg_count & 0x00FF);

        return 12; // msg length
    } else {
        return -1;
    }
}

int parse_answer(uint8_t *buf, int buflen) {
    if (buflen == 27) {
        /*  response from the TCP slave
         * char peer1_0[] = {
         * 0x00, 0x01, transaction id dit is one hoort bij vraag 1 
         * 0x00, 0x00, protocol id is altijd 0 
         * 0x00, 0x15, lengte van response is 21 bytes
         * 0x00, unit id 
 
         * 0x04, function code read registers
         * 0x12, byte count van response 0x12==18 bytes
         * 0x00, 0x00, register 0 waarde 
         * 0x00, 0x00, register 1
         * 0x00, 0x00, reg 2
         * 0x00, 0x00, reg 3
         * 0x00, 0x00, ..
         * 0x00, 0x00, ..
         * 0x00, 0x00, ..
         * 0x00, 0x01, register 7 waarde (input 8, is hoog)
         * 0x00, 0x05, register 8 waarde (teller waarde 5)
         * }; */

        uint16_t trans_id = (buf[0] << 8) + buf[1];
        fprintf(stderr, "trans id = %d\n", trans_id);
        uint16_t protocol_id = (buf[2] << 8) + buf[3];
        fprintf(stderr, "protocol id = %d\n", protocol_id);
        uint16_t msg_len = (buf[4] << 8) + buf[5];
        fprintf(stderr, "msg_len = %d\n", msg_len);
        uint8_t unit_id = buf[6];
        fprintf(stderr, "unit id= %d\n", unit_id);
        uint8_t function_code = buf[7];
        fprintf(stderr, "func code = %d\n", function_code);
        uint16_t resp_len = buf[8];
        fprintf(stderr, "resp bytes = %d\n", resp_len);

        int j = 0;
        int i = 0;
        for (i = 0; i < resp_len; i += 2) {
            
            uint16_t regval = (*(buf + 9 + i) << 8) + *(buf + 10 + i);
            
            fprintf(stderr, "register %"PRIu16" = %"PRIu16"\n", j, regval);
            j++;
        }
        return 0;

    } else {
        fprintf(stderr, "ongeldige lengte van buffer\n");
        return -1;
    }

}

/* protocol_state_machine
 * 0 : wacht op data
 * 1 : we lezen URI, dat mag X seconden duren, stop bij de \r\n
 * 2 : we lezen een header regel... \r\n
 * 3 : we lezen de body
 *
 */


int main(int argc, char** argv) {

    run_server();
    
    
    
    int count = 1000;
    uint8_t buf[4096] = {0};
    uint8_t reply[4096] = {0};

    while (count) {

        int fd = connect_to_host(hostname, poort);
        if (fd > 0) {

            int req_size = make_modbus_req(buf, sizeof buf, 1, 0, 16384, 9);
            fprintf(stderr, "req size = %d\n", req_size);
            //int snd_cnt = send(fd, request, sizeof request, MSG_NOSIGNAL); // send do not signal
            //if (snd_cnt == sizeof request) {
            int snd_cnt = send(fd, buf, req_size, MSG_NOSIGNAL);
            if (snd_cnt == req_size) {
                // ok, now read reply
                int recv_cnt = read(fd, reply, 27);
                fprintf(stderr, "got %d bytes from modbus slave", recv_cnt);
                if (recv_cnt == 27) {
                    /* I want 27 bytes */
                    parse_answer(reply, recv_cnt);
                }
            }

            close(fd);
        } else {
            perror("connect failed\n");

        }
        sleep(1);
        count--;
    }
    return 0;
}

