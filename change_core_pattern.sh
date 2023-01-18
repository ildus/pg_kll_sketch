#!/bin/bash

sysctl -w kernel.core_pattern=/tmp/core
su postgres -c 'bash run_tests.sh'
