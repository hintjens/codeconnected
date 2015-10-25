void *publisher = zsocket_new (ctx, ZMQ_XPUB);
zsocket_set_xpub_verbose (publisher, 1);
zsocket_bind (publisher, "tcp://*:6001");
