#!/bin/bash
echo "------------Start Test 3--------------"

valgrind --leak-check=full obj/FileStorageServer test3/Config.txt &
server_pid=$!
echo $server_pid


sleep 2
chmod +x test3/unlimitedClient.sh
./test3/unlimitedClient.sh&
unlimited_pid=$!

sleep 30
kill -s SIGINT $server_pid
kill ${unlimited_pid}


sleep 10
./script/statisticsTest3.sh


