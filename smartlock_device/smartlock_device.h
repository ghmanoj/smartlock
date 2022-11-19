#ifndef __SMARTLOCK_DEVICE_INCLUDED__
#define __SMARTLOCK_DEVICE_INCLUDED__

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

#endif