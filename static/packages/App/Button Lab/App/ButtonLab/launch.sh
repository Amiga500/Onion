#!/bin/sh
echo $0 $*
progdir=`cd -- "$(dirname "$0")" >/dev/null 2>&1; pwd -P`
cd $progdir
exec ./buttonlab
