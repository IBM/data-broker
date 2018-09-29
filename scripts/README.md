There are prerequisites for running redis in cluster mode.

Download URLs:

1. User must have ruby 2.3.0 or > installed (https://cache.ruby-lang.org/pub/ruby/2.3/ruby-2.3.6.tar.gz)

you can use rvm (ruby environment manager) to determine what versions of ruby are available on your system. (http://rvm.io/)

Ruby can be installed in users local directory

2. Redis 4.0.6 or > needs to be installed on system or in user directory
    - follow Redis readme for install instructions


Installation steps for Ruby
```
1. wget https://cache.ruby-lang.org/pub/ruby/2.3/ruby-2.3.6.tar.gz
2. gpg --keyserver hkp://keys.gnupg.net --recv-keys 409B6B1796C275462A1703113804BB82D39DC0E3 7D2BAF1CF37B13E2069D6956105BD0E739499BDB
3. \curl -sSL https://get.rvm.io | bash
4. source ~/.profile (just put in .bashrc)
5. rvm install ruby-2.3.6 --verify-downloads 2 --disable-binary
```

Installation of `redis-trib.rb` with password authentication

There is a unstable `redis-trib.rb` that includes password authentication. It has not been merged into Redis master github repo yet, but it can be obtained via wget or curl download

``` $ wget https://raw.githubusercontent.com/otherpirate/redis/0781f056b99fa88cb56861f9660bef4e2088d3ca/src/redis-trib.rb ```

It should replace `redis-trib.rb` in your <redis-<x.x.x>/src> directory 


## Deploying a Redis Cluster
First, install Redis gem for Ruby
``` 
$ gem install redis 
```
Then, the `run.sh` script in `lsf/` folder must be updated with the following info:
1. `REDISHOME` variable must point to Redis home directory
2. `PW` variable must be updated with the preferred password
3. `WORK_DIR` and `-cwd` must be updated with the preferred directory where to put logs and conf file generate during the Redis cluster deployment. The default path is the `lsf` directory. To change the path, both `WORK_DIR` variable and the `-cwd` bsub option should be updated accordingly.

## Save/Restore Redis RDB files
1.  Install the redis-rdb-tools
```
$ pip install git+https://github.com/lesandr/redis-rdb-tools@1f7bcf366073adf5510ad18f1efe0bf46ae5e0c1
```
2. `lsf/run.sh` will create a `redis-save.sh` script in the directory you execute from.
The script will save the data in the redis rdb files into a directory.

3. `lsf/run.sh` will create a `redis-import.sh` script in the directory you execute from. 
The script can be used to import the saved redis rdb files from step #2 into a new redis cluster. 
At this time the new cluster must be the same size as previous cluster for the import to work correctly.
