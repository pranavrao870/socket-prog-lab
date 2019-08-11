#!/bin/bash

echo "Expecting rolls in STDIN..."

#REMOTE=10.129.131.113
REMOTE=10.129.158.81
#REMOTE=10.129.41.2
#REMOTE=127.0.0.1
START_PORT=2000
CL_RUNDIR_BASE=/tmp/submissions

file[1]="file1K.txt"
file[2]="file200K.txt"
file[3]="file200K.pdf"
file[4]="file1M.jpg"

function find_next_free_port() {
    port=$1
    while true
    do
	netstat -an | grep tcp | grep ":$port" > /dev/null
	res=$?
	if [ $res -ne 0 ]; then
	    echo $port
	    break;
	fi
	let port=$port+1
	if [ $port -ge 65535 ]; then
	    echo "Ran out of ports, resetting, sleeing for 5 sec!" 1>&2
	    sleep 5
	    port=$START_PORT
	fi
    done
}

function transfer_files() {
    R=$1
    cp -ra compile.sh kill_after_TOSLEEP.sh test-sp.sh timed.sh submissions/$R/
    cp -ra test-files/* submissions/$R/
    rsync -aru submissions/$R /tmp/submissions/
} # End transfer_files()

function test_phase1() {
    R=$1
    echo "############## Testing Phase1 $R ##########################"
    CL_RUNDIR="$CL_RUNDIR_BASE/$R/"
    marks_str="0"
    if [ -x "./SimpleFTPClientPhase1" ]; then
	# Run client with wrong args: 0.5
	pushd "$CL_RUNDIR"
	echo "Running client with no args"
	./timed.sh ./SimpleFTPClientPhase1
	res=$?
	if [ "$res" -eq 1 ]; then
	    marks=0.5
	else
	    marks=0
	fi
	echo "Exit code $res, marks=$marks"
	marks_str="$marks_str+$marks"

	# Run client with wrong server port: 0.5
	let WRONG_PORT=$next_port
	echo "Running client with wrong port $WRONG_PORT"
	./timed.sh ./SimpleFTPClientPhase1 "$REMOTE:$WRONG_PORT" "${file[1]}.client"
	res=$?
	if [ "$res" -eq 2 ]; then
	    marks=0.5
	else
	    marks=0
	fi
	echo "Exit code $res, marks=$marks"
	marks_str="$marks_str+$marks"
	popd

    else
	echo "No executable file ./SimpleFTPClientPhase1"
	marks=0
	marks_str="$marks_str+$marks"
    fi

    if [ -x "./SimpleFTPServerPhase1" ]; then
	# Run server with wrong args: 0.5
	echo "Running server with no args"
	./timed.sh ./SimpleFTPServerPhase1
	res=$?
	if [ "$res" -eq 1 ]; then
	    marks=0.5
	else
	    marks=0
	fi
	echo "Exit code $res, marks=$marks"
	marks_str="$marks_str+$marks"

	# Run server on already bound python port: 0.5
	echo "Running server on already bound python port $PYTHON_PORT"
	./timed.sh ./SimpleFTPServerPhase1 "$PYTHON_PORT" "${file[1]}"
	res=$?
	if [ "$res" -eq 2 ]; then
	    marks=0.5
	else
	    marks=0
	fi
	echo "Exit code $res, marks=$marks"
	marks_str="$marks_str+$marks"

    else
	echo "No executable file ./SimpleFTPServerPhase1"
	marks=0
	marks_str="$marks_str+$marks"
    fi

    numTests=4
    if [ ! -x ./SimpleFTPClientPhase1 ]; then
	numTests=0
    fi
    if [ ! -x ./SimpleFTPServerPhase1 ]; then
	numTests=0
    fi

    # Test file transfers
    for s in `seq 1 $numTests`
    do
	next_port=$(find_next_free_port $next_port)
	ser_md5=$(md5sum ${file[$s]} | awk '{print $1}')
	echo "Running server at port $next_port file=${file[$s]}"
	STDOUT_REDIR=server-out-phase1-$s.txt ./timed.sh ./SimpleFTPServerPhase1 $next_port ${file[$s]} &
	sleep 1
	pushd $CL_RUNDIR
	echo "Running client to $REMOTE:$next_port"
	rm -f "${file[$s]}.client"
	./timed.sh ./SimpleFTPClientPhase1 "$REMOTE:$next_port" ${file[$s]}.client > client-out-phase1-$s.txt
	cli_md5=$(md5sum ${file[$s]}.client | awk '{print $1}')
	popd
	sleep 1
	killall -TERM SimpleFTPServerPhase1
	if [ "$ser_md5" == "$cli_md5" ]; then
	    marks=1
	else
	    marks=0
	fi
	echo "ser_md5=$ser_md5"
	echo "cli_md5=$cli_md5"
	marks_str="$marks_str+$marks"
	echo "marks=$marks"
    done

    cp -ra $CL_RUNDIR/*.client $CL_RUNDIR/client-out-phase1-*.txt ./

    marks=$(echo $marks_str | bc)
    echo "PHASE1_MARKS=$marks_str=$marks"

    echo "############## Done Phase1 $R ##########################"

} # End test_phase1

function test_phase2() {
    R=$1
    echo "############## Testing Phase2 $R ##########################"
    CL_RUNDIR="$CL_RUNDIR_BASE/$R/"
    marks_str="0"
    numTests=3
    if [ ! -x ./SimpleFTPClientPhase2 ]; then
	numTests=0
    fi
    if [ ! -x ./SimpleFTPServerPhase2 ]; then
	numTests=0
    fi

    for fnum in `seq 1 4`
    do
	ser_md5[$fnum]=$(md5sum ${file[$fnum]} | awk '{print $1}')
	ser_file_size[$fnum]=$(ls -s "${file[$fnum]}" | awk '{print $1}')
    done

    # Test file transfers
    for s in `seq 1 $numTests`
    do
	next_port=$(find_next_free_port $next_port)

	echo "Running server at port $next_port"
	STDOUT_REDIR=server-out-phase2-$s.txt ./timed.sh ./SimpleFTPServerPhase2 $next_port &
	sleep 1

	pushd $CL_RUNDIR
	numCl=1
	if [ $s -eq "$numTests" ]; then
	    numCl=2
	fi
	for c in `seq 1 $numCl`
	do
	    let fnum=$s+$c-1
	    echo "Running client to $REMOTE:$next_port file=${file[$fnum]}"
	    rm -f "${file[$fnum]}"
	    ./timed.sh ./SimpleFTPClientPhase2 "$REMOTE:$next_port" ${file[$fnum]} > client-out-phase2-$fnum.txt
	    if [ -f "${file[$fnum]}" ]; then
		mv "${file[$fnum]}" "${file[$fnum]}.client"
	    else
		echo "No file ${file[$fnum]} at client; creating empty file"
		rm -f "${file[$fnum]}.client"
		touch "${file[$fnum]}.client"	    
	    fi
	    cli_file_size=$(ls -s "${file[$fnum]}.client" | awk '{print $1}')
	    let max_file_size=${ser_file_size[$fnum]}*10
	    if [ "$cli_file_size" -ge "$max_file_size" ]; then
		echo "File at client too large; buggy client; replacing ${file[$fnum]}.client with empty file"
		rm -f "${file[$fnum]}.client"
		touch "${file[$fnum]}.client"
	    fi
	    cli_md5=$(md5sum ${file[$fnum]}.client | awk '{print $1}')

	    if [ "${ser_md5[$fnum]}" == "$cli_md5" ]; then
		marks=1
	    else
		marks=0
	    fi
	    echo "ser_md5=${ser_md5[$fnum]}"
	    echo "cli_md5=$cli_md5"
	    marks_str="$marks_str+$marks"
	    echo "marks=$marks"
	done
	popd

	sleep 1
	killall -TERM SimpleFTPServerPhase2

    done

    cp -ra $CL_RUNDIR/*.client $CL_RUNDIR/client-out-phase2-*.txt ./

    marks=$(echo $marks_str | bc)
    echo "PHASE2_MARKS=$marks_str=$marks"

    echo "############## Done Phase2 $R ##########################"

} # End test_phase2

function test_phase3() {
    R=$1
    echo "############## Testing Phase3 $R ##########################"
    CL_RUNDIR="$CL_RUNDIR_BASE/$R/"
    marks_str="0"

    # Run server
    next_port=$(find_next_free_port $next_port)

    echo "Running server at port $next_port"
    TO_SLEEP=41 STDOUT_REDIR=server-out-phase3.txt ./timed.sh ./SimpleFTPServerPhase3 $next_port &
    sleep 1

    for fnum in `seq 1 4`
    do
	ser_md5[$fnum]=$(md5sum ${file[$fnum]} | awk '{print $1}')
	ser_file_size[$fnum]=$(ls -s "${file[$fnum]}" | awk '{print $1}')
    done

    pushd $CL_RUNDIR

    fnum=4
    interval=20 # ms
    echo "Running client to $REMOTE:$next_port file=${file[$fnum]} interval=$interval"
    rm -f "${file[$fnum]}"
    TO_SLEEP=40 STDOUT_REDIR=client-out-phase3-$fnum.txt ./timed.sh ./SimpleFTPClientPhase3 "$REMOTE:$next_port" ${file[$fnum]} $interval &
    cl_pid[$fnum]=$!
    fnum=1
    interval=1 # ms
    echo "Running client to $REMOTE:$next_port file=${file[$fnum]} interval=$interval"
    rm -f "${file[$fnum]}"
    TO_SLEEP=40 STDOUT_REDIR=client-out-phase3-$fnum.txt ./timed.sh ./SimpleFTPClientPhase3 "$REMOTE:$next_port" ${file[$fnum]} $interval &
    cl_pid[$fnum]=$!
    #wait
    wait ${cl_pid[4]} ${cl_pid[1]} # wait for clients to finish
    sleep 1
    killall -TERM SimpleFTPServerPhase3

    for fnum in 4 1
    do
	if [ -f "${file[$fnum]}" ]; then
	    mv "${file[$fnum]}" "${file[$fnum]}.client"
	else
	    echo "No file ${file[$fnum]} at client; creating empty file"
	    rm -f "${file[$fnum]}.client"
	    touch "${file[$fnum]}.client"	    
	fi
	cli_file_size=$(ls -s "${file[$fnum]}.client" | awk '{print $1}')
	let max_file_size=${ser_file_size[$fnum]}*10
	if [ "$cli_file_size" -ge "$max_file_size" ]; then
	    echo "File at client too large; buggy client; replacing ${file[$fnum]}.client with empty file"
	    rm -f "${file[$fnum]}.client"
	    touch "${file[$fnum]}.client"
	fi
	cli_md5[$fnum]=$(md5sum ${file[$fnum]}.client | awk '{print $1}')
    done

    popd

    if [ "${ser_md5[4]}" == "${cli_md5[4]}" -a "${ser_md5[1]}" == "${cli_md5[1]}" ]; then
	marks=1
    else
	marks=0
    fi

    for fnum in 4 1
    do
	echo "file=${file[$fnum]}"
	echo "ser_md5=${ser_md5[$fnum]}"
	echo "cli_md5=${cli_md5[$fnum]}"
    done
    marks_str="$marks_str+$marks"
    echo "marks=$marks"

    cp -ra $CL_RUNDIR/*.client $CL_RUNDIR/client-out-phase3-*.txt ./

    # Finish time of 2nd file should be before finish time of first: 4 marks
    if [ "$marks" -eq 1 ]; then
	if [ "${file[4]}.client" -nt "${file[1]}.client" ]; then
	    echo "Smaller file ${file[1]} downloaded before larger file ${file[4]}"
	    marks=4
	else
	    echo "Smaller file ${file[1]} downloaded only after larger file ${file[4]}"
	    marks=0
	fi
	marks_str="$marks_str+$marks"
	echo "marks=$marks"
    fi

    marks=$(echo $marks_str | bc)
    echo "PHASE3_MARKS=$marks_str=$marks"

    echo "############## Done Phase3 $R ##########################"

} # End test_phase3


function test_phase4() {
    R=$1
    echo "############## Testing Phase4 $R ##########################"
    CL_RUNDIR="$CL_RUNDIR_BASE/$R/"
    marks_str="0"

    # Run server
    next_port=$(find_next_free_port $next_port)

    echo "Running server at port $next_port"
    TO_SLEEP=41 STDOUT_REDIR=server-out-phase4.txt ./timed.sh ./SimpleFTPServerPhase4 $next_port &
    sleep 1

    for fnum in `seq 1 4`
    do
	ser_md5[$fnum]=$(md5sum ${file[$fnum]} | awk '{print $1}')
	ser_file_size[$fnum]=$(ls -s "${file[$fnum]}" | awk '{print $1}')
    done

    pushd $CL_RUNDIR

    for op in put get
    do
	for fnum in 2 3
	do
	    interval=1 # ms
	    if [ $op == "put" ]; then
		mv "${file[$fnum]}" "${file[$fnum]}.put"
		file_name="${file[$fnum]}.put"
	    else
		rm -f "${file[$fnum]}"
		file_name="${file[$fnum]}"
	    fi
	    echo "Running client to $REMOTE:$next_port op=$op file=$file_name interval=$interval"
	    TO_SLEEP=40 STDOUT_REDIR=client-out-phase4-$op-$fnum.txt ./timed.sh ./SimpleFTPClientPhase4 "$REMOTE:$next_port" $op $file_name $interval
	    if [ $op == "get" ]; then
		if [ -f "${file[$fnum]}" ]; then
		    mv "${file[$fnum]}" "${file[$fnum]}.get"
		else
		    echo "No file ${file[$fnum]} at client; creating empty file"
		    rm -f "${file[$fnum]}.get"
		    touch "${file[$fnum]}.get"	    
		fi
		cli_file_size=$(ls -s "${file[$fnum]}.get" | awk '{print $1}')
		let max_file_size=${ser_file_size[$fnum]}*10
		if [ "$cli_file_size" -ge "$max_file_size" ]; then
		    echo "File at client too large; buggy client; replacing ${file[$fnum]}.get with empty file"
		    rm -f "${file[$fnum]}.get"
		    touch "${file[$fnum]}.get"
		fi
	    fi
	done # for fnum
    done # for op

    popd
    sleep 5
    killall -TERM SimpleFTPServerPhase4

    cp -ra $CL_RUNDIR/*.get $CL_RUNDIR/client-out-phase4-*.txt ./

    for op in put get
    do
	for fnum in 2 3
	do
	    if [ "$op" == "put" ]; then
		if [ ! -f "${file[$fnum]}.put" ]; then
		    echo "No file ${file[$fnum]}.put at server; creating empty file"
		    rm -f "${file[$fnum]}.put"
		    touch "${file[$fnum]}.put"
		fi
		put_file_size=$(ls -s "${file[$fnum]}.put" | awk '{print $1}')
		let max_file_size=${ser_file_size[$fnum]}*10
		if [ "$put_file_size" -ge "$max_file_size" ]; then
		    echo "File at server too large; buggy code; replacing ${file[$fnum]}.put with empty file"
		    rm -f "${file[$fnum]}.put"
		    touch "${file[$fnum]}.put"
		fi
	    fi

	    transfer_md5=$(md5sum "${file[$fnum]}.$op" | awk '{print $1}')
	    if [ "$transfer_md5" == "${ser_md5[$fnum]}" ]; then
		marks=1
	    else
		marks=0
	    fi
	    echo "transfer_md5=$transfer_md5"
	    echo "expected_md5=${ser_md5[$fnum]}"
	    echo "marks=$marks"
	    marks_str="$marks_str+$marks"
	done # fnum
    done # op

    marks=$(echo $marks_str | bc)
    if [ "$marks" -eq 4 ]; then
	echo "put and get working fine; not checking for simultaneous clients; bonus 1 mark"
	marks=5
	marks_str="${marks_str}+1"
    fi
    echo "PHASE4_MARKS=$marks_str=$marks"

    echo "############## Done Phase4 $R ##########################"

} # End test_phase4


# Main begins here
PYTHON_PORT=$(find_next_free_port $START_PORT)
echo "Running python on $PYTHON_PORT"
python -m SimpleHTTPServer $PYTHON_PORT &
python_pid=$!

let next_port=$PYTHON_PORT+1
next_port=$(find_next_free_port $next_port)

while read R
do
    transfer_files $R
    pushd submissions/$R
    for p in `seq 4 4`
    do
	if [ "$p" -gt 1 ]; then
	    kill -TERM $python_pid
	fi
	if [ "$p" -ge 3 ]; then
	    REMOTE="0.0.0.0"
	fi
	test_phase$p $R </dev/null >phase$p-out.txt 2>phase$p-out.err
    done
    popd
done

kill -TERM $python_pid
