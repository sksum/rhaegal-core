## Steps:
- Update submodules libuv(event loop abstraction) and http_parser(parse requests)
- Install libuv and http_parser (method on their github) 
sh autogen.sh
./configure
make
make check
make install