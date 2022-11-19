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

#include "logger.h"




const char PUBLISH_TOPIC[]      = "cmd/series100/group-3/lock-1/+";
const char SERVER_PUBADDR[]     = "tcp://*:9091";
const char SERVER_COLLECTADDR[] = "tcp://*:9092";


int pubthread_arg           = 0;
int collectorthread_arg     = 0;
bool close_app              = false;


pthread_t pubthread_id;
pthread_t collectorthread_id;


/* ZMQ context */
void *context;


/* prototypes */
void cleanup();
void *publisher_thread();
void *collector_thread();

char *create_payload(const char *client_id, const char *session_id, const char *topic);


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

    if (is_error) 
        exit (EXIT_FAILURE);
    else 
        exit ( EXIT_SUCCESS );
}

int main() {
    signal(SIGINT,  signal_handler);
    signal(SIGSEGV, signal_handler);

    context = zmq_ctx_new ();

    if (pthread_create (&pubthread_id, NULL, 
                    (void*)publisher_thread, &pubthread_arg) != 0) {
        perror ("pthread_create()");
        cleanup();
        exit ( EXIT_FAILURE );
    }

    if (pthread_create (&collectorthread_id, NULL, 
                    (void*)collector_thread, &collectorthread_arg) != 0) {
        perror ("pthread_create()");
        cleanup();
        exit ( EXIT_FAILURE );
    }

    while (!close_app) { // main loop
        sleep (1);
    }
}


void *publisher_thread(void *args) {
    int rc;
    int recv_len=0;
    char msg[256];
    char buffer[1024];
    void *publisher;

    publisher = zmq_socket (context, ZMQ_PUB);
    rc = zmq_bind (publisher, SERVER_PUBADDR);
    assert ( rc == 0);

    sprintf (msg, "Publishing Messages\t@ %s", SERVER_PUBADDR);
    dbg_log (msg);

    uint32_t count = 0;

    while (!close_app) {
        char *data = create_payload("mobile-1", "0193-0428", "cmd/series100/mobile-1/res");

        if (data) {
            zmq_send ( publisher, PUBLISH_TOPIC, strlen (PUBLISH_TOPIC), ZMQ_SNDMORE );
            zmq_send ( publisher, data, strlen ( data ), ZMQ_DONTWAIT );
            free ( data );
        }
        usleep ( 10000000 ); // 10 second for publishing each message
    }

    dbg_log ("Shutting down publisher");
    zmq_close (publisher);

    return (NULL);
}


void *collector_thread(void *args) {
    int rc;
    char msg[256];

    char topic[128]; memset ( topic, 0, sizeof (topic));
    char buffer[1024]; memset ( buffer, 0, sizeof (buffer));

    void *collector;

    collector = zmq_socket (context, ZMQ_PULL);
    rc = zmq_bind (collector, SERVER_COLLECTADDR);
    assert ( rc == 0);

    zmq_setsockopt (collector, ZMQ_SUBSCRIBE, "", 0 );

    sprintf (msg, "Collecting Messages\t@ %s", SERVER_COLLECTADDR);
    dbg_log (msg);

    while (!close_app) {
        rc = zmq_recv (collector, topic,  sizeof (topic), 0 );
        if (rc == -1) {
            perror("collector_thread()");
            break;
        }
        rc = zmq_recv (collector, buffer, sizeof (buffer), 0 );
        if (rc == -1) {
            perror("collector_thread()");
            break;
        }
        sprintf (msg, "Topic: %s, Message: %s", topic, buffer);
        dbg_log ( msg );
    }
    dbg_log ("Shutting down collector");
    zmq_close (collector);

    return (NULL);
}




void cleanup() {
    if (context) {
        zmq_ctx_term (context);
        dbg_log ("Completed shutting down ZMQ context");
        context = NULL;
    }
}


char *create_payload(const char *client_id, const char *session_id, const char *topic) {
    char *str = NULL;
    cJSON *payload = NULL;
    cJSON *clientId = NULL;
    cJSON *sessionId = NULL;
    cJSON *topicId = NULL;
    cJSON *action = NULL;
    cJSON *type = NULL;
    cJSON *uid = NULL;

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


    topicId = cJSON_CreateString (topic);
    if (topicId == NULL)
        goto end;
    cJSON_AddItemToObject(payload, "topicId", topicId);

    action = cJSON_CreateObject();
    if (action == NULL)
        goto end;
    cJSON_AddItemToObject(payload, "action", action);


    type = cJSON_CreateString("generate-passcode");
    if (type == NULL)
        goto end;
    cJSON_AddItemToObject(action, "type", type);

    uid = cJSON_CreateString("visitor-1");
    if (uid == NULL)
        goto end;
    cJSON_AddItemToObject(action, "uid", uid);

    str = cJSON_Print(payload);
    if (str == NULL)
        dbg_log ("Failed to print payload");

end:
    cJSON_Delete(payload);
    return str;
}
