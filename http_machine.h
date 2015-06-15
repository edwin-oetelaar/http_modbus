/* copyright 2015 etc Edwin van den Oetelaar
 * all rights reserved
 */
#ifndef HTTP_MACHINE_H
#define	HTTP_MACHINE_H

#ifdef	__cplusplus
extern "C" {
#endif


#include <sys/time.h>
#include "ringbuffer.h"

    typedef struct {
        int current_state; /* state of the machine */
        int my_fd; /* my socket fd to read and write */
        struct timeval buffer_timestamp; /* buffer timestamping gettimeofday */
        struct timeval idle_timestamp; /* updated timestamp after every recv of data */
        struct timeval connection_timestamp; /* moment of connection */
        jack_ringbuffer_t *recv_queue; /* the input buffer, dynamic alloc */
        // char recv_buffer[512]; /* filled direct by recv  ZERO COPY */
        // char send_buffer[512]; /* used directly by send  ZERO COPY */
        jack_ringbuffer_t *send_queue; /* dynamic allocated */
        int line_buffer_index; /* index offset in de linebuffer */
        char line_buffer[512]; /* line buffer temp */
        // char header_line_buffer[512]; /* buffer for one header line */s
        // int header_line_buffer_index; /* index offset in header line buffer */
        char VERB[16]; /* VERB, GET PUT DELETE POST etc */
        char URI[512]; /*  mag 32k zijn volgens spec */
        char HTTPVER[16]; /* version HTTP/1.0 or HTTP/1.1 */
        char HOST[512]; /* Host from headers, zeker groot genoeg */
        char BODY[512]; /* body van de POST message komt hier */

    } http_state_t;


    int http_machine_init(http_state_t *self, int fd);
    int http_machine_deinit(http_state_t *self);
    int http_machine_step(http_state_t *self, int hangup);

#ifdef	__cplusplus
}
#endif

#endif	/* HTTP_MACHINE_H */

