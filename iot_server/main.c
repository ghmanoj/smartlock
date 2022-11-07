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



void dbg_log ( const char *message )
{
    fprintf ( stdout, "LOG[INFO]: %s\n", message );
    fflush ( stdout );
}


const char PUBLISH_TOPIC[]   = "cmd/series100/group-3/lock-1/";
const char SERVER_PUBADDR[]  = "tcp://*:9091";


int pubthread_arg   = 0;
bool close_app      = false;

pthread_t pubthread_id;


/* ZMQ context */
void *context;


/* prototypes */
void cleanup();
void *publisher_thread();
void generate_json_str(char *buffer);

char *create_monitor(void);

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

    if (pthread_create (&pubthread_id, NULL, (void*)publisher_thread, &pubthread_arg) != 0) {
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

    sprintf (msg, "Publishing @ %s", SERVER_PUBADDR);
    dbg_log (msg);

    uint32_t count = 0;

    while (!close_app) {
        // generate_json_str ( buffer );

        char *data = create_monitor();
        if (data) {
            zmq_send ( publisher, PUBLISH_TOPIC, strlen (PUBLISH_TOPIC), ZMQ_SNDMORE );
            zmq_send ( publisher, data, strlen ( data ), ZMQ_DONTWAIT );
            free ( data );
        }

        sleep ( 1 );
    }

    dbg_log ("Shutting down publisher");
    zmq_close (publisher);

    return (NULL);
}


void cleanup() {
    if (context) {
        zmq_ctx_term (context);
        dbg_log ("Completed shutting down ZMQ context");

        context = NULL;
    }
}

void generate_json_str (char *buffer) {

    // sprintf ( 
    //     buffer, 
    //     "%s", // pattern
    //     "mobile-1",
    //     "0193-0428",
    //     "cmd/series100/mobile-1/res",
    //     "generate-passcode",
    //     "visitor-1"
    // );


/*
    typedef struct 
    {
        char type[32];
        char uid[16];
        
    } smartlock_action_t;   

    typedef struct
    {
        char code[8];

    } smartlock_res_t;

    typedef struct 
    {
        char client_id[32];
        char session_id[16];
        char topic[64];
        smartlock_action_t action;

    } smartlock_sub_t ;

    typedef struct 
    {
        char client_id[32];
        char session_id[16];
        char passcode[64];
        char ttl[16];
        smartlock_res_t result;

    } smartlock_pub_t;
*/
}



//create a monitor with a list of supported resolutions
//NOTE: Returns a heap allocated string, you are required to free it after use.
// https://github.com/DaveGamble/cJSON
char *create_monitor(void)
{
    const unsigned int resolution_numbers[3][2] = {
        {1280, 720},
        {1920, 1080},
        {3840, 2160}
    };
    char *string = NULL;
    cJSON *name = NULL;
    cJSON *resolutions = NULL;
    cJSON *resolution = NULL;
    cJSON *width = NULL;
    cJSON *height = NULL;
    size_t index = 0;

    cJSON *monitor = cJSON_CreateObject();
    if (monitor == NULL)
    {
        goto end;
    }

    name = cJSON_CreateString("Awesome 4K");
    if (name == NULL)
    {
        goto end;
    }
    /* after creation was successful, immediately add it to the monitor,
     * thereby transferring ownership of the pointer to it */
    cJSON_AddItemToObject(monitor, "name", name);

    resolutions = cJSON_CreateArray();
    if (resolutions == NULL)
    {
        goto end;
    }
    cJSON_AddItemToObject(monitor, "resolutions", resolutions);

    for (index = 0; index < (sizeof(resolution_numbers) / (2 * sizeof(int))); ++index)
    {
        resolution = cJSON_CreateObject();
        if (resolution == NULL)
        {
            goto end;
        }
        cJSON_AddItemToArray(resolutions, resolution);

        width = cJSON_CreateNumber(resolution_numbers[index][0]);
        if (width == NULL)
        {
            goto end;
        }
        cJSON_AddItemToObject(resolution, "width", width);

        height = cJSON_CreateNumber(resolution_numbers[index][1]);
        if (height == NULL)
        {
            goto end;
        }
        cJSON_AddItemToObject(resolution, "height", height);
    }

    string = cJSON_Print(monitor);
    if (string == NULL)
    {
        fprintf(stderr, "Failed to print monitor.\n");
    }

end:
    cJSON_Delete(monitor);
    return string;
}