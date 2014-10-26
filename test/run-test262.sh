#!/bin/sh

DIR=$PWD
TEST262_DIR=../../test262/

if [ ! -d $TEST262_DIR ];
then
	echo "$TEST262_DIR was not found."
	exit 1
fi

if [[ $# -gt 0 ]];
then
	TEST_CHAPTER=$1
else
	# Pass nothing, run everything
	TEST_CHAPTER=""
fi

cd $TEST262_DIR
tools/packaging/test262.py --command $DIR/ejs-test-wrapper --summary $TEST_CHAPTER

