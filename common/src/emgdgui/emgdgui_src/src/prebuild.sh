#!/bin/bash
#----------------------------------------------------------------------------
# Copyright (c) 2002-2014, Intel Corporation.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
# 
#----------------------------------------------------------------------------
# This method will be called from make
#
DST_FILE=$1

function insertHTML() {
	htmlfile=$1
	replace_str=$2
	if grep -q '"' "$htmlfile"; then
		echo 'Error: double-quotes (") found in index.html'
		echo "Please change them to single-quotes (')"
		exit 1
	fi
	awk '{print "\"",$0,"\\n\"" }' "$htmlfile" > .tmpfile
	echo ";" >> .tmpfile
	LINE_NUM=$(grep -nh "$replace_str" "$DST_FILE"| cut -d: -f1)
	sed -i "${LINE_NUM}r .tmpfile"  "$DST_FILE"
	rm .tmpfile
}

cp -f emgduid.cpp "$DST_FILE"
insertHTML "index.html" "REPLACE_INDEX_HTML"
chmod -w "$DST_FILE"
