# server-c

This is an implementation of a simple C server from scratch, using https://eli.thegreenplace.net/2017/concurrent-servers-part-1-introduction/ as a template.
This repo is divided based on different parts of the blog :
- sequential servers
- threaded both one_thread_per_client and using a thread_pool
- event driven 
- libuv