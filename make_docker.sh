#!/bin/bash

docker build -t defreez/eesi:dev -f Dockerfile.dev .
docker build -t defreez/eesi:latest .

docker tag defreez/eesi:latest defreez/eesi:artifact
docker tag defreez/eesi:dev 362138654849.dkr.ecr.us-west-2.amazonaws.com/defreez/eesi:dev
docker push defreez/eesi:latest
docker push defreez/eesi:artifact
docker push defreez/eesi:dev
docker push 362138654849.dkr.ecr.us-west-2.amazonaws.com/defreez/eesi:dev
