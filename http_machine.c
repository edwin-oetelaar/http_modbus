/* copyright 2015 etc Edwin van den Oetelaar
 * all rights reserved
 * written from scratch, no cut and paste
 */

#include "http_machine.h"
#include "ringbuffer.h"
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>

int http_machine_init(http_state_t *self, int fd) {
    self->current_state = 0;
    self->my_fd = fd;
    self->send_queue = jack_ringbuffer_create(4096); /* 1 page buffer */
    self->recv_queue = jack_ringbuffer_create(4096); /* 1 page buffer */
    gettimeofday(&self->connection_timestamp, NULL);
    fprintf(stderr, "machine %p init done\n", self);
    return 0;
}

int http_machine_deinit(http_state_t *self) {
    // clear memory etc
    free(self->recv_queue);
    free(self->send_queue);
    fprintf(stderr, "machine %p deinit done\n", self);
    return 0;
}

int is_uri_char(uint8_t c) {
    /* return 1 on success, 0 on fail */
    if (c >= 'A' && c <= 'Z') return 1;
    if (c >= 'a' && c <= 'z') return 1;
    if (c >= '0' && c <= '9') return 1;
    if (
            c == '?' || c == '/' || c == '%' || c == '.' || c == '_' ||
            c == '+' || c == '-' || c == ':' || c == '#' || c == '@' ||
            c == '$' || c == '&' || c == '=' || c == ';' || c == '~' ||
            c == '*'
            ) return 1;

    return 0;
}

int is_httpver_char(uint8_t c) {
    if (c == 'H' || c == 'T' || c == 'P' || c == '/' ||
            c == '0' || c == '1' || c == '.')
        return 1;
    else
        return 0;

}

int is_header_char(uint8_t c) {
    /* accept any char for now, execpt EOL */
    if (c == '\r' || c == '\n') return 0;
    return 1;
}

int handle_header_line(http_state_t *self, char *buf, int buflen) {
    fprintf(stderr, "handle line '%s' len=%d\n", buf, buflen);

    // return -1 on bogus input
    // 0 on ok
    // 1 on end of headers

    if (buflen == 0) {
        fprintf(stderr, "end of headers\n");
        return 1; /* headers end */
    }
    return 0; /* no problem read next header*/
}

int is_known_http_verb(const char *verb) {
    if (strcmp(verb, "GET") == 0) return 1;
    if (strcmp(verb, "POST") == 0) return 1;
    if (strcmp(verb, "PUT") == 0) return 1;
    if (strcmp(verb, "DELETE") == 0) return 1;
    return 0; /* unknown verb */
}

