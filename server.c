
/*
 ** selectserver.c -- a async server
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>

#include "server.h"

void * fd_to_machine_map[1024] = {0,};


// get sockaddr, IPv4 or IPv6:

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*) sa)->sin6_addr);
}

typedef struct {
    int current_state; /* state of the machine */
    struct timeval timestamp; /* buffer timestamping gettimeofday */
    int buffer_index; /* teller in de linebuffer */
    char buffer[512]; /* line buffer temp */
    char requesttype[32]; /* POST GET PUT etc */
    char URI[512]; /* langer is vast onzin, mag 32k zijn volgens spec */
    char HOST[512]; /* onzin maar zeker groot genoeg */
    char BODY[512]; /* body van de POST message komt hier */

} http_state_t;

int http_machine(http_state_t *machine,  int buflen) {
    /* the machine, uses timestamp to handle timeouts */

    return 0; /* all ok */
    return -1; /* error, break connection */
}

int run_server(void) {
    fd_set master; // master file descriptor list
    fd_set read_fds; // temp file descriptor list for select()
    int fdmax; // maximum file descriptor number

    int listener; // listening socket descriptor
    int newfd; // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;

    //char buf[256]; // buffer for client data
    //int nbytes;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes = 1; // for setsockopt() SO_REUSEADDR, below
    int i,  rv;

    struct addrinfo hints, *ai, *p;

    FD_ZERO(&master); // clear the master and temp sets
    FD_ZERO(&read_fds);

    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }

    for (p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }

        // lose "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // if we got here, it means we didn't get bound
    if (p == NULL) {
        fprintf(stderr, "selectserver: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    // main loop
    for (;;) {
        read_fds = master; // copy it

        struct timeval timeout = {.tv_sec = 1, .tv_usec = 0};

        int srv = select(fdmax + 1, &read_fds, NULL, NULL, &timeout);
        if (srv == -1) {
            perror("select");
            exit(4);
        } else if (srv == 0) {
            /* handle timeouts */
            fprintf(stderr, "timeout\n");
        } else {
            // run through the existing connections looking for data to read
            for (i = 0; i <= fdmax; i++) {
                if (FD_ISSET(i, &read_fds)) { // we got one!!
                    if (i == listener) {
                        // handle new connections
                        addrlen = sizeof remoteaddr;
                        newfd = accept(listener,
                                (struct sockaddr *) &remoteaddr,
                                &addrlen);

                        if (newfd == -1) {
                            perror("accept");
                        } else {
                            FD_SET(newfd, &master); // add to master set
                            if (newfd > fdmax) { // keep track of the max
                                fdmax = newfd;
                            }
                            printf("selectserver: new connection from %s on "
                                    "socket %d\n",
                                    inet_ntop(remoteaddr.ss_family,
                                    get_in_addr((struct sockaddr*) &remoteaddr),
                                    remoteIP, INET6_ADDRSTRLEN),
                                    newfd);
                            /* allocate new struct for this client */
                            void *ptr = calloc(1, sizeof (http_state_t));
                            if (ptr == NULL) {
                                /* no memory left. stop connection */
                                close(newfd); // bye!
                                FD_CLR(newfd, &master); // remove from master set
                                fprintf(stderr, "OOM, connection removed\n");
                            } else {
                                fprintf(stderr, "new machine created %p\n", ptr);
                                fd_to_machine_map[newfd] = ptr;
                            }
                        }
                    } else {
                        // handle data from a client
                        http_state_t *client = fd_to_machine_map[i]; /* fd is the index */
                        ssize_t nbytes = recv(i, client->buffer, sizeof client->buffer, 0);
                        // if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
                        if (nbytes <= 0) {
                            // got error or connection closed by client
                            if (nbytes == 0) {
                                // connection closed
                                printf("selectserver: socket %d hung up\n", i);
                            } else {
                                perror("recv");
                            }
                            close(i); // bye!
                            FD_CLR(i, &master); // remove from master set
                        } else {
                            // we got some data from a client
                            // int trv = clock_gettime(CLOCK_MONOTONIC,&client->timestamp);
                            int trv = gettimeofday(&client->timestamp, NULL);
                            if (trv  == -1) { perror("gettime problem"); }
                            int rv = http_machine(client, nbytes );
                            if (rv < 0) {
                                /* error, cleanup */
                                close(i);
                                FD_CLR(i, &master); // remove from master set
                                free(client);
                                fd_to_machine_map[i] = 0; /* remove ref */
                            }

                            //                            for (j = 0; j <= fdmax; j++) {
                            //                                // send to everyone!
                            //                                if (FD_ISSET(j, &master)) {
                            //                                    // except the listener and ourselves
                            //                                    if (j != listener && j != i) {
                            //                                        if (send(j, buf, nbytes, 0) == -1) {
                            //                                            perror("send");
                            //                                        }
                            //                                    }
                            //                                }
                            //                            }
                        }
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!

    return 0;
}
