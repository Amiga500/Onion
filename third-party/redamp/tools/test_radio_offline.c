#include "radio.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    radio_stream *r=radio_open("http://127.0.0.1:1/no-server");
    assert(r);
    unsigned char data[128];
    assert(radio_probe(r,data,16)==0);
    /* localhost:1 refuses TCP; curl reports 7 (connect), not DNS 6. */
    assert(radio_take_network_error()==2);
    radio_close(r);
    puts("Offline stream: first connection failure detected, no retry loop: PASS");
    return 0;
}
