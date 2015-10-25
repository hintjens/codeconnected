//  The Census Pattern
//  Model 2, over ROUTER-DEALER

#include "czmq.h"

static void
counter_task (void *args, zctx_t *ctx, void *pipe)
{
    void *counter = zsocket_new (ctx, ZMQ_ROUTER);
    zsocket_bind (counter, "tcp://*:6001");

    //  Parameters for the census
    int census_msec = 250;       //  Msecs to settle down

    //  Calling thread tells us the population size
    char *population = zstr_recv (pipe);
    
    //  All activity happens on our counter socket
    zmq_pollitem_t items [] = { { counter, 0, ZMQ_POLLIN, 0 } };

    int headcount = 0;         //  Known target size
    int positives = 0;         //  How many said "yes"
    
    int64_t timer_end = zclock_time () + census_msec;
    int still_waiting = atoi (population);
    while (still_waiting) {
        int64_t time_left = timer_end - zclock_time ();
        if (time_left <= 0)
            break;              //  We're done here
        int rc = zmq_poll (items, 1, time_left * ZMQ_POLL_MSEC);
        if (rc == -1)
            break;              //  Interrupted

        if (items [0].revents & ZMQ_POLLIN) {
            zframe_t *address = zframe_recv (counter);
            char *message = zstr_recv (counter);
            if (streq (message, "Hello")) {
                headcount++;
                zframe_send (&address, counter, ZFRAME_MORE);
                zstr_send (counter, "Who wants pizza?");
            }
            else
            if (streq (message, "Yes"))
                positives++;
            
            zframe_destroy (&address);
            free (message);
        }
    }
    printf ("Out of %d people, %d want pizza\n", headcount, positives);
    zstr_send (pipe, "DONE");
}

//  .split target task
//  The target task starts by saying Hello, then it waits for the
//  census question and answers Yes or No randomly:

static void
target_task (void *args, zctx_t *ctx, void *pipe)
{
    void *subscriber = zsocket_new (ctx, ZMQ_DEALER);
    zsocket_connect (subscriber, "tcp://localhost:6001");

    zstr_send (subscriber, "Hello");
    char *question = zstr_recv (subscriber);
    char *answer = randof (2) == 0? "Yes": "No";
    printf ("%s %s\n", question, answer);
    free (question);
    zstr_send (subscriber, answer);
}

//  .split main thread
//  The main task starts a counter task and a set of target tasks:

int main (void)
{
    zctx_t *ctx = zctx_new ();
    
    //  Size of target population
    srand ((unsigned) time (NULL));
    int population = randof (10) + 1;

    //  Start counter task
    void *pipe = zthread_fork (ctx, counter_task, NULL);
    zstr_send (pipe, "%d", population);

    //  Start target population
    while (population--)
        zthread_fork (ctx, target_task, NULL);

    //  Wait for census to complete
    free (zstr_recv (pipe));
    
    zctx_destroy (&ctx);
    return 0;
}
