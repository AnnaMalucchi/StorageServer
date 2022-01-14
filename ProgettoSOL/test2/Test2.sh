#!/bin/bash
echo "------------Start Test 2--------------"
valgrind --leak-check=full obj/Client -f ./sockname -W paperino -w ./test2/Client1/PersonaggiDisney -D ./test2/Client1/FileReplacement -p 
valgrind --leak-check=full obj/Client -f ./sockname -W paperino -w ./test2/Client2/Supereroi,3 -D ./test2/Client2/FileReplacement -p 

#sighup al server
killall -s SIGHUP memcheck-amd64-
sleep 4
./script/statisticsTest2.sh
