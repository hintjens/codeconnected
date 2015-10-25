//  Minimal WTFP server in 0MQ
#include "czmq.h"

static void *
wtfp_server (void *args)
{
    zctx_t *ctx = zctx_new ();
    void *router = zsocket_new (ctx, ZMQ_ROUTER);
    int rc = zsocket_bind (router, "tcp://*:8080");
    assert (rc != -1);

    while (true) {
        //  Get WTFP request
        zframe_t *handle = zframe_recv (router);
        if (!handle)
            break;          //  Ctrl-C interrupt
        char *request = zstr_recv (router);
        puts (request);     //  Professional Logging(TM)
        free (request);     //  We throw this away
        //  Send Hello World response
        zframe_send (&handle, router, ZFRAME_MORE);
        zstr_send (router, "Hello, World!");
    }
    zctx_destroy (&ctx);
    return NULL;
}

int main (void)
{
    zthread_new (wtfp_server, NULL);

    zctx_t *ctx = zctx_new ();
    void *dealer = zsocket_new (ctx, ZMQ_DEALER);
    int rc = zsocket_connect (dealer, "tcp://localhost:8080");
    assert (rc != -1);

    zstr_send (dealer, "GET /Hello");
    char *response = zstr_recv (dealer);
    puts (response);

    return 0;
}
