#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <stdio.h>

void dbg_log ( const char *message )
{
    fprintf ( stdout, "LOG[INFO]: %s\n", message );
    fflush ( stdout );
}


#endif 