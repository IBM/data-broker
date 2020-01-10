#!/usr/bin/env python
# -*- coding: cp1252 -*-
#
# Copyright (C) 2019 IBM Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
from __future__ import print_function
import sys, getopt
import os, socket
import pickle
import time
import signal
import argparse
from subprocess import Popen, PIPE, call
from collections import OrderedDict
#
# global variables
# user should customize these global variables to their own installations needs
launchnode = "c699launch01"
cwd = "./"
os.chdir(cwd)
redisclient = "redis-cli"
redisserver = "redis-server"
rdb = "rdb"
rdir = ""
netif = "ib0"
# redis configuration values
# user should customize these values to their own needs
pw = "foobared"
replicas = 0
timeout = 150000
port = 1601
rs_per = 1
nnodes = 3
jobid = -1
hosts=[]
#
#
# function definitions
class outoftime(Exception):
	pass
#
def deadline(outoftime, *args):
	def decorate(f):
		def handler(signum, frame):
			raise outoftime
#
		def new_f(*args):
			signal.signal(signal.SIGALRM, handler)
			signal.alarm(outoftime)
			return f(*args)
			signal.alarm(0)
#
		new_f.__name__ = f.__name__
		return new_f
	return decorate
#
def getipaddr(hosts):
	global rs_per
	hostlist = []
	for i in range(len(hosts)):
		for j in range(rs_per):
			nexthost = hosts[i]+'-'+netif
			addr=socket.gethostbyname(nexthost)
			nexthost = addr
			hostlist.append(nexthost)
#
	return hostlist
#
def gethost():
	global hosts
#
	hostlist = hosts.split()
#
	myhost=socket.gethostbyaddr(socket.gethostname())[0]
	head, sep, tail = myhost.partition('.')
	if (head in hostlist):
		index = hostlist.index(head)
	else:
		index=-1
	myhost = head+'-'+netif
	return myhost,index
#
def gethosts(jobid):
	args=['-o','-noheader','exec_host', '%d' % jobid]
	cmd=['bjobs'] + args
	print(cmd)
	p = Popen(cmd, stdout=PIPE, stderr=PIPE, stdin=PIPE)
	stdout, stderr = p.communicate()
	if stderr:
		print('gethosts error: %s' % stderr.decode())
		exit()
	hosts = stdout.decode().split(':')
	hosts = hosts[1:]
	for i in range(len(hosts)):
		head, sep, tail = hosts[i].partition('*')
		hosts[i] = tail
		hosts[i] = hosts[i].replace('\n','')
	return hosts
#
def serversrunning(hosts):
	global rs_per
	global pw
	global port
	global redisclient
	hostlist = []
#
	hostlist = getipaddr(hosts)
#
	for h in hostlist:
		for j in range(rs_per):
			cmd = [redisclient]
			args = '--no-auth-warning -h %s -p %s -a %s ping' % (h, port+j, pw)
			args = args.split(' ')
			cmd = cmd + args
			noping = True
			while noping == True:
				p = Popen(cmd, stdin=PIPE, stderr=PIPE, stdout=PIPE)
				stdout, stderr = p.communicate()
				if stderr:
					print('>>> serversrunning stderr:')
					print(stderr.decode())
				if stdout.decode().strip() != 'PONG':
					print('>>> serversrunning stdout:')
					print(stdout.decode())
				else:
					print('>>> PONG %s:%s' % (h, port+j))
					noping = False

def createcluster(hosts):
	global pw
	global replicas
	cnt = 0
	hostlist = []
	print('num of hosts %d' % len(hosts))
	print('num of tasks  %d' % rs_per)
	for i in range(len(hosts)):
		for j in range(rs_per):
			nexthost = hosts[i]+'-'+netif
			addr=socket.gethostbyname(nexthost)
			nexthost = addr  + ':' + str(port+j)
			hostlist.append(nexthost)
			cnt = cnt + 1
	hostlist = " ".join(str(x) for x in hostlist)
	cmd=[redisclient]
	args="--no-auth-warning --cluster create -a %s --cluster-replicas %d %s" % (pw, 0, hostlist)
	args=args.split(' ')
	cmd = cmd + args
	print('cmd to create cluster:%s' % str(cmd))
	print('Waiting for cluster creation')
	p = Popen(cmd, stdout=PIPE, stderr=PIPE, stdin=PIPE)
	p.stdin.write(b'yes\n')
	stdout, stderr = p.communicate()
	print('stdout: %s' % stdout.decode())
	print('stderr: %s' % stderr.decode())
