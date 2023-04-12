#!/bin/sh
euRun &
sleep 1 
euLog &
sleep 5 
euCliProducer -n FERSProducer -t my_fers0 &
