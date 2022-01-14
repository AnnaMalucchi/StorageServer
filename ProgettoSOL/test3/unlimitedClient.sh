#!/bin/bash
echo "Welcome in unlimited Clients"

while :
do
    obj/Client -t 0 -f ./sockname -W paperino -w ./test3/Client2/Supereroi/IronMan -l ./test2/Client2/Supereroi/IronMan -u ./test2/Client2/Supereroi/IronMan -l ./test3/Client1/PersonaggiDisney/pluto -u ./test3/Client1/PersonaggiDisney/pluto
    sleep 4
done
echo "End unlimitedClients"
echo "--------------------------------------------------------------------------------"