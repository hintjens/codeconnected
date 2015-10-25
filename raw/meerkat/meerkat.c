//  The Meerkat Pattern
//
//  In which we address the slow subscriber problem by asking for
//  a show of hands before we start publishing.

#include "czmq.h"

static void
publisher_task (void *args, zctx_t *ctx, void *pipe)
{
    zctx_set_linger (ctx, 1000);
    void *publisher = zsocket_new (ctx, ZMQ_XPUB);
    zsocket_set_xpub_verbose (publisher, 1);
    zsocket_bind (publisher, "tcp://*:6001");

    //  Calling thread tells us the population size
    char *population = zstr_recv (pipe);
    int still_waiting = atoi (population);
    byte meerkat [] = { 1, 'M', 'e', 'e', 'r', 'k', 'a', 't' };
    
    while (still_waiting) {
        zframe_t *frame = zframe_recv (publisher);
        if (!frame)
            break;              //  Interrupted
        if (zframe_size (frame) == sizeof (meerkat)
        &&  memcmp (zframe_data (frame), meerkat, sizeof (meerkat)) == 0)
            still_waiting--;
        //  Dump the frame contents for the benefit of the reader
        zframe_print (frame, NULL);
        zframe_destroy (&frame);
    }
    //  Now broadcast our message
    zstr_send (publisher, "Hello, World!");
    zclock_sleep (250);
    zstr_send (pipe, "DONE");
}

//  .split subscriber task
//  The subscriber sends a "Meerkat" subscription and then
//  any other subscriptions it wants:

static void
subscriber_task (void *args, zctx_t *ctx, void *pipe)
{
    void *subscriber = zsocket_new (ctx, ZMQ_XSUB);
    zsocket_connect (subscriber, "tcp://localhost:6001");

    //  Tell publisher we're here
    byte meerkat [] = { 1, 'M', 'e', 'e', 'r', 'k', 'a', 't' };
    zmq_send (subscriber, &meerkat, sizeof (meerkat), 0);
    //  Subscribe to everything as well (empty subscription)
    zmq_send (subscriber, &meerkat, 1, 0);
    
    char *hello = zstr_recv (subscriber);
    puts (hello);
    free (hello);
}

//  .split main task
//  The main task starts publisher task and then the subscribers:

int main (void)
{
    zctx_t *ctx = zctx_new ();

    //  Size of target population
    srand ((unsigned) time (NULL));
    int population = randof (10) + 1;
    
    //  Start publisher task
    void *pipe = zthread_fork (ctx, publisher_task, NULL);
    zstr_send (pipe, "%d", population);
    
    //  Start target population
    while (population--)
        zthread_fork (ctx, subscriber_task, NULL);

    //  Wait for publisher to complete
    free (zstr_recv (pipe));

    zctx_destroy (&ctx);
    return 0;
}
