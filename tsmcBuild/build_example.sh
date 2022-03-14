#! /bin/bash

docker build . -f frepple/tsmcBuild/Dockerfile -t frepple-build:test --no-cache

docker build . -f frepple/tsmcBuild/Dockerfile.release -t frepple:test --no-cache