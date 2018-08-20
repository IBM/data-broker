# Python Interface to the Data Broker

This interface is implemented on top of CFFI and provides a Python API for developing applications to be deployed on the Data Broker.

It requires CFFI to be installed on the system. This requirement is checked during the building phase.

Please refer to http://cffi.readthedocs.io/en/latest/installation.html for CFFI installation details.
It is recommended to use a Python virtual environment if no root access is available.

## How to build

To enable the PyDBR interface building, enable the `-DPYDBR=true` option to cmake when building the Data Broker.

After the building completes, set the path to the databroker libraries into the LD_LIBRARY_PATH.
```
$ export LD_LIBRARY_PATH=path/to/databroker/shared/object:$LD_LIBRARY_PATH
```

### Standalone building
Alternatively, the PyDBR interface can be built and installed in a second moment directly in the `bindings/python` directory without re-building the Data Broker.

```
$ mkdir build
$ cd build
$ cmake ..
$ make
$ make install
```

## Run an example
In the `bindings/python/test` directory are present some example applications.
Assuming a Data Broker instance is running (for example a Redis instance), then run an example as the following

```
$ cd test 
$ python <example>.py
```

## Writing an application
Using the API is straightforward: the Data Broker modules must be imported
```
from _dbr_interface import ffi
from dbr_module import dbr
```
Functions can be accessed from the `dbr` module and the Data Broker types can be allocated with the `ffi` module.
For instance:
```
dbr_name = "DBRtestname"
groups = ffi.new('DBR_GroupList_t','0')
dbr_hdl = ffi.new('DBR_Handle_t*')
level = dbr.DBR_PERST_VOLATILE_SIMPLE

# Create the Data Broker
dbr_hdl = dbr.dbrCreate(dbr_name, level, groups)
```
The following tables report all the currently supported functions, according to the Data Broker API.

### Troubleshooting
It is preferable to build and install the binding within a Python virtual environment, in case no sudo privileges are available. The installation makes the `_dbr_interface` available without the need of specifying the include path in the source code.
Otherwise it is possible to specify the path to the `dbr_module` directory by writing the following in the Python application before the import statements:

```
sys.path.insert(0, os.path.abspath('path/to/dbr_module'))
```



**Data Broker Management Functions**

|Name | Description|
| ------------- |:-------------|
| dbrCreate() | Create a new Data Broker|
| dbrDelete() | Delete an existing Data Broker|
| dbrAttach() | Attach to an existing Data Broker|
| dbrDetach() | Detach from an existing Data Broker|
| dbrQuery()  | Query information about an existing Data Broker|
| dbrTest()   | Test the status of an asynchronous call |
| dbrCancel() | Cancel an asynchronous call |

**Data Broker Access Functions**

| Name | Description|
| ------------- |:-------------|
| dbrPut()  | Insert a tuple in the Data Broker|
| dbrRead() | Read a tuple in the Data Broker|
| dbrGet()  | Pop a tuple in the Data Broker |
| dbrReadA()| Read a tuple in the Data Broker, non blocking|
| dbrPutA() | Put a tuple in the Data Broker, non blocking|
| dbrGetA() | Pop a tuple in the Data Broker, non blocking|
| dbrMove() | Move a tuple from a source namespace to a destination namespace|

**Data Broker and CFFI**

| Name | Description|
| ------------- |:-------------|
|createBuf(buftype, bufsize) | Create a buffer with type `buftype` and size `bufsize`. The buffer will be passed to DBR access functions|
|getErrorCode(return_value) | Returns the error code from a return value|
|getErrorMessage(return_value) | Returns a description of the error returned by a dbr function call|
