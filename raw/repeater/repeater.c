//  The Repeater Pattern - example 1

#include "czmq.h"

static void
weather_mediator (void *args, zctx_t *ctx, void *pipe)
{
    void *mediator = zsocket_new (ctx, ZMQ_XPUB);
    zsocket_bind (mediator, "tcp://*:6001");

    while (true) {
        zframe_t *frame = zframe_recv (mediator);
        if (!frame)
            break;
        if (zframe_data (frame)[0] > 1) 
            zframe_send (&frame, mediator, 0);
        else
            zframe_destroy (&frame);
    }
}

static void
weather_station (void *args, zctx_t *ctx, void *pipe)
{
    void *station = zsocket_new (ctx, ZMQ_XSUB);
    assert (station);
    zsocket_connect (station, "tcp://localhost:6001");
    srandom ((unsigned) time (NULL));
    long count = 0;
    while (true) {
        int zipcode, temperature, relhumidity;
        zipcode = randof (10000);
        temperature = randof (215) - 80;
        relhumidity = randof (50) + 10;
        int rc = zstr_send (station, "%05d %d %d",
                            zipcode, temperature, relhumidity);
        count++;
        if (rc) {
            printf ("%ld sent\n", count);
            break;
        }
    }
}

static void
weather_client (void *args, zctx_t *ctx, void *pipe)
{
    void *client = zsocket_new (ctx, ZMQ_XSUB);
    assert (client);
    zsocket_connect (client, "tcp://localhost:6001");
    byte subscribe [] = { 1, 0, 0, 0, 0, 0, 0 };
    sprintf ((char *) subscribe + 1, "%05d", randof (10000));
    zmq_send (client, &subscribe, 6, 0);
    
    int updates = 0;
    long total_temp = 0;
    while (updates < 100) {
        char *message = zstr_recv (client);
        if (!message)
            break;
        int zipcode, temperature, relhumidity;
        sscanf (message, "%d %d %d",
            &zipcode, &temperature, &relhumidity);
        total_temp += temperature;
        free (message);
        updates++;
    }
    zstr_send (pipe, "zipcode=%s avg=%dF",
        (char *) subscribe + 1, (int) (total_temp / updates));
}

int main (void)
{
    zctx_t *ctx = zctx_new ();
    zthread_fork (ctx, weather_mediator, NULL);
    zthread_fork (ctx, weather_station, NULL);
    void *pipe1 = zthread_fork (ctx, weather_client, NULL);
    void *pipe2 = zthread_fork (ctx, weather_client, NULL);

    //  Get reports from the two clients
    char *report = zstr_recv (pipe1);
    if (report) {
        puts (report);
        free (report);
    }
    report = zstr_recv (pipe2);
    if (report) {
        puts (report);
        free (report);
    }
    //  We're done, terminate
    zctx_destroy (&ctx);
    return 0;
}
