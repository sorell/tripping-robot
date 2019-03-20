#!/bin/sh

# How many seconds to wait for application before killing it
WAIT_S=20

# Don't modify these
killStat=55
retries=0

while [ $killStat -eq 55 ] ; do
	/PATH/TO/YOUR_APPLICATION_HERE AND_ARGUMENTS &
	retries=$(($retries+1))
	pid=$!
	echo "=== Running with pid $pid ==="

	(sleep $WAIT_S; echo "=== Timeout: killing $pid ==="; kill -9 $pid; exit 55) &
	killerPid=$!

	wait $pid
	appStat=$?
	echo "=== Returned $appStat ==="
	kill $killerPid 2>/dev/null
	wait $killerPid
	killStat=$?
	killall -9 YOUR_APPLICATION # 2>/dev/null
	sleep 2
done

echo "=== Crashed after $retries retries ==="
