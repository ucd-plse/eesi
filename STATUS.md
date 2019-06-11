We are applying for "Reusable" and "Available" badges.

## Reusable

This has been packaged into publicly available docker containers. All 
of the scripts used to generate these containers are also included in 
the repository. The containers can be built using the `make_docker.sh`
script (pushes will fail.)

The docker containers are:
```
# The development environment with necessary dependenceies
# to build EESI.
defreez/eesi:dev    

# A compiled version of EESI. The artifact tag matches latest
# the source code at the time of artifact submission, while
# latest will keep moving forward.
defreez/eesi:latest 
defreez/eesi:artifact
```

Instructions have been included for reproducing the data in the paper.
We are in the process of updating the numbers for the camera ready, so
some numbers will not match exactly. The artifact results are reproducible 
and are the numbers that will appear in the camera ready.

Instructions are also included for running EESI on arbitrary LLVM 7 bitcode 
files.

## Available

The artifact has been placed on the publicly available Github repository
https://github.com/ucd-plse/eesi.