#
#
def startservers(options):
	print('start Redis servers')
#
	global hosts
	global rs_per
	global rdir
#
	rs_per = options.rs_per_node
	rdir = options.rdir
	daemonize = options.daemonize
#
	# get all the hosts lsf allocated
	h = os.environ.get('LSB_DJOB_HOSTFILE')
	print ('hostfile=%s' % h)
	with open(h) as f: hosts = f.read()
	hosts = hosts.replace(launchnode,'')
	hosts="\n".join(list(OrderedDict.fromkeys(hosts.split("\n"))))
	print('hosts: %s' % hosts)
#
	cmd = ['jsrun']
	args = ['--rs_per_host=%d' % rs_per,
	'./dbr_utils.py',
	'startserver',
	'-m %s' % hosts,
	'-d %s' % rdir]
	if daemonize:
		args += ['--daemonize']
	cmd = cmd + args
	print('jsrun cmd:%s' % cmd)
#
	# start the redis server on the host
	p = Popen(cmd, stdout=PIPE, stderr=PIPE, stdin=PIPE)
	stdout, stderr = p.communicate()
	print('startserver output: %s' % stdout.decode())
	print('startserver error: %s' % stderr.decode())
	if stderr:
		exit(1)
#
def startserver(options):
	global pw
	global port
	global timeout
	global hosts
	global rdir
#
	hosts = options.hostlist
	rdir = options.rdir
	daemonize = options.daemonize
#
	print ('start a Redis server')
	rank = int(os.environ.get('JSM_NAMESPACE_LOCAL_RANK'))
#
	# get the hostname of the node starting the redis-server
	myhost,index = gethost()
#
	print('myhost:%s' % myhost)
	print('rank:%d' % rank)
	print('index:%d' % index)
	print('timeout:%d' % timeout)
	print('pw:%s' % pw)
	print('rdir:%s' % rdir)
	cmd=[redisserver]
	print('redisserver:%s' % cmd)
	args = ['--requirepass %s' % pw,
	'--bind %s' % myhost,
	'--port %d' % (port+rank),
	'--cluster-enabled yes',
	'--cluster-config-file nodes-%d-%d.conf' % (index, rank),
	'--dir %s' % rdir,
	'--dbfilename dump-%d-%d.rdb' % (index, rank),
	'--logfile log-%d-%d.log' % (index, rank),
	'--daemonize %s' % ("yes" if daemonize else "no"),
	'--cluster-node-timeout %d' %  timeout]
	cmd = cmd + args
	print('server cmd:%s' % cmd)
	p = Popen(cmd, stdout=PIPE, stderr=PIPE, stdin=PIPE)
	stdout, stderr = p.communicate()
	if stderr:
		print('redis server error:%s' % stderr.decode())
		exit(-1)
#
import time
def start(options):
	global rs_per
	global nnodes
	global rdir
	print('start Redis server')
#
	rs_per = options.rs_per_node
	nnodes = options.nodes
	rdir = options.rdir
	cmd =[]
	cmd = ['bsub',
	'-nnodes %d' % nnodes,
	'-o%J.out',
	'-e%J.err',
	'-q excl',
	'-W 30']
#
	args = ['./dbr_utils.py',
	'startservers',
	'-n %d' % nnodes,
	'-r %d' % rs_per,
	'-d %s' % rdir]
	cmd = cmd + args
	p = Popen(cmd, stdout=PIPE, stderr=PIPE, stdin=PIPE)
	stdout, stderr = p.communicate()
	if not stderr:
		jobid=stdout.decode()
		jobid = jobid.split()
		jobid = jobid[1]
		jobid = jobid.replace('<','')
		jobid = jobid.replace('>','')
		jobid = int(jobid)
	else:
		print('startservers stderr: %s' % stderr)
		exit(-1)
#
	# once job is started we can proceed to cluster creation
	cmd = ['bjobs']
	args = ['-o','-noheader','stat','%d' % jobid]
	cmd = cmd + args
	backoff = 1
	while True:
		p = Popen(cmd, stdout=PIPE, stderr=PIPE, stdin=PIPE)
		stdout,stderr = p.communicate()

		print('Waiting for job to run')
		if stdout.decode().strip('\n') == 'RUN':
			print('Running job')
			break
		else:
			print('Job not running: backing off %d secs' % backoff)
			time.sleep(backoff)
			backoff *= 2
