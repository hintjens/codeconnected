//  The Census Pattern
//  Model 1, over XPUB-XSUB

#include "czmq.h"

static void
counter_task (void *args, zctx_t *ctx, void *pipe)
{
    void *counter = zsocket_new (ctx, ZMQ_XPUB);
    zsocket_set_xpub_verbose (counter, 1);
    zsocket_bind (counter, "tcp://*:6001");

    //  .split census parameters
    //  The counter task is broken into two steps. First it allows
    //  all targets to get ready and raise their hands, using the
    //  Meerkat pattern. Then it sends out its census question and
    //  allows all targets time to reply:
    
    //  Parameters for the census
    int count_msec = 250;       //  Msecs to settle down
    int think_msec = 250;       //  Msecs for responses

    //  Calling thread tells us the population size
    char *population = zstr_recv (pipe);
    
    //  All activity happens on our counter socket
    zmq_pollitem_t items [] = { { counter, 0, ZMQ_POLLIN, 0 } };
    byte meerkat [] = { 1, 'M', 'e', 'e', 'r', 'k', 'a', 't' };
    
    //  .split census step 1
    //  Both steps are zmq_poll loops which exit either when we
    //  get the expected number of responses, or we time-out. In
    //  the first step we count only Meerkat subscriptions:
    int headcount = 0;         //  Known target size
    int64_t timer_end = zclock_time () + count_msec;
    int still_waiting = atoi (population);
    while (still_waiting) {
        int64_t time_left = timer_end - zclock_time ();
        if (time_left <= 0)
            break;              //  We're done here
        int rc = zmq_poll (items, 1, time_left * ZMQ_POLL_MSEC);
        if (rc == -1)
            break;              //  Interrupted

        if (items [0].revents & ZMQ_POLLIN) {
            zframe_t *frame = zframe_recv (counter);
            if (!frame)
                break;              //  Interrupted
            if (zframe_size (frame) == sizeof (meerkat)
            &&  memcmp (zframe_data (frame), meerkat,
                        sizeof (meerkat)) == 0) {
                still_waiting--;
                headcount++;
            }
            zframe_destroy (&frame);
        }
    }
    //  .split census step 2
    //  Now we've got our target population and we know they're
    //  subscribed, we send out the census question:
    zstr_send (counter, "Who wants pizza?");

    //  .split census step 3
    //  In the second poll loop, we wait for valid answers to our
    //  census question. We might still receive subscription
    //  messages so we have to discount those:
    int positives = 0;         //  How many said "yes"
    timer_end = zclock_time () + think_msec;
    still_waiting = headcount;
    while (still_waiting) {
        int64_t time_left = timer_end - zclock_time ();
        if (time_left <= 0)
            break;              //  We're done here
        int rc = zmq_poll (items, 1, time_left * ZMQ_POLL_MSEC);
        if (rc == -1)
            break;              //  Interrupted

        if (items [0].revents & ZMQ_POLLIN) {
            zframe_t *frame = zframe_recv (counter);
            if (!frame)
                break;              //  Interrupted
            byte *data = zframe_data (frame);
            //  Ignore any subscriptions we might still get
            if (data [0] > 1) {
                if (streq ((char *) data, "Yes"))
                    positives++;
                still_waiting--;
            }
            zframe_destroy (&frame);
        }
    }
    printf ("Out of %d people, %d want pizza\n", headcount, positives);
    zstr_send (pipe, "DONE");
}

//  .split target task
//  The target task starts by doing a Meerkat subscription, and then
//  subscribes to everything with a zero-sized subscription message.
//  It waits for the census question and answers Yes or No randomly:

static void
target_task (void *args, zctx_t *ctx, void *pipe)
{
    void *target = zsocket_new (ctx, ZMQ_XSUB);
    zsocket_connect (target, "tcp://localhost:6001");

    //  Tell publisher we're here
    byte meerkat [] = { 1, 'M', 'e', 'e', 'r', 'k', 'a', 't' };
    zmq_send (target, &meerkat, sizeof (meerkat), 0);
    //  Subscribe to everything as well (empty subscription)
    zmq_send (target, &meerkat, 1, 0);
    
    char *question = zstr_recv (target);
    char *answer = randof (2) == 0? "Yes": "No";
    printf ("%s %s\n", question, answer);
    free (question);
    zstr_send (target, answer);
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
