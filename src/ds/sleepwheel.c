#include "sleepwheel.h"
#include <string.h>


static inline void pick_bucket(uint64_t dt, int *level, uint32_t *slot){
    if (dt < LVL0_SIZE) {
        *level = 0;
        *slot = (uint32_t)dt & LVL0_MASK;
    } else if (dt < (1ULL << LVL2_SHIFT)) {
        *level = 1;
        *slot = (uint32_t)((dt >> LVL1_SHIFT) & LVL1_MASK);
    } else if (dt < (1ULL << LVL3_SHIFT)) {
        *level = 2;
        *slot = (uint32_t)((dt >> LVL2_SHIFT) & LVL2_MASK);
    } else {
        *level = 3;
        *slot = (uint32_t)((dt >> LVL3_SHIFT) & LVL3_MASK);
    }
}

void tw_init(timer_wheel_t *tw, uint64_t tick_ns, uint32_t max_concurrents){
    memset(tw, 0, sizeof(*tw));
    tw->tick_ns = tick_ns ? tick_ns : TW_TICK_NS;
    int total_buckets=  LVL0_SIZE + LVL1_SIZE + LVL2_SIZE + LVL3_SIZE;
    int avg_load = ceil_int_div(max_concurrents,total_buckets);
    for (int i = 0; i < LVL0_SIZE; i++) sleep_q_init(&tw->lvl0[i], avg_load);
    for (int i = 0; i < LVL1_SIZE; i++) sleep_q_init(&tw->lvl1[i], avg_load);
    for (int i = 0; i < LVL2_SIZE; i++) sleep_q_init(&tw->lvl2[i], avg_load);
    for (int i = 0; i < LVL3_SIZE; i++) sleep_q_init(&tw->lvl3[i], avg_load);
}

void tw_add(timer_wheel_t *tw, uint64_t delay_ns, void *napper){
    if (delay_ns == 0) delay_ns = 1;
    uint64_t dticks = (delay_ns + tw->tick_ns - 1) / tw->tick_ns;
    if (dticks == 0) dticks = 1;
    int level; uint32_t slot;
    pick_bucket(dticks, &level, &slot);
    sleep_entry_t ent = { .wake_me = tw->current_tick + dticks, .napper = napper };
    switch (level) {
    case 0: {
        uint32_t idx = (tw->current_tick + slot) & LVL0_MASK;
        sleep_q_enqueue(&tw->lvl0[idx], ent);
        break;
    }
    case 1:
        sleep_q_enqueue(&tw->lvl1[slot], ent);
        break;
    case 2:
        sleep_q_enqueue(&tw->lvl2[slot], ent);
        break;
    default:
        sleep_q_enqueue(&tw->lvl3[slot], ent);
        break;
    }
}

static void cascade(timer_wheel_t *tw, sleep_q *src, uint32_t shift){
    while (!sleep_q_is_empty(src)) {
        sleep_entry_t ent;
        sleep_q_dequeue(src, &ent);
        uint64_t remaining_ticks = (uint64_t)1 << shift;
        uint64_t delay_ns = remaining_ticks * tw->tick_ns;
        tw_add(tw, delay_ns, ent.napper);
    }
}

void tw_advance(timer_wheel_t *tw, uint64_t now_ns, tw_waker_t on_wake, void *user_ctx){
    uint64_t target_tick = now_ns / tw->tick_ns;
    while (tw->current_tick <= target_tick) {
        uint32_t idx0 = tw->current_tick & LVL0_MASK;
        sleep_q *bucket0 = &tw->lvl0[idx0];
        while (!sleep_q_is_empty(bucket0)) {
            sleep_entry_t ent;
            sleep_q_dequeue(bucket0, &ent);
            if (on_wake) on_wake(ent.napper, user_ctx);
        }
        if (idx0 == 0) {
            uint32_t idx1 = (tw->current_tick >> LVL1_SHIFT) & LVL1_MASK;
            if (idx1 == 0) {
                uint32_t idx2 = (tw->current_tick >> LVL2_SHIFT) & LVL2_MASK;
                if (idx2 == 0) {
                    uint32_t idx3 = (tw->current_tick >> LVL3_SHIFT) & LVL3_MASK;
                    cascade(tw, &tw->lvl3[idx3], LVL3_SHIFT);
                }
                cascade(tw, &tw->lvl2[idx2], LVL2_SHIFT);
            }
            cascade(tw, &tw->lvl1[idx1], LVL1_SHIFT);
        }
        tw->current_tick ++;
    }
}
