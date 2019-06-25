Instructions to build:
  make

The first build will extract and build libmicrohttpd


Instructions to package:
  make package

The required files will be packaged into the pkg folder


Instructions to run:

execute emgduid binary
	./emgduid

open browser (Firefox or Chrome)
Set the URL to:
	http://localhost:8888


Other Notes:
Instructions to build 32 bit binaries on a 64 bit machine:
  make distclean (only needed if you have previously built 64 bit binaries)
  linux32 make


