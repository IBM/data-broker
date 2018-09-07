#!/bin/bash
 #
 # Copyright © 2018 IBM Corporation
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

set -e
export REPLICAS=0
export PW=foobared
export PORT=1601
export TIMEOUT=5000
# user specified env variablesj
export REDISHOME=$REDIS_HOME
export REDISDUMP=$RDB_HOME
export NODE_TIMEOUT=120
export nodes=4

export SCRIPT_DIR=`pwd`
export WORK_DIR=`pwd`

## Build script to start redis servers across nodes
## User replaces c699launch01 with the one on their system
cat  >redis-start.sh  << 'EOR'
LAUNCHNODE="c699launch01"
REDISHOME=$1
PW=$2
TIMEOUT=$3
PORT=$4
i=`hostname`
myhost=${i%%.*}
NUM=$(cat $HOSTS | sed /$LAUNCHNODE/d | sort -u | grep -n $myhost | cut -d: -f1)
myhost="$myhost-ib0"
echo $myhost

${REDISHOME}/bin/redis-server --requirepass $PW --bind $myhost   \
--port $PORT --cluster-enabled yes --cluster-config-file nodes-${NUM}:${PORT}.conf       \
--cluster-node-timeout $TIMEOUT --appendonly yes                                            \
--appendfilename appendonly-${NUM}:${PORT}.aof --dbfilename dump-${NUM}:${PORT}.rdb   \
--logfile ${NUM}:${PORT}.log --daemonize no
EOR
chmod +x redis-start.sh

echo "redis-start $REDISHOME $PW $TIMEOUT $PORT"

## Build script to submit redis-start.sh script
cat >batchjob  << 'EOF'
#BSUB -o %J.out
#BSUB -e %J.err
#BSUB -nnodes 4
#BSUB -q excl
#BSUB -W 4:00
#BSUB -cwd "" 
ulimit -s 10240

echo $LSB_JOBID > $WORK_DIR/lsbjobid
echo $LSB_DJOB_HOSTFILE > $WORK_DIR/lsbdjobhostfile
# could pass ENV vars in redis.conf file
/opt/ibm/spectrum_mpi/jsm_pmix/bin/jsrun --smpiargs “none” --rs_per_host 1 $SCRIPT_DIR/./redis-start.sh $REDISHOME $PW $TIMEOUT $PORT
EOF

# submit job
bsub < batchjob

until [ -e $WORK_DIR/lsbjobid ]; do
    echo "$WORK_DIR/lsbjobid  doesn't exist as of yet..."
    sleep 1
done
LSB_JOBID=`cat $WORK_DIR/lsbjobid`
until [ -e $WORK_DIR/lsbdjobhostfile ]; do
    echo "$WORK_DIR/lsbdjobhostfile  doesn't exist as of yet..."
    sleep 1
done
LSB_DJOB_HOSTFILE=`cat $WORK_DIR/lsbdjobhostfile`

waiting=true
while [ "$waiting" = true ] 
do
    state=$(bjobs $LSB_JOBID 2>&1)
    if [[ $state = *"RUN"* ]]; then
    ## create a redis-stop script so we can clean up the redis server instances
        echo "#!/bin/bash" >redis-shutdown.sh
        echo "#!/bin/bash" >redis-flushall.sh
        echo "#!/bin/bash" >redis-save.sh
        echo "#!/bin/bash" >redis-import.sh
        CLUSTER=""
        jobhosts=$(cat $LSB_DJOB_HOSTFILE | sed s/" "/"\n"/g | sort -u)
        splithosts=$(echo $jobhosts | sed s/$LAUNCHNODE//g)
        echo $splithosts
        cnt=1
        for i in `echo $splithosts`
        do
            myhost=${i%%.*}
            myhost="$myhost-ib0"
            echo "${REDISHOME}/bin/redis-cli -h $myhost -p $PORT -a $PW shutdown" >>redis-shutdown.sh
            echo "${REDISHOME}/bin/redis-cli -h $myhost -p $PORT -a $PW flushall" >>redis-flushall.sh
            echo "${REDISHOME}/bin/redis-cli -h $myhost -p $PORT -a $PW BGSAVE" >>redis-save.sh
            echo "${REDISHOME}/bin/rdb --command protocol ${REDISDUMP}/dump-${cnt}:${PORT}.rdb | ${REDISHOME}/bin/redis-cli -h $myhost -p $PORT -a $PW --pipe" >>redis-import.sh
            myip=`getent hosts $myhost | awk '{print $1}'`
            CLUSTER="$CLUSTER $myip:$PORT"
            cnt=$((cnt + 1))
        done
        chmod +x redis-shutdown.sh
        chmod +x redis-flushall.sh
        chmod +x redis-save.sh
        chmod +x redis-import.sh

        # could we execute redis-trib from one of hosts in $LSB_HOSTS
        noping=true
        while [ "$noping" = true ] 
        do
            cnt=0
            for i in `echo $splithosts`
            do
                myhost=${i%%.*}
                myhost="$myhost-ib0"
                rc=$(${REDISHOME}/bin/redis-cli -h $myhost -p $PORT -a $PW ping)  ||  rc="FAILED" 
                if [ $rc == "PONG" ]; then
                    cnt=$((cnt + 1)) 
                fi
            done
            echo $cnt
            echo "nodes"
            echo $nodes
            if [ $cnt -eq  $nodes ]; then
                noping=false
            fi
            sleep 2
        done

        ${REDISHOME}/src/redis-trib.rb create --password $PW  --replicas $REPLICAS $CLUSTER
        waiting=false
    fi
done


# safer to remove these so they won't accidentally be picked up by next job
rm  $WORK_DIR/lsbjobid
rm  $WORK_DIR/lsbdjobhostfile
