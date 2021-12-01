#!/bin/bash
## This file is where we will write the script to run the prometheus server
## To use this script enter the first arg as the server port # and the second arg as the endpoint name of the target
test_invalid_args() {
	local TESTS=4
	local CORRECT=0
	echo "Starting TESTER PART1: INVALID SERVER ARGS"
	echo "Test1: No Arguments"
	## Too few args
	local RET=$(./server)
	echo "Received following output message: ${RET}"
	if [[ "${RET}" == "FAILED: Not enough arguments" ]]; then
		((CORRECT=CORRECT+1)) 
		echo "Test1 Passed!"
	else 
		echo "Test1 Failed!"
	fi
	echo "Test2: Port Number Only"
	RET=$(./server 7390)
	echo "Received following output message: ${RET}"
	if [[ "${RET}" == "FAILED: Not enough arguments" ]]; then
		((CORRECT=CORRECT+1)) 
		echo "Test2 Passed!"
	else 
		echo "Test2 Failed!"
	fi
	echo "Test3: Port Number and Number of Connections"
	RET=$(./server 7390 5)
	echo "Received following output message: ${RET}"
	if [[ "${RET}" == "FAILED: Not enough arguments" ]]; then
		((CORRECT=CORRECT+1)) 
		echo "Test3 Passed!"
	else 
		echo "Test3 Failed!"
	fi
	echo "Test4: Port Number, Number of Connections, Timeout, and Extra Arg"
	## Too many args
	RET=$(./server 7390 5 5 6)
	echo "Received following output message: ${RET}"
	if [[ "${RET}" == "FAILED: Too many arguments" ]]; then
		((CORRECT=CORRECT+1)) 
		echo "Test4 Passed!"
	else 
		echo "Test4 Failed!"
	fi
	echo "Test1: Invalid Args Status: Pass Accuracy ${CORRECT}/${TESTS}"
}

