#!/bin/bash

./producer 1
echo "Added 1 passenger."
sleep 1

./consumer --start
echo "Started elevator."
sleep 7

./consumer --stop
echo "Stopped elevator."
sleep 4

./consumer --start
echo "Started elevator."
sleep 7

./producer 5
echo "Added 5 passengers."
sleep 6

./consumer --stop
echo "Stopped elevator."
sleep 20

./consumer --start