int http_machine_step_single_byte(http_state_t *self, char c, int control_flag) {
    /* standard FSM
     * based on input c take one step based on the current state
     * no way to do multiple state-transitions based on one input char
     * if control_flag == 1 : reset the machine before processing char
     */
    char tmp[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\nHello everybody";
    if (control_flag == 1) {
        /* reset */
        fprintf(stderr, "reset http_statemachine to state==0\n");
        self->current_state = 0; /* initial state */
    }

    int ns = 0;
    uint8_t cc = isprint(c) ? c : ' ';
    fprintf(stderr, "http_machine_step_input working on state=%d c=%c hex=0x%2.2x\n", self->current_state, cc, c);

    /* choose action based on current state */
    switch (self->current_state) {

        case 0:
            if (!(self->line_buffer_index < sizeof self->VERB)) {
                /* VERB too long protocol error */
                fprintf(stderr, "http verb too long, protocol error\n");
                return -1; // ns = 1000;
            } else if (c >= 'A' && c <= 'Z') {
                /* normal char seen */
                self->line_buffer[self->line_buffer_index] = c;
                self->line_buffer_index++; /* next char in line */
                ns = 0;
            } else if (c == 0x20) {
                self->line_buffer[self->line_buffer_index] = 0; /* terminate string */
                strcpy(self->VERB, self->line_buffer);
                self->line_buffer_index = 0; /* start at begin of buffer again for URI */
                fprintf(stderr, "VERB complete '%s'\n", self->VERB);

                if (is_known_http_verb(self->VERB) == 0) {
                    fprintf(stderr, "protocol error, unknown verb %s\n", self->VERB);
                    return -1; // ns = 1000; /* unknown verb protocol error */
                } else {
                    ns = 2; /* VERB complete go on to find URI */
                }
            } else {
                /* verb not finished, error in protocol */
                return -1; // ns = 1000; /* protocol error seen */
            }

            break;

        case 2:
            /* copy the URI part of the first line */
            /* URI ends with a space */
            if (!(self->line_buffer_index < sizeof self->URI)) {
                /* no cnt++ here */
                return -1; // ns = 1000; /* too long is protocol error  */
            } else if (is_uri_char(c)) {
                self->line_buffer[self->line_buffer_index] = c;
                self->line_buffer_index++; /* next char in line */
                ns = 2;
            } else if (c == 0x20) {
                /* a space found, uri ends here */
                self->line_buffer[self->line_buffer_index] = 0; /* terminate string */
                strcpy(self->URI, self->line_buffer);
                self->line_buffer_index = 0; /* start at begin of buffer again */
                fprintf(stderr, "URI complete '%s'\n", self->URI);
                ns = 3; /* on to the protocol version stuff*/
            } else {
                fprintf(stderr, "WTF happened invalid URI char '%2x'\n", c);
                return -1; // ns = 1000; /* protocol error */
            }

            break;

        case 3:
            if (!(self->line_buffer_index < sizeof self->HTTPVER)) {
                return -1; // ns = 1000; /* too long is protocol error  */
            } else if (is_httpver_char(c)) {
                self->line_buffer[self->line_buffer_index] = c;
                self->line_buffer_index++; /* next char in line */
                ns = 3;
            } else if (c == '\r') {
                /* a CR found, HTTPVER ends here */
                self->line_buffer[self->line_buffer_index] = 0; /* terminate string */
                strcpy(self->HTTPVER, self->line_buffer);
                self->line_buffer_index = 0; /* start at begin of buffer again */
                fprintf(stderr, "HTTP VER complete '%s'\n", self->HTTPVER);
                ns = 4; /* on to the protocol version stuff*/

            } else {
                fprintf(stderr, "WTF happened invalid HTTP VERSION char '%2x'\n", c);
                return -1; // ns = 1000; /* protocol error */
            }
            break;

        case 4: /* we look for the LINEFEED at the end of the first line */
            if (c == '\n') {
                /* we have a complete line and can now handle header lines */
                ns = 5; /* handle header line */
            } else {
                return -1; // ns = 1000; /* unexpected char, is protocol error */
            }
            break;

        case 5:
            /* read upto a CR into line_buffer a header line */
            if (!(self->line_buffer_index < sizeof self->line_buffer)) {
                return -1; // ns = 1000; /* too long is protocol error  */
            } else if (is_header_char(c)) {
                /* the byte is a valid byte inside a header line, append to line */
                self->line_buffer[self->line_buffer_index] = c;
                self->line_buffer_index++; /* next char in line */
                ns = 5;
            } else if (c == '\r') {
                /* a CR found, header line ends here */
                self->line_buffer[self->line_buffer_index] = 0; /* terminate string */
                ns = 6; /* read closing \n from line */
            } else {
                fprintf(stderr, "WTF happened invalid header char '%2x'\n", c);
                return -1; // ns = 1000; /* protocol error */
            }
            break;

        case 6:
            /* read LF after header line */
            if (c == '\n') {
                int rv = handle_header_line(self, self->line_buffer, self->line_buffer_index);
                if (rv < 0) {
                    /* invalid header protocol error */
                    fprintf(stderr, "invalid header line : %s\n", self->line_buffer);
                    return -1; // ns = 1000;
                } else if (rv == 1) {
                    // empty header, ends the header
                    ns = 20;
                } else {
                    fprintf(stderr, "header line ok : '%s'\n", self->line_buffer);
                    self->line_buffer_index = 0; /* start at begin of buffer again */
                    ns = 5; /* read another header line upto \r */
                }

            } else {
                fprintf(stderr, "unexpected char %2x protocol error\n", c);
                return -1; // ns = 1000;
            }
            break;

        case 20:
            /* read body of http message if any is expected */
            /* should respect the Content-Length header value */
            if (!strcmp(self->VERB, "GET")) {
                fprintf(stderr, "expecting no body, send response now\n");
                ns = 21;
            } else {
                ns = 20;
            }
            break;

        case 21:
        {

            size_t n = jack_ringbuffer_write(self->send_queue, tmp, sizeof tmp);
            fprintf(stderr, "pushed %d into queue\n", n);
            ns = 21;
        }
            break;

            //        case 1000:
            //            /* cleanup and close up */
            //            return -1; // clean exit close in main loop
            //            break;

        default:
            fprintf(stderr, "Can not happend, unhandled state %d\n", self->current_state);
            return -1;

    }

    /* machine to next state*/
    self->current_state = ns;

    return 0;
}

int http_machine_step(http_state_t *self, int event_type) {
    /* self, event_type flag (when peer closed==1 network error==2, timer==0) */
    /* if buflen == 0 then we have no data but just timer event, timestamp in machine->timestamp */
    /* the machine, uses timestamp to handle timeouts */
    /* return 0 on success handling buffer or timeout event */
    /* return negative value on timeouts or client hangup (-1,-2,-3) , or protocol error (-100,-101, etc) */
    

    /* we get a buffer of size 0 (timer or error)  or a buffer of some size.. */
    /* we must process the whole buffer here, we can not leave stuff unprocessed
     * well... maybe we could, let us return CALL_ME_AGAIN (1) so the caller calls us again */
    /* we iterate over the whole buffer pushing every byte into the state-machine */

    //int ns = 0; /* next state */
    /* get the byte */
    char c = 0; // work on this  

    // enum states_t  { initial_s, verb_s, uri_s, httpver_s,  };

    size_t nn = jack_ringbuffer_read(self->recv_queue, &c, 1);
    while (nn > 0) {

        /* push into state machine */
        int rv = http_machine_step_single_byte(self, c, 0);
        /* check return values for next actions */
        if (rv < 0) {
            /* protocol error */
            return -1; /* mainloop must close() connection */
        } else if (rv == 1) {
            /* complete GET request received. handle it and start sending answer now */
        } else if (rv == 2) {
            /* complete POST received, start handling and send answer  */
        } else if (rv == 3) {
            /* PUT request */
        } else if (rv == 4) {
            /* DELETE REQUEST */
        } else {
            /* nothing to say, all is ok, I need more bytes */
        }
        /* next byte from queue */
        nn = jack_ringbuffer_read(self->recv_queue, &c, 1);
    }

    /* we reset timeout after every reception */
    /* if we idle too long it still times out */
    self->idle_timestamp = self->buffer_timestamp;

    if (event_type == 0) {
        /* two ways to timeout, on idle timeout (data already received)  and on connection_timestamp */
        if (self->idle_timestamp.tv_sec == 0) {
            /* no data received yet, because then the idle_timestamp would be a value */
            /* see if we have a timeout on CONNECT without data coming in */
            if ((self->buffer_timestamp.tv_sec - self->connection_timestamp.tv_sec) < 20) {
                /* no timeout yet on connection */
            } else {
                fprintf(stderr, "no initial data in 20 seconds, close connection\n");
                return -2;
            }
        } else {
            /* a timer event between some data, just check the idle_timestamp for now */
            if ((self->buffer_timestamp.tv_sec - self->idle_timestamp.tv_sec) < 10) {
                /* no timeout yet */
            } else {
                /* taking too long */
                fprintf(stderr, "taking too long 10 second idle, close connection\n");
                return -3;
            }

        }
    } else if (event_type == 1) {
        return -1;
    } else if (event_type == 2) {
        return -1;
    } else {
        fprintf(stderr, "invalid hangup value %d\n", event_type);
        return -1;
    }

    fprintf(stderr, "here..\n");

    return 0;
}
