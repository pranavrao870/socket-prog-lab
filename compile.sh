#!/bin/bash

echo "Expecting rolls in STDIN..."

while read R
do
    pushd submissions/$R
    {
	for phase in `seq 1 4`
	do
	    for part in Client Server
	    do
		f="SimpleFTP${part}Phase${phase}"
		if [ -s "$f.cpp" ]; then
		    rm -f "$f"
		    flags[1]=""
		    flags[2]="-std=c++11"
		    # {
		    # 	echo "#include <stdio.h>"; echo "#include <errno.h>"; cat "$f.cpp"
		    # } > "$f-mod.cpp"
		    for i in 1 2
		    do
			echo "Trying comping with g++ flags ${flags[$i]}..."
			g++ ${flags[$i]} -o "$f" "$f.cpp"
			if [ -x "$f" ]; then
			    break
			fi
		    done
		    if [ -x "$f" ]; then
			echo "$f compilation successful"
		    else
			echo "$f compilation failed"
		    fi
		else
		    msg="File $f.cpp not present or is empty"
		    echo $msg
		    echo $msg 1>&2
		fi
	    done
	done
    } > g++-compile-out.txt 2>g++-compile-out.err
    popd
done
