//  Minimal HTTP server in 0MQ
#include "czmq.h"

int main (void)
{
    zctx_t *ctx = zctx_new ();
    void *router = zsocket_new (ctx, ZMQ_ROUTER);
    zsocket_set_router_raw (router, 1);
    int rc = zsocket_bind (router, "tcp://*:8080");
    assert (rc != -1);
    
    while (true) {
        //  Get HTTP request
        zframe_t *handle = zframe_recv (router);
        if (!handle)
            break;          //  Ctrl-C interrupt
        char *request = zstr_recv (router);
        puts (request);     //  Professional Logging(TM)
        free (request);     //  We throw this away

        //  Send Hello World response
        zframe_send (&handle, router, ZFRAME_MORE + ZFRAME_REUSE);
        zstr_send (router,
            "HTTP/1.0 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "\r\n"
            "Hello, World!");

        //  Close connection to browser
        zframe_send (&handle, router, ZFRAME_MORE);
        zmq_send (router, NULL, 0, 0);
    }
    zctx_destroy (&ctx);
    return 0;
}
