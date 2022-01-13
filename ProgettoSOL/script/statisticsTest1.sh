#!/bin/bash

#Verifico che il file LOG esista
echo "-------------------------"
echo "Statistics.sh"
FILE=./test1/LogFile
if test -f "$FILE"; 
    then echo "$FILE exist"
else
    echo "No $FILE found. Aborting script."
    exit 1
fi

#Numero di write
nr_write=$(grep "WRITE FILE" -c $FILE)
echo "Number of written files: ${nr_write}"
#Media delle letture in bytes
avg_Write=$(grep "AVERAGE OF BYTES WRITE"  $FILE)
avg_Write=${avg_Write[@]:23}
echo "Average of bytes write: ${avg_Write}"
#Numero di read
nr_read=$(grep "READ FILE" -c $FILE)
echo "Number of files read: ${nr_read}"
#Media delle scritture in bytes
avg_Read=$(grep "AVERAGE OF BYTES READ"  $FILE)
avg_Read=${avg_Read[@]:21}
echo "Average of bytes read: ${avg_Read}"
#Numero di volte in cui e stata acquisita la lock in SCRITTURA
nr_Wlock=$(grep "WRITE LOCK FILE" -c $FILE)
echo "Number of Write Lock: ${nr_Wlock}"
#Numero di volte in cui e stata acquisita la lock in LETTURA
nr_Rlock=$(grep "READ LOCK FILE" -c $FILE)
echo "Number of Read Lock: ${nr_Rlock}"
#Numero di volte in cui e stata rilasciata la lock in SCRITTURA
nr_Wunlock=$(grep "WRITE UNLOCK FILE" -c $FILE)
echo "Number of Write Unlock: ${nr_Wunlock}"
#Numero di volte in cui e stata rilasciata la lock in LETTURA
nr_Runlock=$(grep "READ UNLOCK FILE" -c $FILE)
echo "Number of Read Unlock: ${nr_Runlock}"
#Numero di close
nr_close=$(grep "CLOSE FILE" -c $FILE)
echo "Number of closed files: ${nr_close}"
#Dimensione massima in Mbytes raggiunta dallo storage
dim_max_storage=$(grep "DIMENSION MAX MBYTES"  $FILE)
dim_max_storage=${dim_max_storage[@]:21}
echo "Max dimension Storage (Mbytes): ${dim_max_storage}"
#Massimo numero di File raggiunto dallo storage
nr_max_storage=$(grep "NUMBER MAX FILE"  $FILE)
nr_max_storage=${nr_max_storage[@]:16}
echo "Max number of files in storage: ${nr_max_storage}"
#Numero di rimpiazzamenti
nr_replacements=$(grep "REPLACE_FILE" -c $FILE)
echo "Number of replacements: ${nr_replacements}"
#Massimo numero di Client attivi sullo Storage
nr_client=$(grep "MAXIMUM NUMBER OF CLIENTS CONNECTED AT THE SAME TIME" $FILE)
nr_client=${nr_client[@]:52}
echo "Maximum number of clients connected at the same time: ${nr_client}"
#Ricavo il numero di Workers
nr_workers=$(grep "NR WORKERS" $FILE)
nr_workers=${nr_workers[@]:11}
echo "Number of Workers: ${nr_workers}"
n=0
while [ $n -lt $nr_workers ];
do
    nr_request=$(grep "REQUEST MADE BY WORKERS: $n" -c $FILE)
    echo "Number of request made by workers $n: ${nr_request}"
    ((n++))
done
echo "----------------------"
