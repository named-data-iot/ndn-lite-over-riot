#include <stdio.h>
#include "xtimer.h"
#include "timex.h"

#include "ndn-lite/util/uniform-time.h"



ndn_time_ms_t ndn_time_now_ms(void){
   return xtimer_usec_from_ticks64(xtimer_now64()) / 1000;
    printf("slept until %" PRIu32 "\n", xtimer_usec_from_ticks(xtimer_now()));
}

ndn_time_us_t ndn_time_now_us(void){
return xtimer_usec_from_ticks64(xtimer_now64());
}

void ndn_time_delay(ndn_time_ms_t delay){
xtimer_usleep(delay * 1000);
}