#
	hosts = gethosts(jobid)
	# wait for servers to all start running
	serversrunning(hosts)
#
	createcluster(hosts)
#
def start_allocated(options):
	print('start Redis server on allocted nodes')

	options.rs_per_node = 1
	options.daemonize = True
	startservers(options)

	### wait for connections
	with open(os.environ.get('LSB_DJOB_HOSTFILE')) as f:
		hostset = set([x.strip() for x in f.readlines()])
	hostset.discard(launchnode)
	hosts = list(hostset)

	print(">>> pingpong hosts: %s" % hosts)

	serversrunning(hosts)

	createcluster(hosts)

def gettaskspernode(jobid):
	cmd = ['bjobs']
	args = ['-l',
	'%d' % options.jobid]
	cmd = cmd + args
	p = Popen(cmd, stdout=PIPE,stderr=PIPE,stdin=PIPE)
	stdout,stderr = p.communicate()
	if stderr:
		print(stderr.decode())
		exit(-1)
#
	index = stdout.decode().find('-r')
	mylist = stdout.decode().replace('"','')
	mylist = mylist.replace(',','')
	mylist = mylist.replace('<','')
	mylist = mylist.replace('>','')
#
	if index > 0:
		mylist = mylist.split()
		for i in mylist:
			if i == '-r':
				j=mylist.index(i)
				rs_per_host = int(mylist[j+1])
	return rs_per_host
#
#
#
def delete():
	print('flush Redis cluster')
#
def saverdb(options):
	global port
	global pw
	global hosts
#
	rs_per = options.rs_per_node
	hosts = options.hostlist
#
	myhost,index = gethost()
#
	lastsave = 'LASTSAVE'
	bgsave = 'BGSAVE'
#
	returns = []
	lastsave = []
#
	for i in range(rs_per):
		cmd = [redisclient]
		args = '-h %s -p %d -a %s LASTSAVE' % (myhost, port+i, pw)
		args = args.split()
		cmd = cmd + args
		p = Popen(cmd, stderr=PIPE, stdout=PIPE, stdin=PIPE)
		stdout, stderr = p.communicate()
		print(stdout.decode().replace('\n',''))
		returns.append(stdout.decode().replace('\n',''))
		lastsave.append(returns[i])
		cmd = [redisclient]
		args = '-h %s -p %d -a %s BGSAVE' % (myhost, port+i, pw)
		args = args.split()
		cmd = cmd + args
		print(cmd)
		p = Popen(cmd, stderr=PIPE, stdout=PIPE, stdin=PIPE)
		stdout, stderr = p.communicate()
#
#
	waiting = True
	i = 0
	while waiting:
		waiting = True
		for i in range(rs_per):
			cmd = [redisclient]
			args = '--no-auth-warning -h %s -p %d -a %s LASTSAVE' % (myhost, port+i, pw)
			args = args.split()
			cmd = cmd + args
			p = Popen(cmd, stderr=PIPE, stdout=PIPE, stdin=PIPE)
			stdout, stderr = p.communicate()
			if stdout.decode().replace('\n','') != returns[i]:
				waiting = False
def save(options):
	jobid = options.jobid
	rs_per = gettaskspernode(jobid)
	print('save  Redis rdb files')
	hostlist = []
	hosts = gethosts(jobid)
	for x in hosts:
		hostlist.append(x+'-'+netif)
	hostlist = " ".join(str(x) for x in hostlist)
	hostlist = hostlist.replace(' ',',')
#
	hosts = ' '.join(word for word in hosts)
	cmd = ['pdsh']
	args = ['-w %s' % hostlist,
			'%s/dbr_utils.py' % cwd,
			'saverdb',
			'-r %d' % rs_per,
			'-m "%s"' % hosts]
	cmd = cmd + args
	print(cmd)
	p = Popen(cmd, stderr=PIPE, stdout=PIPE, stdin=PIPE)
	stdout, stderr = p.communicate()
	print('stdout:%s' % stdout.decode())
	print('stderr:%s' % stderr.decode())
#
def shutdown(options):
	global port
	global pw
	global rs_per
	global hosts
#
	rs_per = options.rs_per_node
	hosts = options.hostlist
#
	myhost,index = gethost()
#
	for i in range(rs_per):
		cmd = [redisclient]
		args = '--no-auth-warning -h %s -p %d -a %s SHUTDOWN' % (myhost, port+i, pw)
		args = args.split()
		cmd = cmd + args
		p = Popen(cmd, stderr=PIPE, stdout=PIPE, stdin=PIPE)
		stdout, stderr = p.communicate()
		print(stdout.decode().replace('\n',''))
