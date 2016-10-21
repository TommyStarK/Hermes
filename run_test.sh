#!/bin/sh

mkdir build ; cd build ; cmake ../ ; make ; ./test_netlib ; cd ../ ;
