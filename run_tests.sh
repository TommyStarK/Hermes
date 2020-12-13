#!/usr/bin/env bash

rm -rf build && mkdir build && cd build && cmake .. && make && ./tests_hermes && cd ../ ;