#
def stop(options):
	jobid = options.jobid
	rs_per = gettaskspernode(jobid)
#
#
	hostlist = []
	print('stop Redis cluster')
	hosts = gethosts(jobid)
	for x in hosts:
		hostlist.append(x+'-'+netif)
#
	hostlist = " ".join(str(x) for x in hostlist)
	hostlist = hostlist.replace(' ',',')
#
	hosts = ' '.join(word for word in hosts)
	cmd = ['pdsh']
	args = ['-w %s' % hostlist,
			'%s/dbr_utils.py' % cwd,
			'shutdown',
			'-m "%s"' % hosts,
			'-r %d' % rs_per]
	cmd = cmd + args
	print(cmd)
	p = Popen(cmd, stdin=PIPE, stderr=PIPE, stdout=PIPE)
	stdout, stderr = p.communicate()
	print('stdout:%s' % stdout.decode())
	print('stderr:%s' % stderr.decode())
#
#
def stop_allocated(options):

	# get all the hosts lsf allocated
	h = os.environ.get('LSB_DJOB_HOSTFILE')
	print ('hostfile=%s' % h)
	with open(h) as f: hostlist = [x.strip() for x in f.readlines()]
	hostlist.remove(launchnode)
	hostlist = list(set(hostlist))

	cmd = ['pdsh']
	args = ['-w %s' % ",".join(hostlist),
			'%s/dbr_utils.py' % options.rdir,
			'shutdown',
			'-m "%s"' % "\n".join(hostlist)]
	cmd = cmd + args
	print(cmd)
	p = Popen(cmd, stdin=PIPE, stderr=PIPE, stdout=PIPE)
	stdout, stderr = p.communicate()
	print('stdout:%s' % stdout.decode())
	print('stderr:%s' % stderr.decode())
#
#
def getrdb(options):
	global port
	global pw
	global rdir
	global hosts
#
	rs_per = options.rs_per_node
	rdir = options.rdir
	hosts = options.hostlist
#
	myhost,index = gethost()
#
	for i in range(rs_per):
		cmd = [rdb]
		args = '--c protocol %s/dump-%d-%d.rdb' % (rdir, index, i)
		args = args.split()
		cmd = cmd + args
		p0 = Popen(cmd, stderr=PIPE, stdout=PIPE, stdin=PIPE)
		print('p0 output:%s' % p0.stdout.decode())
		print('p0 stderr:%s' % p0.stderr.decode())
		if p0.stdout:
			cmd = [redisclient]
			args = '-h %s -p %d -a %s --pipe' % (myhost, port+i, pw)
			args = args.split()
			cmd = cmd + args
			p1 = Popen(cmd, stdout=PIPE, stderr=PIPE, stdin=p0.stdout)
			p1.communicate()
			print('p1 output:%s' % p1.stdout.decode())
			print('p1 stderr:%s' % p1.stderr.decode())
		else:
			print('no additional data to restore')
#
def restore(options):
	print('restore  Redis rdb files')
	global jobid
	global rs_per
#
	jobid = options.jobid
	rs_per = gettaskspernode(jobid)
	rdir = options.rdir
#
	print('restore  Redis rdb files')
	hostlist = []
	hosts = gethosts(jobid)
	for x in hosts:
		hostlist.append(x+'-'+netif)
#
	hostlist = " ".join(str(x) for x in hostlist)
	hostlist = hostlist.replace(' ',',')
#
	hosts = ' '.join(word for word in hosts)
#
	cmd = ['pdsh']
	args = ['-w %s' % hostlist,
			'%s/dbr_utils.py' % cwd,
			'getrdb',
			'-m "%s"' % hosts,
			'-r %d' % rs_per,
			'-d %s' % rdir]
	cmd = cmd + args
	print(cmd)
	p = Popen(cmd, stderr=PIPE, stdout=PIPE, stdin=PIPE)
	stdout, stderr = p.communicate()
	print('stdout:%s' % stdout.decode())
	print('stderr:%s' % stderr.decode())


