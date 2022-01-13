#!/bin/bash
echo "------------Start Test 3--------------"

valgrind --leak-check=full obj/FileStorageServer test3/Config.txt &
server_pid=$!
echo $server_pid

sleep 2

for i in $(seq 1 1 100)
do
    #sobj/Client -t 30 -f ./sockname -W paperino -p &
    obj/Client -t 0 -f ./sockname -W paperino -w ./test3/Client2/Supereroi/IronMan -l ./test2/Client2/Supereroi/IronMan -u ./test2/Client2/Supereroi/IronMan -l ./test3/Client1/PersonaggiDisney/pluto -u ./test3/Client1/PersonaggiDisney/pluto &
    #obj/Client -t 0 -f ./sockname -l ./test3/Client1/PersonaggiDisney/pluto -u ./test3/Client1/PersonaggiDisney/pluto &
    #obj/Client -t 0 -f ./sockname -l ./test3/Client1/PersonaggiDisney/pluto -u ./test3/Client1/PersonaggiDisney/pluto &   
    #obj/Client -t 0 -f ./sockname -r ./test3/Client1/PersonaggiDisney/pluto -c ./test3/Client1/PersonaggiDisney/pluto &
    #obj/Client -t 0 -f ./sockname -r ./test3/Client1/PersonaggiDisney/pluto -c ./test3/Client1/PersonaggiDisney/pluto &
    
    obj/Client -t 0 -f ./sockname -r ./test3/Client1/PersonaggiDisney/topolino -c ./test3/Client1/PersonaggiDisney/topolino &
    #obj/Client -t 0 -f ./sockname -l ./test3/Client1/PersonaggiDisney/clarabella -u ./test3/Client1/PersonaggiDisney/clarabella &
    #obj/Client -t 0 -f ./sockname -l ./test3/Client1/PersonaggiDisney/minnie  -u ./test3/Client1/PersonaggiDisney/minnie &
    #obj/Client -t 0 -f ./sockname W ./test3/Client1/PersonaggiDisney/clarabella -l ./test3/Client1/PersonaggiDisney/clarabella -u ./test3/Client1/PersonaggiDisney/clarabella &
    #obj/Client -t 0 -f ./sockname -r ./test3/Client1/PersonaggiDisney/clarabella -c ./test3/Client1/PersonaggiDisney/clarabella &
    #obj/Client -t 0 -f ./sockname -l ./test3/Client1/PersonaggiDisney/clarabella -u ./test3/Client1/PersonaggiDisney/clarabella &
    #obj/Client -t 0 -f ./sockname -W paperino -w ./test3/Client2/Supereroi/IronMan -l ./test2/Client2/Supereroi/IronMan -u ./test2/Client2/Supereroi/IronMan &
    
done
sleep 30
echo "Send SIGINT"
kill -s SIGINT $server_pid

sleep 60
./script/statisticsTest3.sh
