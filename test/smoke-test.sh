#!/bin/sh -ex

cd ..
make clean
make all CFLAGS="-g -O0 -static"

sudo -E ./room list | grep -q smoketest && sudo -E ./room smoketest destroy || true

sudo -E ./room smoketest create
sudo -E ./room smoketest exec hostname
sudo -E ./room smoketest destroy
