#!/bin/bash

if [ -z "$TOSLEEP" ]; then
    export TOSLEEP=10
fi

cmd="$*"
set -m
if [ -z "$STDOUT_REDIR" ]; then
    $cmd &
else
    echo "STDOUT_REDIR=$STDOUT_REDIR"
    stdbuf -o0 $cmd > "$STDOUT_REDIR" &
fi
child_pid=$!
./kill_after_TOSLEEP.sh $child_pid &
fg %1
