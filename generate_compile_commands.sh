#!/bin/sh
make -j32 | grep -wE 'cc' | grep -w '\-c' | jq -nR '[inputs|{directory:".", command:., file: match(" [^ ]+$").string[1:]}]'  > compile_commands.json

