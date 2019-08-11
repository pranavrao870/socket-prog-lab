#!/bin/bash

if [ -z "$TOSLEEP" ]; then
	TOSLEEP=20
fi

sleep $TOSLEEP
kill -TERM $1
