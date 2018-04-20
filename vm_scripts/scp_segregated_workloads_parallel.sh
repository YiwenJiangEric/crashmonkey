#!/bin/bash

batch_size=5
st=1
end=`expr $st + $batch_size - 1`
num_servers=64

i=1
while [ $st -le $num_servers ]; do
	nohup ./scp_segregated_workloads.sh $st $end > out"$i".log &
	st=`expr $st + $batch_size`
	end=`expr $end + $batch_size`
	i=`expr $i + 1`
done