Tiny Tiny HTTPD
===============

WHAT IS THIS:
-------------

	tiny tiny httpd writen in C.

REQUIRES:
---------

* libiconv

AUTHOR:
-------

	mattn

HOW TO DO:
----------

	# ./tthttpd -p 8080 -d path-to-root

or

	# cat my-httpd.conf
	[global]
	port=8080
	root=/path/to/contents
	indexpages=index.html,index.php
	charset=cp932

	[mime/types]
	cgi=@/usr/bin/perl
	php=@/usr/bin/php
	
	# ./tthttpd -c my-httpd.conf

SCREEN SHOT:
------------

![MTOS working](http://farm4.static.flickr.com/3305/3597469456_1f5210975f_o.png "MTOS Working")

![WordPress working] (http://farm4.static.flickr.com/3643/3603370389_4d4bc5c9c3_o.png "WordPress Working")
