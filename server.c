/* copyright 2015 etc Edwin van den Oetelaar
 * all rights reserved
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/time.h>

#include "server.h"
#include "generic_machine.h"
#include "http_machine.h"
#include "mb_tcp_machine.h"


void *fd_to_machine_map[1024] = {0,};


// get sockaddr, IPv4 or IPv6:

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}

int run_server(void) {
    fd_set master; // master file descriptor list
    fd_set read_fds; // temp file descriptor list for select()
    fd_set write_fds; // temp file descriptor list for select() 
    int fdmax = -1; // maximum file descriptor number

    int listener = -1; // listening socket descriptor
    int newfd = -1; // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes = 1; // for setsockopt() SO_REUSEADDR, below

    struct addrinfo hints, *ai, *p;

    FD_ZERO(&master); // clear the master and temp sets
    FD_ZERO(&read_fds);

    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int rv;
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
        /* try to set NODELAY on listener, might be inherited by connected sockets */
        if (setsockopt(listener, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof (int))) {
            perror("setsockopt(listener, TCP_NODELAY)");
        }

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }
    freeaddrinfo(ai); // all done with this

    // if we got here, it means we didn't get bound
    if (p == NULL) {
        fprintf(stderr, "selectserver: failed to bind\n");
        exit(2);
    }

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
        /* call select() wait for at most 1 second */
        int srv = select(fdmax + 1, &read_fds, NULL, NULL, &timeout);
        /* check errors */
        if (srv == -1) {
            perror("select");
            exit(4);
        } else if (srv == 0) {
            /* handle timeouts */
            fprintf(stderr, "timeout\n");
            // signal all running state machines of timer event
            int i = 0;
            for (i = 0; i <= fdmax; i++) {
                /* get machine object pointer by FD as id */
                gm_state_t *gm = fd_to_machine_map[i];
                /* is it a http_machine then handle it */
                if ((gm != NULL) && (gm->machine_type == http_machine_magic)) {
                    http_state_t *m = (http_state_t *) gm;
                    // set the timestamp
                    gettimeofday(&m->call_timestamp, NULL);
                    // call that machine, buflen=0 and hangup=0
                    int smrv = http_m_step(m, 0);
                    /* check for error */
                    if ((smrv < 0) && (m->deferred_cleanup == 0)) {
                        /* error or timeout we need to close and cleanup */
                        // on error always clean up anyway
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                        http_m_deinit(m);
                        free(m);
                        fd_to_machine_map[i] = 0; /* remove ref */
                    }
                }

                /* is it a mb machine, then handle it */
                if ((gm != NULL) && (gm->machine_type == mb_machine_magic)) {
                    mb_state_t *m = (mb_state_t *) gm;
                    // set the timestamp
                    gettimeofday(&m->call_timestamp, NULL);
                    // call that machine, buflen=0 and hangup=0
                    int smrv = mb_m_step(m, 0);
                    /* check for error */
                    if ((smrv < 0) && (m->deferred_cleanup == 0)) {
                        /* error or timeout we need to close and cleanup */
                        // on error always clean up anyway
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                        mb_m_deinit(m);
                        free(m);
                        fd_to_machine_map[i] = 0; /* remove ref */
                    }
                }
            }

        } else {
            // run through the existing connections looking for data to read
            int i;
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
                                    get_in_addr((struct sockaddr *) &remoteaddr),
                                    remoteIP, INET6_ADDRSTRLEN),
                                    newfd);
                            /* set socket options, KEEP_ALIVE en DISABLE NAGLE  */
                            /* Set the option active */
                            int optval = 1;
                            int optlen = sizeof (optval);
                            if (setsockopt(newfd, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) {
                                perror("setsockopt(SO_KEEPALIVE)");
                            }

                            if (setsockopt(newfd, IPPROTO_TCP, TCP_NODELAY, &optval, optlen) < 0) {
                                perror("setsockopt(TCP_NODELAY)");

                            }

                            /* allocate new struct for this client and clear it */
                            void *ptr = calloc(1, sizeof (http_state_t));
                            if (ptr != NULL) {
                                /* init machine with defaults */
                                http_m_init(ptr, newfd);
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
                        /* amount of space in recv buffer ? */
                        size_t ib = jack_ringbuffer_write_space(client->recv_queue);
                        /* read upto free buffer size */
                        size_t numtoread = ((sizeof tmp) > ib) ? ib : sizeof tmp;
                        /* read the bytes from the socket in blocking mode, should not block, since select() told us */
                        ssize_t nbytes = recv(i, tmp, numtoread, MSG_NOSIGNAL);

                        if (nbytes <= 0) {
                            // got error or connection closed by client
                            if (nbytes == 0) {
                                // connection closed
                                printf("selectserver: socket %d hung up\n", i);
                                // signal the state machine, maybe needs to handle this

                                http_m_step(client, 1);

                                // we must clean up always
                            } else {
                                // < 0 from recv, error condition
                                perror("recv");
                                // we must add cleanup code here TODO
                                http_m_step(client, 2);
                            }
                            // on error always clean up anyway
                            close(i); // bye!
                            FD_CLR(i, &master); // remove from master set
                            http_m_deinit(client);
                            free(client);
                            fd_to_machine_map[i] = 0; /* remove ref */
                            client = NULL;
                        } else {
                            // we got some data from a client, timestamp the data
                            int trv = gettimeofday(&client->call_timestamp, NULL);
                            if (trv == -1) {
                                perror("gettime problem");
                            }

                            /* copy the recv data into the state machine */
                            size_t nrv = jack_ringbuffer_write(client->recv_queue, tmp, (size_t) nbytes);
                            if (nrv != nbytes) {
                                fprintf(stderr,
                                        "could not fit all data into ring buffer, can not happen. fd=%d obj=%p\n", i,
                                        client);
                                /* close up and cleanup */
                            }


                            // feed into state machine
                            int smrv = http_m_step(client, 0);

                            // on error cleanup the connection and the statemachine
                            if ((smrv < 0) && client->deferred_cleanup == 0) {
                                /* error, cleanup now if requested */
                                close(i);
                                FD_CLR(i, &master); // remove from master set
                                http_m_deinit(client);
                                free(client);
                                fd_to_machine_map[i] = 0; /* remove ref */
                                client = NULL; /* invalidate pointer */
                            } else {
                                /* Koffie what else ?? 
                                 * all is OK */

                            }
                        }
                    }
                } // fd is set
            } // end for fds
        }

        /* loop over every http machine and check transmit buffer */
        int j;
        for (j = 0; j <= fdmax; j++) {
            // send to everyone!
            if (FD_ISSET(j, &master)) {
                // except the listener
                if (j != listener) {
                    http_state_t *mm = fd_to_machine_map[j];
                    if (mm != NULL && (mm->machine_type == http_machine_magic)) {
                        /* is there data in TX buffer to send ?? */
                        if (jack_ringbuffer_read_space(mm->send_queue) > 0) {
                            /*
                             // where socketfd is the socket you want to make non-blocking
                            int status = fcntl(socketfd, F_SETFL, fcntl(socketfd, F_GETFL, 0) | O_NONBLOCK);

                            if (status == -1){
                            perror("calling fcntl");
                            // handle the error.  By the way, I've never seen fcntl fail in this way
                            }
                                     
                             */
                            /* copy the data from the queue without moving the read pointer, since we do not know if sending will work */

                            char tmp[1024] = {0,};
                            size_t nbytes = jack_ringbuffer_peek(mm->send_queue, tmp, sizeof tmp);
                            if (nbytes > 0) {
                                /* TODO set the socket to NO_BLOCK so it will not block 
                                 * and add a TIMEOUT here so we do not keep trying forever
                                 */
                                ssize_t nsent = send(j, tmp, nbytes, MSG_NOSIGNAL);
                                if (nsent > 0) {
                                    // update read pointer, data has left the building
                                    jack_ringbuffer_read_advance(mm->send_queue, (size_t) nsent);
                                    fprintf(stderr, "OK sending stuff obj=%p fd=%d len=%zd\n", mm, j, nsent);
                                } else if (nsent < 0) {
                                    // send failed, can not continue
                                    fprintf(stderr, "Error sending stuff obj=%p fd=%d close() now\n", mm, j);
                                    perror("vreemd:");
                                    /* TODO cleanup and close up, set deferred_cleanup flag maybe */
                                } else {
                                    // no data available was sent out, although the queue was not empty, very strange
                                    fprintf(stderr, "no data sent obj=%p fd=%d\n", mm, j);
                                }
                            }
                            /* after sending stuff, clean up the object if so requested */
                            if ((nbytes == 0) && mm->deferred_cleanup == 1) {
                                /* no more bytes to send, and cleanup requested */
                                close(j);
                                FD_CLR(j, &master); // remove from master set
                                http_m_deinit(mm);
                                free(mm);
                                fd_to_machine_map[j] = 0; /* remove ref */
                                mm = NULL; /* invalidate pointer */
                            }
                        }
                    } else {
                        fprintf(stderr, "Unexpected NULL object in transmit fd=%d obj=%p\n", j, mm);
                    }
                } // not listener
            } // if fd is set
        } // end for fds to send
    } // END for(;;)

    return 0;
}


/*
 * speudo code
 * 
 * list of http connections
 * list of modbus connections
 * 
 * bind to port
 * listen on port
 * while (1) {
 *   accept connections (http)
 *   push into http_m statemachine
 *   
 * 
 * if message needs modbus request
 * 
 */

