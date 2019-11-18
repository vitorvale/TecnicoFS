#!/bin/bash

INPUTDIR="$1"
OUTPUTDIR="$2"
MAXTHREADS="$3"
NUMBERBUCKETS="$4"

for INPUTFILE in ${INPUTDIR}/*.txt
do
	FILENAME=$(basename "${INPUTFILE}" .txt)
	echo InputFile=${FILENAME}.txt NumThreads=1
	OUTPUTFILE="${FILENAME}-1.txt"
	./tecnicofs-nosync ${INPUTFILE} ${OUTPUTDIR}/${OUTPUTFILE} 1 1 | grep -v "found" 
	echo
	for NUMBERTHREADS in $(seq 2 ${MAXTHREADS})
	do
		FILENAME=$(basename "${INPUTFILE}" .txt) 
		echo InputFile=${FILENAME}.txt NumThreads=${NUMBERTHREADS}
		OUTPUTFILE="${FILENAME}-${NUMBERTHREADS}.txt"
		./tecnicofs-mutex ${INPUTFILE} ${OUTPUTDIR}/${OUTPUTFILE} ${NUMBERTHREADS} ${NUMBERBUCKETS} | grep -v "found" 
		echo
	done
done

chmod +x runTests.sh