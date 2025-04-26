#include "circq.h"
#include <stdint.h>
#include "../util/maths.h"

#ifndef TIMER_WHEEL_H
#define TIMER_WHEEL_H
typedef struct sleep_entry_t {
    uint64_t wake_me;   
    void    *napper;    
} sleep_entry_t;


DEFINE_CIRCULAR_QUEUE(sleep_entry_t, sleep_q);

#ifndef TW_TICK_NS                
#   define TW_TICK_NS 1000000ULL
#endif

#define LVL0_BITS   8   /* 256 slots  – 1 tick/slot  →   256 ticks span */
#define LVL1_BITS   6   /*  64 slots  – 256 ticks/slot → 16 384 ticks  */
#define LVL2_BITS   6   /*  64 slots  – 16 384/slot    →   1 M ticks  */
#define LVL3_BITS   6   /*  64 slots  – 1 M/slot       →  64 M ticks  */

#define LVL0_SIZE   (1U << LVL0_BITS)
#define LVL1_SIZE   (1U << LVL1_BITS)
#define LVL2_SIZE   (1U << LVL2_BITS)
#define LVL3_SIZE   (1U << LVL3_BITS)

#define LVL0_MASK   (LVL0_SIZE - 1)
#define LVL1_MASK   (LVL1_SIZE - 1)
#define LVL2_MASK   (LVL2_SIZE - 1)
#define LVL3_MASK   (LVL3_SIZE - 1)

#define LVL1_SHIFT  LVL0_BITS
#define LVL2_SHIFT  (LVL1_SHIFT + LVL1_BITS)
#define LVL3_SHIFT  (LVL2_SHIFT + LVL2_BITS)

typedef struct timer_wheel_t {
    sleep_q  lvl0[LVL0_SIZE];
    sleep_q  lvl1[LVL1_SIZE];
    sleep_q  lvl2[LVL2_SIZE];
    sleep_q  lvl3[LVL3_SIZE];
    uint64_t tick_ns;        
    uint64_t current_tick;  
} timer_wheel_t;
typedef void (*tw_waker_t)(void *napper, void *user_ctx);




void tw_init(timer_wheel_t *tw, uint64_t tick_ns, uint32_t max_concurrents);


void tw_add(timer_wheel_t *tw, uint64_t delay_ns, void *napper);


void tw_advance(timer_wheel_t *tw, uint64_t now_ns,tw_waker_t on_wake, void *user_ctx);



#endif
