/* copyright 2015 etc Edwin van den Oetelaar
 * all rights reserved
 */
#ifndef HTTP_MACHINE_H
#define    HTTP_MACHINE_H

#ifdef    __cplusplus
extern "C" {
#endif


#include <sys/time.h>
#include <stdint.h>
#include "ringbuffer.h"

typedef struct {
    int current_state;
    /* state of the machine */
    int my_fd;
    /* my socket fd to read and write */
    int keep_alive;
    /* use keep alive http 1.1 */
    int deferred_cleanup;
    /* try to send stuff, then cleanup and destroy */
    struct timeval buffer_timestamp;
    /* buffer timestamping gettimeofday */
    struct timeval idle_timestamp;
    /* updated timestamp after every recv of data */
    struct timeval connection_timestamp;
    /* moment of connection */
    jack_ringbuffer_t *recv_queue;
    /* the input buffer, dynamic alloc */
    jack_ringbuffer_t *send_queue;
    /* dynamic allocated */
    int line_buffer_index;
    /* index offset in de linebuffer */
    char line_buffer[512];
    /* line buffer temp */
    char VERB[16];
    /* VERB, GET PUT DELETE POST etc */
    int VERB_code;
    /* code for verb, GET=1 POST=2 etc. */
    char URI[512];
    /*  mag 32k zijn volgens spec */
    char HTTPVER[16];
    /* version HTTP/1.0 or HTTP/1.1 */
    char HOST[512];
    /* Host from headers, zeker groot genoeg */
    int content_length;
    /* from the header, needed for POST handling */
    int content_body_index;
    /* offset into buffer counter */
    char BODY[1024]; /* body van de POST message komt hier */
} http_state_t;


int http_m_init(http_state_t *self, int fd);

int http_m_reset(http_state_t *self);

int http_m_deinit(http_state_t *self);

int http_m_step(http_state_t *self, int hangup);

int http_m_step_single_byte(http_state_t *self, uint8_t c, int control_flag); /* parse input chars */

#ifdef    __cplusplus
}
#endif

#endif	/* HTTP_MACHINE_H */