# argument processing
print('Script for starting, stoping, saving, and restoring a Redis cluster')
print('Users should modify script to customize launch node, current working directory and path to redis binaries')
parser = argparse.ArgumentParser(description='start/stop/save/restore Redis clusters')
subparsers = parser.add_subparsers(metavar='{start,stop,save,restore,}',help='sub-command help')
start_parser = subparsers.add_parser('start', help='start -n <nodecnt> -r <servers per node>')
start_parser.set_defaults(func=start)
start_parser.add_argument('-n', '--nodes', type=int, nargs='?', const=3, default=3, help='number of nodes to start servers on')
start_parser.add_argument('-r', '--rs_per_node', type=int, nargs='?', const=1, default=1, help='number of servers per node')
start_parser.add_argument('-d', '--rdir', nargs='?', const="./", default="./", help='rdirectory for rdb files')
start_allocated_parser = subparsers.add_parser('start_allocated', help='start_allocated -r <servers per node>')
start_allocated_parser.set_defaults(func=start_allocated)
start_allocated_parser.add_argument('-d', '--rdir', nargs='?', const="./", default="./", help='rdirectory for rdb files')
stop_parser = subparsers.add_parser('stop', help='stop -j <jobid>')
stop_parser.set_defaults(func=stop)
stop_parser.add_argument('-j', '--jobid', type=int, help='jobid of cluster to stop')
stop_allocated_parser = subparsers.add_parser('stop_allocated', help='stop_allocated -r <servers per node>')
stop_allocated_parser.set_defaults(func=stop_allocated)
stop_allocated_parser.add_argument('-d', '--rdir', nargs='?', const="./", default="./", help='rdirectory for rdb files')
shutdown_parser = subparsers.add_parser('shutdown', help=argparse.SUPPRESS)
shutdown_parser.set_defaults(func=shutdown)
shutdown_parser.add_argument('-j', '--jobid', type=int, help='jobid of cluster to stop')
shutdown_parser.add_argument('-r', '--rs_per_node', type=int, nargs='?', const=1, default=1, help='number of servers per node')
shutdown_parser.add_argument('-m', '--hostlist', type=str)
save_parser = subparsers.add_parser('save', help='-j <jobid>')
save_parser.set_defaults(func=save)
save_parser.add_argument('-j', '--jobid', type=int, help='jobid of cluster to save')
saverdb_parser = subparsers.add_parser('saverdb', help=argparse.SUPPRESS)
saverdb_parser.set_defaults(func=saverdb)
saverdb_parser.add_argument('-r', '--rs_per_node', type=int, nargs='?', const=1, default=1, help='number of servers per node')
saverdb_parser.add_argument('-m', '--hostlist', type=str)
getrdb_parser = subparsers.add_parser('getrdb', help=argparse.SUPPRESS)
getrdb_parser.set_defaults(func=getrdb)
getrdb_parser.add_argument('-r', '--rs_per_node', type=int, nargs='?', const=1, default=1, help='number of servers per node')
getrdb_parser.add_argument('-d', '--rdir', nargs='?', const='./', default='./', help='directory for rdb files')
getrdb_parser.add_argument('-m', '--hostlist', type=str)
restore_parser = subparsers.add_parser('restore', help='-j <jobid>, -d <rdb file directory')
restore_parser.set_defaults(func=restore)
restore_parser.add_argument('-j', '--jobid', type=int, help='jobid of cluster to restore to')
restore_parser.add_argument('-d', '--rdir', nargs='?', const='./', default='./', help='directory for rdb files')
startserver_parser = subparsers.add_parser('startserver', help=argparse.SUPPRESS)
startserver_parser.set_defaults(func=startserver)
startserver_parser.add_argument('-m', '--hostlist', type=str)
startserver_parser.add_argument('-d', '--rdir', nargs='?', const="./", default="./", help='rdirectory for rdb files')
startserver_parser.add_argument('--daemonize', action='store_true', help='launch Redis in daemonize mode')
startservers_parser = subparsers.add_parser('startservers', help=argparse.SUPPRESS)
startservers_parser.set_defaults(func=startservers)
startservers_parser.add_argument('-n', '--nodes', type=int, nargs='?', const=1, default=1, help='number of nodes to start servers on')
startservers_parser.add_argument('-r', '--rs_per_node', type=int, nargs='?', const=1, default=1, help='number of servers per node')
startservers_parser.add_argument('-d', '--rdir', nargs='?', const="./", default="./", help='rdirectory for rdb files')
startservers_parser.add_argument('--daemonize', action='store_true', help='launch Redis in daemonize mode')
#
if len(sys.argv) <= 1:
    sys.argv.append('--help')
#
options = parser.parse_args()
print(options)
#
# Run the appropriate function (in this case showtop20 or listapps)
options.func(options)
