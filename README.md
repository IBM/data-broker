
# Data Broker

The Data Broker (DBR) is a distributed, in-memory container for named
tuples enabling applications in a workflow to exchange data through
one or more shared namespaces.  The API is inspired by the Linda
coordination and communication model and provides blocking or
non-blocking semantics.  The architecture is split into client and
system library to enable easy adoption of new backing storages via
additional system libraries.  The default Data Broker system library
(or backend) shipped with this version uses Redis (single node or
cluster mode) as the backing store.


## 1 Build of lib and test requires cmake 2.8+.

The Data Broker library (Redis backend) uses libevent2 to manage network connections
to the nodes of a Redis cluster. To build, you'll need the development package with the
header and the libevent2 library. To run, you'll only need the libevent2 library.

cmake uses an out-of-source build tree and in order to build it, it's
best to:

a) create a build directory
```
mkdir build
```
b) switch to that dir
```
cd build
```
c) run cmake:
```
cmake <source_dir> [options]
``` 
with options:
```
-DCMAKE_INSTALL_PREFIX=<path>     set the install path (default: /usr/local)
-DAPPLE=1                         when building for MAC OS
-DDEFAULT_BE=<backend-path-name>  what backend to link by default (default: redis)
```     
example:
when run from the created 'build' dir and to prepare installation in `/opt/databroker`:
```
cmake ../ -DCMAKE_INSTALL_PREFIX=/opt/databroker
```
d) run make
```
make
```
e) install
```
make install
```


## 2 Setting up Redis:

A Redis out-of-the box will almost do the trick.  What's needed in
addition to the basic config is to enable password authorization. Edit
the redis.conf

requirepass <areallylongandcrypticpassphrase>

The password can (and should) be a nice and long cryptic string. You
won't have to type it. (see below)


When setting up Redis, it is suggested to follow general
recommendations to harden and secure a Redis service.

Setting up Redis cluster is more tricky with authorization enabled.
Most important to make it as easy as possible: all instances need to
use the same password.  One way (not the only) to create the cluster
is to omit the password in the beginning, then use the recommended
`redis-trib.rb` script to create and configure the cluster. After it's
set up, you'd add the password configuration to the config file of
each instance and restart.  Newer versions of redis-trib (since
version 4.0.6) allow to provide the password via command line so
there's likely no need for this workaround.

Further details are beyond the scope of this README and can be found
here:  https://redis.io/topics/cluster-tutorial

Recommendation for the debugging phase: use a shorter passphrase if you
plan to use redis-cli to check stored content, you need to authenticate
too.


## 3 Databroker library

### 3.1 Building your client

After running make install in the Data Broker build directory, you
should have a directory with include file and libraries located in
the place you specified with the cmake command (or under `/usr/local`).



### 3.2 Running your client

The Data Broker library is controlled by a number of environment
variables:

DBR_SERVER
      Points to one of the Redis instances that are part of the
      backend.  If not set, it defaults to 'localhost'.

DBR_PORT
      Points to the TCP port of the Redis instance specified by
      DBR_SERVER.  If not set, it defaults to '6379'.

DBR_AUTHFILE
      Point the library to the location of the file that contains the
      Redis passphrase. Make sure to restrict the access to that file
      to the intended group of people who need access to Redis.  If
      not set, it defaults to '.redis.auth' (note this is pointing to
      the current work dir). For better security, it is suggested to
      specify an absolute path.

DBR_TIMEOUT
      Specifies the timeout in seconds for blocking get and read API
      calls. If not set, it defaults to 5 seconds.




## 4 Limitations:

Note: this is a very early development version of the Data Broker
library and it comes with many limitations that will be improved over
time. Here's a brief list of current limits. For a more complete list,
see the issue tracking.

- Each Redis connection uses a fixed limited size data buffer for
  sending and receiving. It is currently limited to 128 MB. This means,
  a tuple plus associated data, cannot exceed the size of 128 MB.  (it's
  a little less than that because there's a small protocol overhead) A
  non-buffering protocol is in the plan, but not yet implemented.

- The persistence levels and group settings have no effect yet.
  Subject to future work.

- There are many cases with a lack of robustness. Applications
  currently need to behave well especially in the context of name
  space creation and deletion. For example, all clients need to have
  called detach before the last one can call delete. Otherwise the delete
  will fail and (if not retried by the app) data will remain undeleted.
  For now you should consider attach-detach and create-delete pairs as
  opening and closing brackets with create-delete being the outermost bracket.

- The size for tuple names or keys is limited to 1024 characters
- The size for namespace names is limited to 1023 characters
- The number of namespaces that can be attached to a single process
  at a time is limited to 1024


## 5 Bindings:

The Data Broker provides a Python interface located into the bindings folder.
For more information, please refer to its [README](bindings/python).
