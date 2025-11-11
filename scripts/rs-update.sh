#!/bin/bash

killall rs-client

killall rs-worker

sleep 1

~/collatz/scripts/rs-bootstrap.sh $*
