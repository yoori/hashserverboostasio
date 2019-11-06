Server for generate murmurv3 hash for input strings received by tcp connections (separated by \n)
Realized with using boost.asio
Performance properties : system cpu usage about 1.5 times more then cpu usage
On host with 8 process cores with 5 execution threads it give ~700 rps (input string length = 100 bytes)

Compiling (standart for cmake project):
mkdir build
cd build
cmake ..
make

Running:
./bin/BAHashServer -p <listen port> -t <number of execution threads>

