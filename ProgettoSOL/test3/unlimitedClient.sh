#!/bin/bash
echo "Welcome in unlimited Clients"

while :
do
    obj/Client -t 0 -f ./sockname -W paperino -w ./test3/Client2/Supereroi/IronMan -l ./test3/Client2/Supereroi/IronMan -u ./test3/Client2/Supereroi/IronMan &
    obj/Client -t 0 -f ./sockname -l ./test3/Client1/PersonaggiDisney/pluto -u ./test3/Client1/PersonaggiDisney/pluto  -W paperino &
    obj/Client -t 0 -f ./sockname -r ./test3/Client1/PersonaggiDisney/pluto -c ./test3/Client1/PersonaggiDisney/pluto -W paperino &
    obj/Client -t 0 -f ./sockname -r ./test3/Client1/PersonaggiDisney/pluto -c ./test3/Client1/PersonaggiDisney/pluto -W paperino&

    sleep 0.5
done
echo "End unlimitedClients"
echo "--------------------------------------------------------------------------------" 