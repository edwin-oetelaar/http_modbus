/* copyright 2015 etc Edwin van den Oetelaar
 * all rights reserved
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
#include "http_machine.h"


void * fd_to_machine_map[1024] = {0,};


// get sockaddr, IPv4 or IPv6:

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*) sa)->sin6_addr);
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
    int i, rv;

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
            // signal all running state machines of timer event
            // TODO
            int max_to_check = fdmax + 1;
            int i = 0;
            for (i = 0; i < max_to_check; i++) {
                if (fd_to_machine_map[i] != NULL) {
                    // call that machine
                    http_machine_step(fd_to_machine_map[i], 0, 0);
                }
            }

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
                                /* init machine with defaults */
                                http_machine_init(ptr);
                                fprintf(stderr, "new machine created %p\n", ptr);
                                fd_to_machine_map[newfd] = ptr;
                            }
                        }
                    } else {
                        // handle data from a client
                        http_state_t *client = fd_to_machine_map[i]; /* fd is the index */
                        ssize_t nbytes = recv(i, client->recv_buffer, sizeof client->recv_buffer, 0);

                        if (nbytes <= 0) {
                            // got error or connection closed by client
                            if (nbytes == 0) {
                                // connection closed
                                printf("selectserver: socket %d hung up\n", i);
                                // signal the state machine, maybe needs to handle this
                                http_machine_step(client, 0, 1);
                                // we must clean up always

                            } else {
                                perror("recv");
                                // we must add cleanup code here TODO
                                http_machine_step(client, 0, 2); // error msg to machine
                            }
                            close(i); // bye!
                            FD_CLR(i, &master); // remove from master set
                            http_machine_deinit(client);
                            free(client);
                            fd_to_machine_map[i] = 0; /* remove ref */
                        } else {
                            // we got some data from a client, timestamp the data
                            int trv = gettimeofday(&client->timestamp, NULL);
                            if (trv == -1) {
                                perror("gettime problem");
                            }
                            // feed into state machine
                            int rv = http_machine_step(client, nbytes, 0);
                            // on error cleanup the connection and the statemachine
                            if (rv < 0) {
                                /* error, cleanup */
                                close(i);
                                FD_CLR(i, &master); // remove from master set
                                http_machine_deinit(client);
                                free(client);
                                fd_to_machine_map[i] = 0; /* remove ref */
                            } else {
                                /* Koffie what else ?? all is OK */
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
