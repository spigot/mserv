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
  <title>Mserv web interface - margin</title>
</head>
<body bgcolor="#ffffff" background="back.gif">
<p align=center><center>
<table>
<tr><td>
<p align=center><img src="mserv7.gif" alt="Mserv"></p>
<p><font size="2"><a target=main href="mserv.cgi?user=$user&pass=$pass&page=status">Status</a>
<br><a target=main href="mserv.cgi?user=$user&pass=$pass&page=albums">Albums</a>
<br><a target=main href="mserv.cgi?user=$user&pass=$pass&page=queue">Queue</a>
<br><a target=main href="mserv.cgi?user=$user&pass=$pass&page=history">History</a>
<br><a target=main href="mserv.cgi?user=$user&pass=$pass&page=top">Top</a>
<br><a target=main href="mserv.cgi?user=$user&pass=$pass&page=authors">Authors</a>
<br><a target=main href="mserv.cgi?user=$user&pass=$pass&page=genres">Genres</a>
EOF

if ($user) {
    print ('<br><a target=main href="mserv.cgi?user=',
	   "$user&pass=$pass", '&page=who">Who</a></font></p>');
}

print <<EOF;
</td></tr>
</table>
</center></p>
</body>
</html>
EOF
