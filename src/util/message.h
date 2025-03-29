#include "../ds/list.h"
#include "../ds/circq.h"
#include <liburing.h>
#include <time.h> 
typedef struct message_t{
    void * payload 
    size_t size;
    struct timespec send_time;
}message_t;

DEFINE_CIRCULAR_QUEUE(message_t , message_q);

bool inline send_msg(message_q q, message_t msg){
   return message_q_enqueue(q, msg);
}
bool inline get_msg(message_q q, message_t msg){
    return message_q_dequeue(q, msg);
}