#!/usr/bin/env bash

echo $0
prevdir=`pwd`;
scriptdir=`dirname $0`;
cd $scriptdir;
scriptdir=`pwd`

if [ "$#" -ne 1 ]; then
	echo "need to provde the sql script to run"
	exit -1
fi

echo "previous dir: $prevdir"
echo "script dir: $scriptdir"
#########################################
#   ____                      _ _       #
#  / ___|___  _ __ ___  _ __ (_) | ___  #
# | |   / _ \| '_ ` _ \| '_ \| | |/ _ \ #
# | |__| (_) | | | | | | |_) | | |  __/ #
#  \____\___/|_| |_| |_| .__/|_|_|\___| #
#                      |_|              #
#########################################
cd '../';
make install > $scriptdir/compile.log 2>&1
if [ $? -ne 0 ]; then
    echo "compile failed";
		exit 1;
fi
cd $scriptdir

runTest() {
	sqlFile=$1
	postmaster -p 13370 -D ../../pgdb > $sqlFile.log 2>&1 & 
	pid=$!
	echo "postmaster has pid $pid"
	echo `ps aux | grep postmaster | grep jmate | grep -v grep`

	sleep 1
	psql -p 13370 -d mytest -a -f $sqlFile 

	kill $pid
	sleep 1
}

runTest $1

cd $prevdir;

