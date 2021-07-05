#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "uv.h"
#include "utils.h"
// #include "llhttp.h"

#define N_BACKLOG 64
#define SENDBUF_SIZE 1024

typedef enum
{
    INITIAL_ACK,
    WAIT_FOR_MSG,
    IN_MSG
} ProcessingState;
typedef struct
{
    ProcessingState state;
    char sendbuf[SENDBUF_SIZE];
    int sendbuf_end;
    uv_tcp_t *client;
} peer_state_t;

void on_wrote_init_ack(uv_write_t *req, int status)
{
    if (status)
    {
        die("Write error: %s\n", uv_strerror(status));
    }
    peer_state_t *peerstate = (peer_state_t *)req->data;
    // Flip the peer state to WAIT_FOR_MSG, and start listening for incoming data
    // from this peer.
    peerstate->state = WAIT_FOR_MSG;
    peerstate->sendbuf_end = 0;

    int rc;
    if ((rc = uv_read_start((uv_stream_t *)peerstate->client, on_alloc_buffer,
                            on_peer_read)) < 0)
    {
        die("uv_read_start failed: %s", uv_strerror(rc));
    }

    // Note: the write request doesn't own the peer state, hence we only free the
    // request itself, not the state.
    free(req);
}

void on_peer_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf)
{
    if (nread < 0)
    {
        if (nread != uv_eof)
        {
            fprintf(stderr, "read error: %s\n", uv_strerror(nread));
        }
        uv_close((uv_handle_t *)client, on_client_closed);
    }
    else if (nread == 0)
    {
        // from the documentation of uv_read_cb: nread might be 0, which does not
        // indicate an error or eof. this is equivalent to eagain or ewouldblock
        // under read(2).
    }
    else
    {
        // nread > 0
        assert(buf->len >= nread);

        peer_state_t *peerstate = (peer_state_t *)client->data;
        if (peerstate->state == initial_ack)
        {
            // if the initial ack hasn't been sent for some reason, ignore whatever
            // the client sends in.
            free(buf->base);
            return;
        }

        // run the protocol state machine.
        for (int i = 0; i < nread; ++i)
        {
            switch (peerstate->state)
            {
            case initial_ack:
                assert(0 && "can't reach here");
                break;
            case wait_for_msg:
                if (buf->base[i] == '^')
                {
                    peerstate->state = in_msg;
                }
                break;
            case in_msg:
                if (buf->base[i] == '$')
                {
                    peerstate->state = wait_for_msg;
                }
                else
                {
                    assert(peerstate->sendbuf_end < sendbuf_size);
                    peerstate->sendbuf[peerstate->sendbuf_end++] = buf->base[i] + 1;
                }
                break;
            }
        }

        if (peerstate->sendbuf_end > 0)
        {
            // we have data to send. the write buffer will point to the buffer stored
            // in the peer state for this client.
            uv_buf_t writebuf =
                uv_buf_init(peerstate->sendbuf, peerstate->sendbuf_end);
            uv_write_t *writereq = (uv_write_t *)xmalloc(sizeof(*writereq));
            writereq->data = peerstate;
            int rc;
            if ((rc = uv_write(writereq, (uv_stream_t *)client, &writebuf, 1,
                               on_wrote_buf)) < 0)
            {
                die("uv_write failed: %s", uv_strerror(rc));
            }
        }
    }
    free(buf->base);
}

void on_peer_connected(uv_stream_t *server_stream, int status)
{
    printf("Connected my g !\n");
    if (status < 0)
        die("%s failed: %s", "Peer Connection", uv_strerror(status));

    // client will represent this peer; it's allocated on the heap and only
    // released when the client disconnects. The client holds a pointer to
    // peer_state_t in its data field; this peer state tracks the protocol state
    // with this client throughout interaction.
    uv_tcp_t *client = (uv_tcp_t *)xmalloc(sizeof(*client));
    status = uv_tcp_init(uv_default_loop(), client);
    if (status < 0)
        die("%s failed: %s", "uv_tcp_init", uv_strerror(status));

    client->data = NULL;
    if (uv_accept(server_stream, (uv_stream_t *)client) == 0)
    {
        struct sockaddr_storage peername;
        int namelen = sizeof(peername);
        status = uv_tcp_getpeername(client, (struct sockaddr *)&peername, &namelen);
        if (status < 0)
            die("%s failed: %s", "uv_tcp_getpeername", uv_strerror(status));
        report_peer_connected((const struct sockaddr_in *)&peername, namelen);

        // Initialize the peer state for a new client: we start by sending the peer
        // the initial '*' ack.
        peer_state_t *peerstate = (peer_state_t *)xmalloc(sizeof(*peerstate));
        peerstate->state = INITIAL_ACK;
        peerstate->sendbuf[0] = '*';
        peerstate->sendbuf_end = 1;
        peerstate->client = client;
        client->data = peerstate;

        // Enqueue the write request to send the ack; when it's done,
        // on_wrote_init_ack will be called. The peer state is passed to the write
        // request via the data pointer; the write request does not own this peer
        // state - it's owned by the client handle.
        uv_buf_t writebuf = uv_buf_init(peerstate->sendbuf, peerstate->sendbuf_end);
        uv_write_t *req = (uv_write_t *)xmalloc(sizeof(*req));
        req->data = peerstate;
        status = uv_write(req, (uv_stream_t *)client, &writebuf, 1, on_wrote_init_ack);
        if (status < 0)
            die("%s failed: %s", "uv_write", uv_strerror(status));
    }
    else
        uv_close((uv_handle_t *)client, on_client_closed);
}

int main(int argc, const char **argv)
{
    // print with no buffer
    setvbuf(stdout, NULL, _IONBF, 0);
    uv_loop_t *DEFAULT_LOOP = uv_default_loop();
    int portnum = 9090;
    if (argc >= 2)
    {
        portnum = atoi(argv[1]);
    }

    printf("Serving on port %d\n", portnum);

    uv_tcp_t server_stream;
    struct sockaddr_in server_address;
    // init default loop by libuv and start a server stream.
    int status = uv_tcp_init(DEFAULT_LOOP, &server_stream);
    printf("%d\n", status);
    if (status < 0)
        die("%s failed: %s", "uv_tcp_init", uv_strerror(status));
    printf("%d\n", status);

    // create a socket for the server to listen on
    status = uv_ip4_addr("0.0.0.0", portnum, &server_address);
    if (status < 0)
        die("%s failed: %s", "uv_ip4_addr", uv_strerror(status));
    printf("%d\n", status);

    // connect/bind the server on the socket
    status = uv_tcp_bind(&server_stream, (const struct sockaddr *)&server_address, 0);
    if (status < 0)
        die("%s failed: %s", "uv_tcp_bind", uv_strerror(status));
    printf("%d\n", status);

    //setup on connect callback and start listening on stream.
    status = uv_listen((uv_stream_t *)&server_stream, N_BACKLOG, on_peer_connected);
    if (status < 0)
        die("%s failed: %s", "uv_listen", uv_strerror(status));
    printf("%d\n", status);

    // Run the libuv event loop.
    uv_run(DEFAULT_LOOP, UV_RUN_DEFAULT);
    // If uv_run returned, close the default loop before exiting.
    return uv_loop_close(DEFAULT_LOOP);
}