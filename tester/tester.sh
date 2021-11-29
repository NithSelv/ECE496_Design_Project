#!/bin/bash
## This file is where we will write the script to run the prometheus server
## To use this script enter the first arg as the server port # and the second arg as the endpoint name of the target
test_invalid_args() {
	local TESTS=4
	local CORRECT=0
	echo "STARTING TEST1 INVALID SERVER ARGS"
	## Too few args
	local RET=$(./server)
	if [[ "${RET}" == "FAILED: Not enough arguments" ]]; then
		((CORRECT=CORRECT+1)) 
	fi
	echo "TEST 1 COMPLETED"
	RET=$(./server 7390)
	if [[ "${RET}" == "FAILED: Not enough arguments" ]]; then
		((CORRECT=CORRECT+1)) 
	fi
	echo "TEST 2 COMPLETED"
	RET=$(./server 7390 5)
	if [[ "${RET}" == "FAILED: Not enough arguments" ]]; then
		((CORRECT=CORRECT+1)) 
	fi
	echo "TEST 3 COMPLETED"
	## Too many args
	RET=$(./server 7390 5 5 6)
	if [[ "${RET}" == "FAILED: Not enough arguments" ]]; then
		((CORRECT=CORRECT+1)) 
	fi
	echo "TEST 4 COMPLETED"
	echo "Test1: Invalid Args Status: Pass Accuracy ${CORRECT}/${TESTS}"
}

test_invalid_http_requests() {
	local TESTS=7
	local CORRECT=0
	echo "STARTING TEST2 INVALID HTTP REQUESTS"
	## Forget to specify the user agent is prometheus
	local RET=$(curl -X GET -s -o /dev/null -w "%{http_code}" http://localhost:${1})
	if [[ "${RET}" == "401" ]]; then
		((CORRECT=CORRECT+1)) 
	fi
	echo "${RET}"
	echo "TEST 1 COMPLETED"
	## Specify that the user agent is prometheus, but don't sepcify the host
	RET=$(curl -X GET -H "User-Agent: Prometheus" -s -o /dev/null -w "%{http_code}" http://localhost:${1})
	if [[ "${RET}" == "400" ]]; then
		((CORRECT=CORRECT+1)) 
	fi
	echo "TEST 2 COMPLETED"
	## Mention the host but forget the port number
	RET=$(curl -X GET -H "User-Agent: Prometheus" -H "Host: localhost" -s -o /dev/null -w "%{http_code}" http://localhost:${1})
	if [[ "${RET}" == "400" ]]; then
		((CORRECT=CORRECT+1)) 
	fi
	echo "TEST 3 COMPLETED"
	## Set the host properly, but forget to mention which resource we want
	RET=$(curl -X GET -H "User-Agent: Prometheus" -H "Host: localhost:${1}" -s -o /dev/null -w "%{http_code}" http://localhost:${1})
	if [[ "${RET}" == "400" ]]; then
		((CORRECT=CORRECT+1)) 
	fi
	echo "TEST 4 COMPLETED"
	## Mention the wrong resource
	RET=$(curl -X GET -H "User-Agent: Prometheus" -H "Host: localhost:${1}" -s -o /dev/null -w "%{http_code}" http://localhost:${1}/wrong_resource)
	if [[ "${RET}" == "404" ]]; then
		((CORRECT=CORRECT+1)) 
	fi
	echo "TEST 5 COMPLETED"
	## Right resource, wrong port number
	RET=$(curl -X GET -H "User-Agent: Prometheus" -H "Host: localhost:0" -s -o /dev/null -w "%{http_code}" http://localhost:${1}/${2})
	if [[ "${RET}" == "303" ]]; then
		((CORRECT=CORRECT+1)) 
	fi
	echo "TEST 6 COMPLETED"
	## Send a POST request instead of a GET request
	RET=$(curl -X POST -H "User-Agent: Prometheus" -H "Host: localhost:${1}" -s -o /dev/null -w "%{http_code}" http://localhost:${1}/${2})
	if [[ "${RET}" == "400" ]]; then
		((CORRECT=CORRECT+1)) 
	fi
	echo "TEST 7 COMPLETED"
	echo "Test2: Check Invalid HTTP Headers: Pass Accuracy ${CORRECT}/${TESTS}"
}

test_check_valid_metrics() {
	local TESTS=2
	local CORRECT=0
	echo "STARTING TEST3 CHECK VALID METRICS"
	## Check to make sure that our HTTP Requests are valid
	local RET=$(curl -X POST -H "User-Agent: Prometheus" -H "Host: localhost:${1}" -s -o /dev/null -w "%{http_code}" http://localhost:${1}/${2})
	if [[ "${RET}" == "200" ]]; then
		((CORRECT=CORRECT+1)) 
	fi
	echo "TEST 1 COMPLETED"
	## Send an HTTP Request to get the metrics
	local RET1=$(curl -X GET -H "User-Agent: Prometheus" -H "Host: localhost:${1}" -s http://localhost:${1}/${2})
	## Send another HTTP Request to get the updated metrics
	local RET2=$(curl -X GET -H "User-Agent: Prometheus" -H "Host: localhost:${1}" -s http://localhost:${1}/${2})
	if [[ "${RET1}" != "${RET2}" ]]; then
		((CORRECT=CORRECT+1)) 
	fi
	echo "TEST 2 COMPLETED"
	echo "Test3: test_check_valid_metrics: Pass Accuracy ${CORRECT}/${TESTS}"
}

if [ "$#" -ne 2 ]; then
	echo "Error: you must specify the server port and the endpoint name!"
	exit 1
fi 
echo "STARTING TESTING"
test_invalid_args
echo "STARTING UP SERVER FOR TEST2 AND TEST3"
./server $1 10 1 & > /dev/null
pid = $(echo $!)
echo "SERVER IS RUNNING"
echo "PID IS ${pid}"
test_invalid_http_requests $1 $2
test_check_valid_metrics $1 $2
echo "TESTING IS FINISHED, KILLING SERVER"
kill -SIGTERM $pid


