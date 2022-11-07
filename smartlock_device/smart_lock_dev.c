#include <stdio.h>
#include <stdlib.h>
#include <zmq.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <cjson/cJSON.h>


#include "smart_lock_dsa.h"
#include "logger.h"



const char SUBCRIBE_TOPIC[]  = "cmd/series100/group-3/lock-1/";
const char SERVER_SADDR[]    = "tcp://192.168.1.80:9091";

int subthread_arg            = 0;
bool close_app               = false;


pthread_t subthread_id;
pthread_t pubthread_id;


/* ZMQ context */
void *context;


/* prototypes */
void cleanup();
void *subcriber_thread();
void *publisher_thread();

int parse_sub_payload ( const char *message, int len, smartlock_sub_t *payload );


static void signal_handler(int sig) {
    close_app = true;
    sleep (1); // give some time for other threads

    char msg[256];
    bool is_error=false;

    switch (sig) {
        case SIGSEGV:
            dbg_log ("Segmentation fault");
            is_error=true;
            break;
        case SIGINT:
            dbg_log( "Interrupted");
            break;
        default:
            sprintf (msg, "Unknown signal %d\n", sig );
            dbg_log ( msg );
            break;
    }

    cleanup ();

    sleep (1);
    if (is_error) 
        exit (EXIT_FAILURE);
    else 
        exit ( EXIT_SUCCESS );
}

int main() {
    signal(SIGINT,  signal_handler);
    signal(SIGSEGV, signal_handler);

    context = zmq_ctx_new ();

    if (pthread_create (&subthread_id, NULL, (void*)subcriber_thread, &subthread_arg) != 0) {
        perror ("pthread_create()");
        cleanup();

        exit ( EXIT_FAILURE );
    }

    while (!close_app) { // main loop
        sleep (1);
    }
}


void *subcriber_thread(void *args) {
    int rc;
    int recv_len=0;
    char msg[256];
    char topic[strlen ( SUBCRIBE_TOPIC)+1];
    char buffer[1024];
    void *subcriber;

    smartlock_sub_t payload; memset (&payload, 0, sizeof (smartlock_sub_t));
    
    subcriber = zmq_socket (context, ZMQ_SUB);
    rc = zmq_connect (subcriber, SERVER_SADDR);
    assert ( rc == 0);

    rc = zmq_setsockopt (subcriber, ZMQ_SUBSCRIBE, SUBCRIBE_TOPIC, strlen ( SUBCRIBE_TOPIC) );
    assert ( rc == 0 );

    int linger = 0; // Do not wait for timeout after socket close request.
    rc = zmq_setsockopt ( subcriber, ZMQ_LINGER, &linger, sizeof(linger) );

    sprintf (msg, "Subcribed to topic %s ==== Publisher @ %s", SUBCRIBE_TOPIC, SERVER_SADDR);
    dbg_log (msg);

    sleep (5);

    while (!close_app) {
        zmq_recv (subcriber, topic, strlen (SUBCRIBE_TOPIC), 0 );
        if (strcmp ( topic, SUBCRIBE_TOPIC ) != 0)
            break; // something is wrong here

        recv_len = zmq_recv (subcriber, buffer, sizeof(buffer), 0 );

        // dbg_log ( topic );
        // dbg_log ( buffer );
        if (recv_len > 0) {
            if (!parse_sub_payload (buffer, recv_len, &payload)) {
                // dbg_log ( "Smart Lock Message Parsed" );
            }
        }
    }

    dbg_log ("Shutting down subcriber");
    zmq_close (subcriber);
}


void cleanup() {
    if (context) {
        zmq_ctx_term (context);
        dbg_log ("Completed shutting down ZMQ context");

        context = NULL;
    }
}

int parse_sub_payload (const char *message, int len, smartlock_sub_t *payload) {
    if (!message || !payload) // edge cases handling
        return 1;

    cJSON *json = cJSON_ParseWithLength (message, len);

    char *str = cJSON_PrintUnformatted (json);
    dbg_log ( str );
    
    cJSON_free (json);
    return 0;
}