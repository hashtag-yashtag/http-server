#!/bin/bash

root=$(git rev-parse --show-toplevel)

testcase=$1
http=$2
mode=$3
verbose=$4

valout="$1.valgrind"
testout="$1.testall"
results="$1.cmp"

valgrind --leak-check=full --show-leak-kinds=all --leak-check-heuristics=stdstring --track-fds=yes -v $http $mode > $valout 2>&1 &
server_pid=$!

sleep 0.5

isalive=$(ps -u $USER | grep $server_pid | uniq | wc -l)

if [[ $isalive != "1" ]]; then
    echo "Could not start server" > $results
    exit 1
fi

port=$($root/get-port.sh $server_pid)

printf "Server pid $server_pid running on port $port..."

$root/testall.sh -h 127.0.0.1:$port 2 4 > $testout 2>&1

sleep 0.5

kill $server_pid
wait $server_pid 2> /dev/null

deflost=$(cat $valout | grep "definitely lost:" | sed -r 's/.*: ([0-9]+) bytes.*/\1/' | tail -n 1)
indlost=$(cat $valout | grep "indirectly lost:" | sed -r 's/.*: ([0-9]+) bytes.*/\1/' | tail -n 1)
fdsopen=$(cat $valout | grep "FILE DESCRIPTORS:" | sed -r 's/.*: ([0-9]+) open.*/\1/' | tail -n 1)

if [[ $deflost != "0" || $indlost != "0" || $fdsopen -gt "4" ]]; then
    echo "$http $mode is leaking!" > $results
    echo "Definitely lost: $deflost" >> $results
    echo "Indirectly lost: $indlost" >> $results
    echo "File descriptors open: $fdsopen" >> $results
    echo "See $valout and $testout for full output" >> $results
    exit 1
elif [[ "$verbose" != "-v" ]]; then
    rm -f $valout $testout
fi

rm -f $results
exit 0
