#!/bin/bash
## This file is where we will write the script to run the prometheus server

test_invalid_args() {
	local TESTS=4
	local CORRECT=0
	## Too few args
	local RET=$(./server)
	if [[ "${RET}" == "FAILED: Not enough arguments" ]]; then
		((CORRECT=CORRECT+1)) 
	fi
	RET=$(./server 7380)
	if [[ "${RET}" == "FAILED: Not enough arguments" ]]; then
		((CORRECT=CORRECT+1)) 
	fi
	RET=$(./server 7380 5)
	if [[ "${RET}" == "FAILED: Not enough arguments" ]]; then
		((CORRECT=CORRECT+1)) 
	fi
	## Too many args
	RET=$(./server 7380 5 5 6)
	if [[ "${RET}" == "FAILED: Not enough arguments" ]]; then
		((CORRECT=CORRECT+1)) 
	fi
	echo "Test1: Invalid Args Status: Pass Accuracy ${CORRECT}/${TESTS}"
}

test_invalid_args
