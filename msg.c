/* msg.c - keep track of a message */

#include "msg.h"

static char *msg = "default message";

void
set_message(char *m)
{
    msg = m;
}

char *
get_message(void)
{
    return msg;
}
