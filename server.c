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
            int max_to_check = fdmax + 1;
            int i = 0;
            for (i = 0; i < max_to_check; i++) {
                /* get machine object pointer by FD as id */
                http_state_t *m = fd_to_machine_map[i];
                /* is it a machine then handle it */
                if (m != NULL) {
                    // set the timestamp
                    gettimeofday(&m->buffer_timestamp, NULL);
                    // call that machine, buflen=0 and hangup=0
                    int smrv = 0;
                    do {
                        smrv = http_machine_step(m, 0);
                    } while (smrv == 1);
                    if (smrv < 0) {
                        /* error or timeout we need to close and cleanup */
                        // on error always clean up anyway
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                        http_machine_deinit(m);
                        free(m);
                        fd_to_machine_map[i] = 0; /* remove ref */
                    }
                }
            }

        } else {
            // run through the existing connections looking for data to read
            for (i = 0; i <= fdmax; i++) {
                if (FD_ISSET(i, &read_fds)) { // we got one!!
                    if (i == listener) {
                        // handle new connection
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
                            /* allocate new struct for this client and clear it */
                            void *ptr = calloc(1, sizeof (http_state_t));
                            if (ptr != NULL) {
                                /* init machine with defaults */
                                http_machine_init(ptr, newfd);
                                fprintf(stderr, "new machine created %p with id=%d\n", ptr, newfd);
                                fd_to_machine_map[newfd] = ptr;
                            } else {
                                /* no memory left. stop connection */
                                close(newfd); // bye!
                                FD_CLR(newfd, &master); // remove from master set
                                fprintf(stderr, "OOM, connection removed fd=%d\n", newfd);
                            }
                        }
                    } else {
                        // handle data from a client
                        http_state_t *client = fd_to_machine_map[i]; /* fd is the index */
                        char tmp[4096];
                        int ib = jack_ringbuffer_write_space(client->recv_queue);
                        /* read upto free buffer size */
                        int numtoread = ((sizeof tmp) > ib) ? ib : sizeof tmp; 
                        
                        ssize_t nbytes = recv(i, tmp, numtoread, MSG_NOSIGNAL);
      //                  ssize_t nbytes = recv(i, client->recv_buffer, sizeof client->recv_buffer, MSG_NOSIGNAL);

                        if (nbytes <= 0) {
                            // got error or connection closed by client
                            if (nbytes == 0) {
                                // connection closed
                                printf("selectserver: socket %d hung up\n", i);
                                // signal the state machine, maybe needs to handle this
                                while (1 == http_machine_step(client,  1)) {
                                    fprintf(stderr, "call again\n");
                                }
                                // we must clean up always
                            } else {
                                // < 0 from recv, error condition
                                perror("recv");
                                // we must add cleanup code here TODO
                                while (1 == http_machine_step(client,  2)) {
                                    fprintf(stderr, "call again2\n");
                                } // error msg to machine
                            }
                            // on error always clean up anyway
                            close(i); // bye!
                            FD_CLR(i, &master); // remove from master set
                            http_machine_deinit(client);
                            free(client);
                            fd_to_machine_map[i] = 0; /* remove ref */
                            client=NULL;
                        } else {
                            // we got some data from a client, timestamp the data
                            int trv = gettimeofday(&client->buffer_timestamp, NULL);
                            if (trv == -1) {
                                perror("gettime problem");
                            }
                            
                            /* copy the recv data into the state machine */
                            int nrv = jack_ringbuffer_write(client->recv_queue,tmp,nbytes);
                            if (nrv != nbytes) {
                                fprintf(stderr,"could not fit all data into ring buffer, can not happen. fd=%d obj=%p\n",i,client);
                                /* close up and cleanup */
                            }
                            
                            int smrv = 0;
                            do {
                                // feed into state machine
                                smrv = http_machine_step(client, 0);
                            } while (smrv == 1);
                            // on error cleanup the connection and the statemachine
                            if (smrv < 0) {
                                /* error, cleanup */
                                close(i);
                                FD_CLR(i, &master); // remove from master set
                                http_machine_deinit(client);
                                free(client);
                                fd_to_machine_map[i] = 0; /* remove ref */
                                client = NULL; /* invalidate pointer */
                            } else {
                                /* Koffie what else ?? all is OK */
                                /* maybe we need to send stuff out now ?? */
                                if (jack_ringbuffer_read_space(client->send_queue) > 0) {
                                    /*
                                     // where socketfd is the socket you want to make non-blocking
int status = fcntl(socketfd, F_SETFL, fcntl(socketfd, F_GETFL, 0) | O_NONBLOCK);

if (status == -1){
  perror("calling fcntl");
  // handle the error.  By the way, I've never seen fcntl fail in this way
}
                                     
                                     */
                                    /* copy the data from the queue without moving the read pointer, since we do not know if sending will work */
                                    char tmp[1024];
                                    ssize_t nsent = 0;
                                    size_t nbytes = jack_ringbuffer_peek(client->send_queue, tmp, sizeof tmp);
                                    nsent = send(i, tmp, nbytes, MSG_NOSIGNAL);
                                    if (nsent > 0) {
                                        // update read pointer
                                        jack_ringbuffer_read_advance(client->send_queue, nsent);
                                        fprintf(stderr, "OK sending stuff obj=%p fd=%d len=%d\n", client, i, nsent);
                                    } else if (nsent < 0) {
                                        // send failed, can not continue
                                        fprintf(stderr, "Error sending stuff obj=%p fd=%d close() now\n", client, i);
                                        /* TODO cleanup and close up */
                                    } else {
                                        // no data available was sent out, although the queue was not empty, very strange
                                        fprintf(stderr, "no data sent obj=%p fd=%d\n", client, i);
                                    }

                                }
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
