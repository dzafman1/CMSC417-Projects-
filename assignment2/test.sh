#!/bin/bash 
truncate -s 0 node0
truncate -s 0 node1
truncate -s 0 node2
truncate -s 0 node3
script -c "sudo ./rt -n 0" node0 & script -c "sudo ./rt -n 1" node1 & script -c "sudo ./rt -n 2" node2 & script -c "sudo ./rt -n 3" node3

