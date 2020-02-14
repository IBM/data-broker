
# Data Broker Utilities

The Data Broker Utilities contain a set of small test programs and
benchmarks to play with the Data Broker as well as data adapter
examples to plug into the data stream of an application.


## perftest

Contains single-node and parallel benchmarks and stress test tools for
the data broker.

## data_adapter

Contains a few examples for data adapter libraries that can be plugged
into the data path of a Data Broker client. Data adapters allow to
parse and modify the keys or values of put, read, and get requests.
They also allow to split a request into multiple chained requests of
the same command.  The hello example splits the input data in half and
creates 2 requests with a '0' and '1' appended to the original key to
demonstrate this concept.  Data adapters enable many data
transformations like filtering, conversion, compression, or
encryption.



## build

 * cmake build process, requires data broker library installed and
   being pointed to via
     `cmake <...> CMAKE_FIND_ROOT_PATH=<path to dbr install>`


 * note that the build process only includes the perftests and
   data adapters.

 * note that if no MPI library is found, cmake prints a warning but
   will not attempt to build the MPI-based tests.


## usage

 * perftest has single and parallel benchmark tests. Running with
   `-h` will provide additional help info.

 * long_random_parallel can be used as a (parallel) stress test or to
   fill the backend with random data or just flood the data broker
   with a mix of put/read/get requests.. It's not measuring
   performance.
