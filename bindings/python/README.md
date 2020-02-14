# Python Interface to the Data Broker

This interface is implemented on top of CFFI and provides a Python API for developing applications to be deployed on the Data Broker. It is built for Python3.x, but previous versions are still compatible. A check on the Python Interpreter and Libraries will notify the user whether the requirements are satisfied or not.

It requires CFFI to be installed on the system. This requirement is checked during the building phase.
CFFI requires Python Development packages are installed on the system.

Please refer to http://cffi.readthedocs.io/en/latest/installation.html for CFFI installation details.
It is recommended to use a Python virtual environment if no root access is available.


## How to build

To enable the PyDBR interface building, enable the `-DPYDBR=1` option to cmake when building the Data Broker.

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
In the `bindings/python/example` directory are present some example applications.
Assuming a Data Broker instance is running (for example a Redis instance), then run an example as the following

```
$ cd example 
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
group_list = ffi.new('DBR_GroupList_t')
dbr_hdl = ffi.new('DBR_Handle_t*')
level = dbr.DBR_PERST_VOLATILE_SIMPLE

# Create a Data Broker instance
dbr_hdl = dbr.create(dbr_name, level, group_list)
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
| ---------- |:-------------|
| create(dbrname, level, groups):dbrhandle   | Create a new Data Broker|
| delete(dbrname):exitstatus   | Delete an existing Data Broker|
| attach(dbrname):dbrhandle   | Attach to an existing Data Broker|
| detach(dbrhandle):exitstatus   | Detach from an existing Data Broker|
| query(dbr_handle, dbr_state, state_mask):exitstatus    | Query information about an existing Data Broker|
| test(tag):exitstatus     | Test the status of an asynchronous call |
| cancel(tag):exitstatus  | Cancel an asynchronous call |

**Data Broker Access Functions**

| Name | Description|
| ---------- |:-------------|
| put(dbr_hdl, tuple_val, tuple_name, group):exitstatus      | Insert a tuple in the Data Broker|
| read(dbr_hdl, tuple_name, match_template, group, flag[, buffer_size=None]):(tuple, exitstatus)     | Read a tuple from the Data Broker|
| get(dbr_hdl, tuple_name, match_template, group, flag[, buffer_size=None]):(tuple, exitstatus)      | Pop a tuple from the Data Broker |
| readA(dbr_hdl, tuple_name, match_template, group[, flag=DBR_FLAGS_NONE, buffer_size=None]):(tag, futuretuple)    | Read a tuple from the Data Broker, non blocking|
| putA(dbr_hdl, tuple_val, tuple_name, group):tag    | Put a tuple in the Data Broker, non blocking|
| getA(dbr_hdl, tuple_name, match_template, group[, flag=DBR_FLAGS_NONE, buffer_size=None]):(tag, futuretuple)     | Pop a tuple from the Data Broker, non blocking|
| move(src_DBRHandle, src_group, tuple_name, match_template, dest_DBRHandle, dest_group):exitstatus     | Move a tuple from a source namespace to a destination namespace|
| testKey(dbr_hdl, tuple_name):exitstatus | Checks if a tuple, identified by its name/key, exists in the namespace|
| directory(dbr_hdl, match_template, group, count, size):(keyslist, size, exitstatus)| Get a list of available tuple names of a namespace filtered by the user-provided pattern|
| iterator(dbr_hdl, iterator, match_template, group):(key, iterator)| Iterate over the available tuple names filtered by the user-provided pattern|

**Data Broker and CFFI**

| Name | Description|
| ------------- |:-------------|
|createBuf(buftype, bufsize):cffibuffer | Create a buffer with type `buftype` and size `bufsize`. The buffer will be passed to DBR access functions|
|getErrorCode(exitstatus):errorcode | Returns the error code from a return value|
|getErrorMessage(exitstatus):description | Returns a description of the error returned by a dbr function call|