test_invalid_http_requests() {
	local TESTS=8
	local CORRECT=0
	echo "Starting TESTER PART2: INVALID HTTP REQUESTS"
	echo "Test1: Not Sending the User-Agent"
	## Forget to specify the user agent is prometheus
	local RET=$(curl -X GET -H "User-Agent:" -s -o /dev/null -w "%{http_code}" http://localhost:${1})
	echo "Received following status code: ${RET}"
	if [[ "${RET}" == "401" ]]; then
		((CORRECT=CORRECT+1)) 
		echo "Test1 Passed!"
	else 
		echo "Test1 Failed!"
	fi
	echo "Test2: Sending the Incorrect User-Agent"
	## Send incorrect user agent
	local RET=$(curl -X GET -H "User-Agent: Random" -s -o /dev/null -w "%{http_code}" http://localhost:${1})
	echo "Received following status code: ${RET}"
	if [[ "${RET}" == "401" ]]; then
		((CORRECT=CORRECT+1)) 
		echo "Test2 Passed!"
	else 
		echo "Test2 Failed!"
	fi
	echo "Test3: Don't specify the host"
	## Specify that the user agent is prometheus, but don't specify the host
	RET=$(curl -X GET -H "User-Agent: Prometheus" -H "Host:" -s -o /dev/null -w "%{http_code}" http://localhost:${1})
	echo "Received following status code: ${RET}"
	if [[ "${RET}" == "400" ]]; then
		((CORRECT=CORRECT+1)) 
		echo "Test3 Passed!"
	else 
		echo "Test3 Failed!"
	fi
	echo "Test4: Forgot the port number"
	## Mention the host but forget the port number
	RET=$(curl -X GET -H "User-Agent: Prometheus" -H "Host: localhost" -s -o /dev/null -w "%{http_code}" http://localhost:${1})
	echo "Received following status code: ${RET}"
	if [[ "${RET}" == "400" ]]; then
		((CORRECT=CORRECT+1)) 
		echo "Test4 Passed!"
	else 
		echo "Test4 Failed!"
	fi
	echo "Test5: Don't specify the endpoint"
	## Set the host properly, but forget to mention which resource we want
	RET=$(curl -X GET -H "User-Agent: Prometheus" -H "Host: localhost:${1}" -s -o /dev/null -w "%{http_code}" http://localhost:${1})
	echo "Received following status code: ${RET}"
	if [[ "${RET}" == "404" ]]; then
		((CORRECT=CORRECT+1)) 
		echo "Test5 Passed!"
	else 
		echo "Test5 Failed!"
	fi
	echo "Test6: Specify the incorrect endpoint"
	## Mention the wrong resource
	RET=$(curl -X GET -H "User-Agent: Prometheus" -H "Host: localhost:${1}" -s -o /dev/null -w "%{http_code}" http://localhost:${1}/wrong_resource)
	echo "Received following status code: ${RET}"
	if [[ "${RET}" == "404" ]]; then
		((CORRECT=CORRECT+1)) 
		echo "Test6 Passed!"
	else 
		echo "Test6 Failed!"
	fi
	echo "Test7: Specify the wrong port number"
	## Right resource, wrong port number
	RET=$(curl -X GET -H "User-Agent: Prometheus" -H "Host: localhost:23" -s -o /dev/null -w "%{http_code}" http://localhost:${1}/${2})
	echo "Received following status code: ${RET}"
	if [[ "${RET}" == "303" ]]; then
		((CORRECT=CORRECT+1)) 
		echo "Test7 Passed!"
	else 
		echo "Test7 Failed!"
	fi
	echo "Test8: Send the wrong type of request"
	## Send a POST request instead of a GET request
	RET=$(curl -X POST -H "User-Agent: Prometheus" -H "Host: localhost:${1}" -s -o /dev/null -w "%{http_code}" http://localhost:${1}/${2})
	echo "Received following status code: ${RET}"
	if [[ "${RET}" == "400" ]]; then
		((CORRECT=CORRECT+1)) 
		echo "Test8 Passed!"
	else 
		echo "Test8 Failed!"
	fi
	echo "TESTER PART2: INVALID HTTP REQUESTS: Pass Accuracy ${CORRECT}/${TESTS}"
}

test_check_valid_metrics() {
	local TESTS=2
	local CORRECT=0
	echo "Starting TESTER PART3: CHECK VALID METRICS"
	## Check to make sure that our HTTP Requests are valid
	echo "Test1: Send a valid http request"
	local RET=$(curl -X GET -H "User-Agent: Prometheus" -H "Host: localhost:${1}" -s -o /dev/null -w "%{http_code}" http://localhost:${1}/${2})
	echo "Received following status code: ${RET}"
	if [[ "${RET}" == "200" ]]; then
		((CORRECT=CORRECT+1)) 
		echo "Test1 Passed!"
	else
		echo "Test1 Failed!"
	fi
	echo "Test2: Send 2 valid http requests and check if the metrics have changed"
	## Send an HTTP Request to get the metrics
	local RET1=$(curl -X GET -H "User-Agent: Prometheus" -H "Host: localhost:${1}" -s http://localhost:${1}/${2})
	echo "Received the following content from first request:"
	echo "$RET1"
	## Sleep for 2 seconds
	echo "Sleeping for 2 seconds..."
	sleep 2
	## Send another HTTP Request to get the updated metrics
	local RET2=$(curl -X GET -H "User-Agent: Prometheus" -H "Host: localhost:${1}" -s http://localhost:${1}/${2})
	echo "Received the following content from second request:"
	echo "$RET2"
	if [[ "${RET1}" != "${RET2}" ]]; then
		((CORRECT=CORRECT+1)) 
		echo "Test2 Passed!"
	else 
		echo "Test2 Failed!"
	fi
	echo "TESTER PART3: CHECK VALID METRICS: Pass Accuracy ${CORRECT}/${TESTS}"
}

if [ "$#" -ne 2 ]; then
	echo "Error: you must specify the server port and the endpoint name!"
	exit 1
fi 
echo "Starting testing..."
test_invalid_args
echo "Starting server..."
trap "exit" INT TERM ERR
trap "kill 0" EXIT
./server $1 10 1 &
echo "Server is running..."
test_invalid_http_requests $1 $2
test_check_valid_metrics $1 $2
echo "Testing is finished, killing server"


