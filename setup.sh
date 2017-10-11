#!/bin/bash

# Script base directory.
basedir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Dynamic libraries path.
lib_dir=$basedir/lib
[[ "$LD_LIBRARY_PATH" =~ "${lib_dir}" ]] || export LD_LIBRARY_PATH=${lib_dir}:$LD_LIBRARY_PATH
[[ "$LIBRARY_PATH" =~ "${lib_dir}" ]] || export LIBRARY_PATH=${lib_dir}:$LIBRARY_PATH

# Include path, for GCC.
inc_dir=$basedir/include
[[ "$C_INCLUDE_PATH" =~ "${inc_dir}" ]] || export C_INCLUDE_PATH=${inc_dir}:$C_INCLUDE_PATH
[[ "$CPLUS_INCLUDE_PATH" =~ "${inc_dir}" ]] || export CPLUS_INCLUDE_PATH=${inc_dir}:$CPLUS_INCLUDE_PATH
