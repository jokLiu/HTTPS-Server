#!/bin/bash

foo(){
    curl --insecure https://localhost:1234
    #wget --no-check-certificate https://localhost:1234
}

for i in {1..200}; do
    foo &
done
