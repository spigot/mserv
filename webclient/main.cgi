#!/usr/bin/perl

use CGI;
use CGI::Carp qw(fatalsToBrowser);
$query = new CGI;

# the following parameters are untrusted
$user = $query->param('user');
$pass = $query->param('pass');

print <<EOF;
Content-Type: text/html

<html>
<head>
  <title>Mserv web interface</title>
</head>
<frameset cols="95,*" border=0>
  <frame src="margin.cgi?user=$user&pass=$pass" frameborder=0 scrolling=auto>
  <frame name=main src="mserv.cgi?user=$user&pass=$pass&page=status" frameborder=0 scrolling=auto>
  <noframes>
    <p>No frame support?  You should go to the
    <a href="margin.cgi?user=$user&pass=$pass">margin</a>.</p>
  </noframes>
</frameset>
</html>
EOF
