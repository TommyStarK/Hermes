#!/bin/sh

mkdir build ; cd build ; cmake ../ ; make ; valgrind ./tests_hermes ; cd ../ ;
