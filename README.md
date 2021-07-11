# Rhaegal
This is a concurrent, lightweight, multi-threaded webserver made in C using inspiration from https://eli.thegreenplace.net/2017/concurrent-servers-part-1-introduction/ and https://github.com/davidmoreno/onion 
This repo is divided based on different parts of the blog :
- Tutorial
    - Sequential
    - Threaded
    - Event driven 
- Rhaegal 
    - demo
        - api,models,src
    - server.c 


## The idea for the project is to build a webserver/plugin that connects your c/c++ codebase to a static front-end and we can benefit from compilers like SvelteKit/Sapper as well as the speed and scalibility of a C webserver.

A great example of this can be a front-end interface for a MNIST categorizer made using libtorch.

## Models taken up from https://github.com/prabhuomkar/pytorch-cpp/tree/master/tutorials/basics 
## Production env will essentially require models/, demo_src/__sapper__/export and server.c file

