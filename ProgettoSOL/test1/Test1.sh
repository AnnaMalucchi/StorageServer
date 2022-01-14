#!/bin/bash
echo "------------Start Test 1--------------"

valgrind --leak-check=full obj/Client -t 200 -f ./sockname -W paperino -W paperino -w ./test1/Client1/PersonaggiDisney 2  -D ./test1/Client1/FileReplacement -l paperino -u paperino -p 
valgrind --leak-check=full obj/Client -t 200 -f ./sockname -r paperino -d test1/Client2/ReadFile -p 
valgrind --leak-check=full obj/Client -t 200 -f ./sockname -R2 -d test1/Client3/ReadFile -p -c paperino 


#sighup al server
killall -s SIGHUP memcheck-amd64-
sleep 3
./script/statisticsTest1.sh

