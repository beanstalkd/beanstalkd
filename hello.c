#include <stdio.h>
#include "msg.h"

int
main() {
    set_message("hello, world");
    printf("%s\n", get_message());
    return 0;
}
