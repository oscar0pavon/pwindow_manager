#!/bin/bash
/usr/bin/make --always-make --dry-run | grep -wE 'cc' | grep -w '\-c' | jq -nR '[inputs|{directory:"/root/pwindow_manager/", command:., file: match(" [^ ]+$").string[1:]}]'  > compile_commands.json

