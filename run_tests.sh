#!/bin/sh

mkdir build ; cd build ; cmake ../ ; make ; ./tests_hermes ; cd ../ ;
