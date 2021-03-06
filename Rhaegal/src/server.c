/*
Saksham Mrig 2021
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "uv.h"
#include "utils.h"
#include "../../http-parser/http_parser.h"

#define SAMPLE_RESPONSE            \
    "HTTP/1.1 200 OK\r\n"          \
    "Content-Type: text/plain\r\n" \
    "Content-Length: 14\r\n"       \
    "\r\n"                         \
    "Hello, World!\n"

/* types */
uv_tcp_t GLOBAL_HANDLE;

typedef struct
{
    uv_tcp_t handle;
    http_parser parser;
} client_t;

#define N_BACKLOG 64
typedef enum
{
    INITIAL_ACK,
    WAIT_FOR_MSG,
    IN_MSG
} ProcessingState;

#define SENDBUF_SIZE 1024
static http_parser_settings parser_settings;
http_parser GLOBAL_PARSER;
typedef struct
{
    ProcessingState state;
    char sendbuf[SENDBUF_SIZE];
    int sendbuf_end;
    uv_tcp_t *client;
} peer_state_t;

void on_alloc_buffer(uv_handle_t *handle, size_t suggested_size,
                     uv_buf_t *buf)
{
    buf->base = (char *)xmalloc(suggested_size);
    buf->len = suggested_size;
}

void on_client_closed(uv_handle_t *handle)
{
    uv_tcp_t *client = (uv_tcp_t *)handle;

    if (client->data)
    {
        free(client->data);
    }
    free(client);
}

void on_wrote_buf(uv_write_t *req, int status)
{
    if (status)
    {
        die("Write error: %s\n", uv_strerror(status));
    }
    peer_state_t *peerstate = (peer_state_t *)req->data;

    if (peerstate->sendbuf_end >= 3 &&
        peerstate->sendbuf[peerstate->sendbuf_end - 3] == 'X' &&
        peerstate->sendbuf[peerstate->sendbuf_end - 2] == 'Y' &&
        peerstate->sendbuf[peerstate->sendbuf_end - 1] == 'Z')
    {
        free(peerstate);
        free(req);
        uv_stop(uv_default_loop());
        return;
    }

    peerstate->sendbuf_end = 0;
    free(req);
}
int headers_complete_cb(http_parser *parser)
{

    uv_write_t *write_req = malloc(sizeof(uv_write_t));
    uv_buf_t buf = uv_buf_init(SAMPLE_RESPONSE, sizeof(SAMPLE_RESPONSE));
    int r = uv_write(write_req, (uv_stream_t *)&GLOBAL_HANDLE, &buf, 1, on_wrote_buf);
    assert(r == 0);

    return 1;
}

void on_peer_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf)
{
    if (nread < 0)
    {
        if (nread != UV_EOF)
        {
            fprintf(stderr, "Read error: %s\n", uv_strerror(nread));
        }
        uv_close((uv_handle_t *)client, on_client_closed);
    }
    else if (nread == 0)
    {
        // donothin http://docs.libuv.org/en/v1.x/stream.html?highlight=uv_read_Start#c.uv_read_cb
    }
    else
    {
        // nread > 0
        assert(buf->len >= nread);

        peer_state_t *peerstate = (peer_state_t *)client->data;
        if (peerstate->state == INITIAL_ACK)
        {
            free(buf->base);
            return;
        }

        // // Run the protocol state machine.
        // for (int i = 0; i < nread; ++i)
        // {
        //     switch (peerstate->state)
        //     {
        //     case INITIAL_ACK:
        //         assert(0 && "can't reach here");
        //         break;
        //     case WAIT_FOR_MSG:
        //         if (buf->base[i] == '^')
        //         {
        //             peerstate->state = IN_MSG;
        //         }
        //         break;
        //     case IN_MSG:
        //         if (buf->base[i] == '$')
        //         {
        //             peerstate->state = WAIT_FOR_MSG;
        //         }
        //         else
        //         {
        //             assert(peerstate->sendbuf_end < SENDBUF_SIZE);
        //             peerstate->sendbuf[peerstate->sendbuf_end++] = buf->base[i] + 1;
        //         }
        //         break;
        //     }
        // }
        size_t parsed = http_parser_execute(&GLOBAL_PARSER, &parser_settings, buf->base, nread);

        if (peerstate->sendbuf_end > 0)
        {

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

        /*

            ALl of this can be modified to send and resivec appropriate mesages
            for libtorch / or any other form of processing.



        */
    }
    free(buf->base);
}

void on_wrote_init_ack(uv_write_t *req, int status)
{
    if (status)
    {
        die("Write error: %s\n", uv_strerror(status));
    }
    peer_state_t *peerstate = (peer_state_t *)req->data;

    peerstate->state = WAIT_FOR_MSG;
    peerstate->sendbuf_end = 0;

    if ((status = uv_read_start((uv_stream_t *)peerstate->client, on_alloc_buffer,
                                on_peer_read)) < 0)
    {
        die("uv_read_start failed: %s", uv_strerror(status));
    }

    free(req);
}

void on_peer_connected(uv_stream_t *server_stream, int status)
{
    if (status < 0)
    {
        fprintf(stderr, "Peer connection error: %s\n", uv_strerror(status));
        return;
    }

    uv_tcp_t *client = (uv_tcp_t *)xmalloc(sizeof(*client));
    status = uv_tcp_init(uv_default_loop(), client);
    if (status < 0)
    {
        die("uv_tcp_init failed: %s", uv_strerror(status));
    }
    client->data = NULL;

    if (uv_accept(server_stream, (uv_stream_t *)client) == 0)
    {
        struct sockaddr_storage peername;
        int namelen = sizeof(peername);
        if ((status = uv_tcp_getpeername(client, (struct sockaddr *)&peername,
                                         &namelen)) < 0)
        {
            die("uv_tcp_getpeername failed: %s", uv_strerror(status));
        }
        report_peer_connected((const struct sockaddr_in *)&peername, namelen);

        peer_state_t *peerstate = (peer_state_t *)xmalloc(sizeof(*peerstate));
        peerstate->state = INITIAL_ACK;
        peerstate->sendbuf[0] = '*';
        peerstate->sendbuf_end = 1;
        peerstate->client = client;
        client->data = peerstate;

        uv_buf_t writebuf = uv_buf_init(peerstate->sendbuf, peerstate->sendbuf_end);
        uv_write_t *req = (uv_write_t *)xmalloc(sizeof(*req));
        req->data = peerstate;
        if ((status = uv_write(req, (uv_stream_t *)client, &writebuf, 1,
                               on_wrote_init_ack)) < 0)
        {
            die("uv_write failed: %s", uv_strerror(status));
        }
        http_parser_init(&GLOBAL_PARSER, HTTP_REQUEST);
        GLOBAL_PARSER.data = client;
    }
    else
    {
        uv_close((uv_handle_t *)client, on_client_closed);
    }
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
    GLOBAL_HANDLE = server_stream;

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

    // just running stuff, innit.
    uv_run(DEFAULT_LOOP, UV_RUN_DEFAULT);

    return uv_loop_close(DEFAULT_LOOP);
}