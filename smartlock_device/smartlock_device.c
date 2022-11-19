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
#include <openssl/sha.h>

#include "smartlock_device.h"
#include "logger.h"



const char SUBCRIBE_TOPIC[]  = "cmd/series100/group-3/lock-1/+";
const char PUBLISH_TOPIC[]   = "cmd/series100/group-3/lock-1/res";
const char SERVER_SADDR[]    = "tcp://localhost:9091";
const char SERVER_PADDR[]    = "tcp://localhost:9092";


int subthread_arg            = 0;
int pubthread_arg            = 0;
bool close_app               = false;


pthread_t subthread_id;
pthread_t pubthread_id;


/* ZMQ context */
void *context;


/* Globals */

static char sha1_string[SHA_DIGEST_LENGTH*2+1];


/* prototypes */
void cleanup();
void *subcriber_thread();
void *publisher_thread();

char *create_payload(const char *client_id, const char *session_id, const char *pcode, const char *code);
int parse_sub_payload ( const char *message, int len, smartlock_sub_t *payload );


static void signal_handler(int sig) {
    close_app = true;

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

    
    const char passcode[] = "OIJQOWIEJOQIWJEOJI123123~@#)(ii;";

    unsigned char hash[SHA_DIGEST_LENGTH];
    size_t code_length = strlen (passcode);
    SHA1(passcode, code_length, hash);

    for (int i=0; i< SHA_DIGEST_LENGTH;++i)
        sprintf(&sha1_string[i*2], "%02x", (unsigned int)hash[i]);


    context = zmq_ctx_new ();

    if (pthread_create (&subthread_id, NULL, 
                (void*)subcriber_thread, &subthread_arg) != 0) {
        perror ("pthread_create()");
        cleanup();
        exit ( EXIT_FAILURE );
    }

    if (pthread_create (&pubthread_id, NULL, 
                (void*)publisher_thread, &pubthread_arg) != 0) {
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

    sprintf (msg, "Subcribed Topic\t@ %s", SUBCRIBE_TOPIC);
    dbg_log (msg);
    sprintf (msg, "Publisher\t\t@ %s", SERVER_PADDR);
    dbg_log (msg);

    sleep (2);

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

void *publisher_thread(void *args) {
    int rc;
    char msg[256];
    char topic[strlen ( SUBCRIBE_TOPIC)+1];
    char buffer[1024];
    void *pusher;

    pusher = zmq_socket (context, ZMQ_PUSH);
    rc = zmq_connect (pusher, SERVER_PADDR);
    assert ( rc == 0 );

    int linger = 0; // Do not wait for timeout after socket close request.
    rc = zmq_setsockopt ( pusher, ZMQ_LINGER, &linger, sizeof(linger) );

    sprintf (msg, "Pushing Messages\t@ %s", SERVER_PADDR);
    dbg_log (msg);

    while (!close_app) {
        char *data = create_payload ("lock-1", "0193-0428", sha1_string, "200");
        if (data) {
            zmq_send (pusher, PUBLISH_TOPIC, strlen (PUBLISH_TOPIC), ZMQ_SNDMORE);
            zmq_send (pusher, data, strlen (data), ZMQ_DONTWAIT);
            free (data);
        }
        sleep (1); // 1 second sleep
    }
    dbg_log ("Shutting down pusher");
    zmq_close (pusher);
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

void cleanup() {
    if (context) {
        zmq_ctx_term (context);
        dbg_log ("Completed shutting down ZMQ context");
        context = NULL;
    }
}


char *create_payload(const char *client_id, const char *session_id, 
                            const char *pcode, const char *code_value) {
    char *str = NULL;
    cJSON *payload = NULL;
    cJSON *clientId = NULL;
    cJSON *sessionId = NULL;
    cJSON *passcode = NULL;
    cJSON *res = NULL;
    cJSON *code = NULL;

    payload = cJSON_CreateObject();
    if (payload == NULL)
        goto end;
    

    clientId = cJSON_CreateString(client_id);
    if (clientId == NULL)
        goto end;
    cJSON_AddItemToObject(payload, "clientId", clientId);

    sessionId = cJSON_CreateString(session_id);
    if (sessionId == NULL)
        goto end;
    cJSON_AddItemToObject(payload, "sessionId", sessionId);


    passcode = cJSON_CreateString (pcode);
    if (passcode == NULL)
        goto end;
    cJSON_AddItemToObject(payload, "passcode", passcode);

    res = cJSON_CreateObject();
    if (res == NULL)
        goto end;
    cJSON_AddItemToObject(payload, "res", res);


    code = cJSON_CreateString(code_value);
    if (code == NULL)
        goto end;
    cJSON_AddItemToObject(res, "code", code);

    str = cJSON_Print(payload);
    if (str == NULL)
        dbg_log ("Failed to print payload");
end:
    cJSON_Delete(payload);
    return str;
}
