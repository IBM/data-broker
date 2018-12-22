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
export TIMEOUT=<value in milliseconds>
export REDISHOME=$HOME
export REDISDUMP=<path to rdb files that you would like to save/restore>
export LAUNCHNODE=<name of your launch node>
export WORKDIR=<path to the working directory>
export nodes=<number of nodes in cluster>

## Build script to start redis servers across nodes
cat  >redis-start.sh  << 'EOR'
LAUNCHNODE=<name of launch node>
REDISHOME=$1
PW=$2
TIMEOUT=$3
PORT=$4
HOSTS=$5
i=`hostname`
myhost=${i%%.*}
NUM=$(cat $HOSTS | sed /$LAUNCHNODE/d | sort -u | grep -n $myhost | cut -d: -f1)
myhost="$myhost-ib0"
echo $myhost $NUM
((NUM=NUM-1))


${REDISHOME}/bin/redis-server --requirepass $PW --bind $myhost   \
--port $PORT --cluster-enabled yes --cluster-config-file nodes-${NUM}:${PORT}.conf       \
--cluster-node-timeout $TIMEOUT --appendonly yes                                            \
--appendfilename appendonly-${NUM}:${PORT}.aof --dbfilename dump-${NUM}:${PORT}.rdb   \
--logfile ${NUM}:${PORT}.log --daemonize no
EOR
chmod +x redis-start.sh

cat  >saverdb.sh  << 'EOS'
HOST=$1
REDISHOME=$2
PW=$3
PORT=$4
RC=`${REDISHOME}/bin/redis-cli -h $HOST  -p $PORT -a $PW  LASTSAVE`
${REDISHOME}/bin/redis-cli -h $HOST  -p $PORT -a $PW BGSAVE
LASTSAVE=$RC
echo $LASTSAVE
until [ $RC != $LASTSAVE ];
do
 LASTSAVE=`${REDISHOME}/bin/redis-cli -h $HOST -p $PORT -a $PW  LASTSAVE`;
done
echo "RC=$RC;LASTSAVE=$LASTSAVE"
exit

EOS
chmod +x saverdb.sh

## Build script to submit redis-start.sh script
cat >batchjob  << 'EOF'
#BSUB -o %J.out
#BSUB -e %J.err
#BSUB -nnodes 6
#BSUB -q excl
#BSUB -W 30
#BSUB -cwd `pwd`
ulimit -s 10240


echo $LSB_JOBID > lsbjobid
echo $LSB_DJOB_HOSTFILE > lsbdjobhostfile
# could pass ENV vars in redis.conf file
/opt/ibm/spectrum_mpi/jsm_pmix/bin/jsrun --smpiargs “none” --rs_per_host 1 ./redis-start.sh ${REDISHOME} $PW $TIMEOUT $PORT $LSB_DJOB_HOSTFILE
EOF

# submit job
bsub < batchjob

until [ -e lsbjobid ]; do
    echo "lsbjobid  doesn't exist as of yet..."
    sleep 1
done
LSB_JOBID=`cat lsbjobid`
until [ -e lsbdjobhostfile ]; do
    echo "lsbdjobhostfile  doesn't exist as of yet..."
    sleep 1
done
LSB_DJOB_HOSTFILE=`cat lsbdjobhostfile`

waiting=true
while [ "$waiting" = true ] 
do
    state=$(bjobs $LSB_JOBID 2>&1)
    if [[ $state = *"RUN"* ]]; then
    ## create a redis-stop script so we can clean up the redis server instances
        echo "#!/bin/bash" >redis-shutdown.sh
        echo "#!/bin/bash" >redis-deleteall.sh
        echo "#!/bin/bash" >redis-save-rdb.sh
        echo "#!/bin/bash" >redis-restore-rdb.sh
        echo "#!/bin/bash" >redis-restore-aof.sh
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
            echo "${REDISHOME}/bin/redis-cli -h $myhost -p $PORT -a $PW flushall" >>redis-deleteall.sh
            savehosts="$savehosts$myhost,"
            myip=`getent hosts $myhost | awk '{print $1}'`
            CLUSTER="$CLUSTER $myip:$PORT"
            cnt=$((cnt + 1))
        done
        
        savehosts=${savehosts%?}
        thehost='\`hostname\`'
        echo "pdsh -w $USER@$savehosts ${WORKDIR}/saverdb.sh %h ${REDISHOME} $PW $PORT" >>redis-save-rdb.sh
        echo "pdsh -w $USER@$savehosts ${REDISHOME}/bin/rdb '--command protocol ${REDISDUMP}/dump-%n:${PORT}.rdb | ${REDISHOME}/bin/redis-cli -h %h  -p $PORT -a $PW --pipe'" >>redis-restore-rdb.sh
        echo "pdsh -w $USER@$savehosts cat '${REDISDUMP}/appendonly-%n:${PORT}.aof | ${REDISHOME}/bin/redis-cli -h %h  -p $PORT -a $PW'" >>redis-restore-aof.sh
        chmod +x redis-shutdown.sh
        chmod +x redis-deleteall.sh
        chmod +x redis-save-rdb.sh
        chmod +x redis-restore-rdb.sh
        chmod +x redis-restore-aof.sh

        # confirm that all redis-servers are communicating before creating cluster
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

        ${REDISHOME}/bin/redis-cli --cluster create -a $PW --cluster-replicas $REPLICAS $CLUSTER
        waiting=false
    fi
done


# safer to remove these so they won't accidentally be picked up by next job
rm  lsbjobid
rm  lsbdjobhostfile
