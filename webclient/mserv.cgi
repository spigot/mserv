#!/usr/bin/perl

$guestuser = 'guest';
$guestpass = 'guest';
$host = 'fox.oaktree.net.uk:4444';
$mservcmd = '/usr/local/bin/mservcmd';

use POSIX;
use CGI;
use CGI::Carp qw(fatalsToBrowser);
$query = new CGI;

$page = $query->param('page');
$user = clean($query->param('user'));
$pass = clean($query->param('pass'));

if ($user eq 'root') {
  $user = 'notroot';
}

if ($user && $pass) {
    $up = "user=$user&pass=$pass&";
    $cmd = "$mservcmd -u $user -p $pass -h $host";
} else {
    $user = $pass = '';
    $up = "";
    $cmd = "$mservcmd -u $guestuser -p $guestpass -h $host";
}

print <<EOF;
Content-type: text/html

<html>
<head>
  <title>Mserv</title>
</head>
<body bgcolor="#ffffff" background="back.gif">
EOF

if ($page eq 'status') {
    status();
} elsif ($page eq 'albums') {
    albums();
} elsif ($page eq 'album') {
    my $album = int($query->param('album'));
    album($album);
} elsif ($page eq 'track') {
    my $album = int($query->param('album'));
    my $track = int($query->param('track'));
    track($album, $track);
} elsif ($page eq 'queue') {
    queue();
} elsif ($page eq 'queueadd') {
    my $album = int($query->param('album'));
    my $track = int($query->param('track'));
    queueadd($album, $track);
} elsif ($page eq 'unqueue') {
    my $album = int($query->param('album'));
    my $track = int($query->param('track'));
    unqueue($album, $track);
} elsif ($page eq 'history') {
    history();
} elsif ($page eq 'top') {
    top();
} elsif ($page eq 'authors') {
    authors();
} elsif ($page eq 'author') {
    my $author = int($query->param('author'));
    author($author);
} elsif ($page eq 'genres') {
    genres();
} elsif ($page eq 'genre') {
    my $genre = int($query->param('genre'));
    genre($genre);
} elsif ($page eq 'rate') {
    my $album = int($query->param('album'));
    my $track = int($query->param('track'));
    my $rate = clean($query->param('rate'));
    rate($album, $track, $rate);
} elsif ($page eq 'who') {
    who();
} elsif ($page eq 'random') {
    my $state = clean($query->param('state'));
    random($state);
} elsif ($page eq 'play') {
    play();
} elsif ($page eq 'pause') {
    pause();
} elsif ($page eq 'stop') {
    stop();
} elsif ($page eq 'next') {
    mynext();
} else {
    print <<EOF;
    <h1>Unknown request</h1>
    <p>The page request was not understood, sorry!</p>
EOF
}
print "<hr>\n";
print '<address><a href="http://www.mserv.org/">http://www.mserv.org/</a>'.
    "</address>\n";
print "</body></html>\n";

sub clean {
    my $text = shift;

    $text =~ s/[^a-zA-Z0-9]//g;
    return $text;
}

sub data {
    my $cmd = shift;
    my @a;

    open(DATA, "$cmd|") || die "Unable to $cmd: $!\n";
    while(<DATA>) {
	chomp;
	push @a, $_;
    }
    if (!close(DATA)) {
	if (!$!) {
	    $exit = $? >> 8;
	    if ($exit == 4) {
		print "<b>Access denied - incorrect user or password</b>\n";
		print "</body></html>\n";
		exit();
	    } else {
		print "<b>Internal error ($exit)</b>\n";
		print "</body></html>\n";
		exit();
	    }
	} else {
	    die "mservcmd returned exit status $? ($!) @ ".ctime(time());
	}
    }
    return @a;
}

sub namevalue {
    my ($name, $value) = @_;
    print "  <tr><td width=\"25%\" bgcolor=\"#bbeeff\">$name</td>";
    if ($value ne "") {
	print "<td width=\"75%\">$value</td></tr>\n";
    } else {
	print "<td width=\"75%\">&nbsp;</td></tr>\n";
    }
}

sub row {
    $col = shift(@_);
    print "  <tr>";
    while(defined($a = shift(@_))) {
	if ($col) {
	    print "<td bgcolor=\"\#$col\">$a</td>";
	} else {
	    print "<td>$a</td>";
	}
    }
    print "</tr>\n";
}

sub status {
    my $b;
    @data = data("$cmd 'status'");
    $response = shift(@data);
    ($code, $responsestr) = split(/\t/, $response);
    if ($code == 224) {
	$str = "Nothing playing";
    } elsif ($code == 222) {
	$str = "Playing";
    } elsif ($code == 223) {
	$str = "Paused";
    } else {
	print "<p>Unknown reply: $code</p>\n";
	print "<p><b>$responsestr</p>\n";
	return;
    }
    $info = shift(@data);
    @info = split(/\t/, $info);
    print "<h1>Status of server</h1>\n";
    print "<center>\n";
    print "<table width=400 cellpadding=2 bgcolor=\"#eeeeee\" border=1>\n";
    namevalue("Status", $str);
    if ($code == 222 || $code == 223) {
	namevalue("Track", '<a href="mserv.cgi?'.$up.'page=track&album='.
		  $info[3].'&track='.$info[4].'">'.
		  $info[3].' / '.$info[4].'</a>');
	namevalue("Name", $info[6]);
	namevalue("Author", $info[5]);
	namevalue("Commenced play", ctime($info[7])." (".$info[8].")");
	$b = 6;
    } elsif ($code == 224) {
	$b = 0;
    } else {
	print "<p>Unknown reply: $code</p>\n";
	print "<p><b>$responsestr</p>\n";
	return;
    }
    namevalue("Filter", $info[0] eq "" ? "off" : $info[0]);
    namevalue("... included", $info[1]);
    namevalue("... excluded", $info[2]);
    if (lc($info[$b+3]) eq 'on') {
	namevalue("Random", lc($info[$b+3])." (".$info[$b+4].'), '.
		  '<a href="mserv.cgi?'.$up.'page=random&state=off">turn '.
		  'off</a>');
    } else {
	namevalue("Random", lc($info[$b+3])." (".$info[$b+4].'), '.
		  '<a href="mserv.cgi?'.$up.'page=random&state=on">turn '.
		  'on</a>');
    }
    print "</table>\n";
    if ($user) {
	print '<p><a href="mserv.cgi?'.$up."page=play\">PLAY</a>";
	print ' | <a href="mserv.cgi?'.$up."page=pause\">PAUSE</a>";
	print ' | <a href="mserv.cgi?'.$up."page=stop\">STOP</a>";
	print ' | <a href="mserv.cgi?'.$up."page=next\">NEXT</a>";
	print "</p>\n";
    }
    print "</center>\n";
}

sub albums {
    @data = data("$cmd 'albums'");
    $response = shift(@data);
    ($code, $responsestr) = split(/\t/, $response);
    if ($code != 227) {
	print "<p>Unknown reply: $code</p>\n";
	print "<p><b>$responsestr</p>\n";
	return;
    }
    print "<h1>Albums available</h1>\n";
    print "<center>\n";
    print "<table width=550 cellpadding=2 bgcolor=\"#eeeeee\" border=1>\n";
    row("bbeeff", "Album \#", "Author", "Name");
    while($line = shift(@data)) {
	@a = split(/\t/, $line);
	row("", '<a href="mserv.cgi?'.$up.'page=album&album='.
	    $a[0].'">'.$a[0].'</a>', $a[1], $a[2]);
    }
    print "</table></center>\n";
}

sub album {
    my $album = shift;
    @data = data("$cmd 'tracks $album'");
    $response = shift(@data);
    ($code, $responsestr) = split(/\t/, $response);
    if ($code == 508) {
	print "<p><b>$responsestr</p>\n";
	return;
    } elsif ($code != 228) {
	print "<p>Unknown reply: $code</p>\n";
	print "<p><b>$responsestr</p>\n";
	return;
    }
    $info = shift(@data);
    @info = split(/\t/, $info);
    print "<h1>Album $album</h1>\n";
    print "<center>\n";
    print "<table width=400 cellpadding=2 bgcolor=\"#eeeeee\" border=1>\n";
    namevalue("Album \#", $info[0]);
    namevalue("Name", '<b>'.$info[2].'</b>');
    namevalue("Author", '<b>'.$info[1].'</b>');
    print "</table></center>\n";
    print "<br><center>\n";
    print "<table width=550 cellpadding=2 bgcolor=\"#eeeeee\" border=1>\n";
    if ($user) {
	row("bbeeff", "Track \#", "Author", "Name", "Rating");
    } else {
	row("bbeeff", "Track \#", "Author", "Name");
    }
    while($line = shift(@data)) {
	@a = split(/\t/, $line);
	if ($user) {
	    row("", '<a href="mserv.cgi?'.$up.'page=track&album='.
		$a[0].'&track='.$a[1].'">'.$a[0].' / '.$a[1].'</a>',
		$a[2], $a[3], $a[4]);
	} else {
	    row("", '<a href="mserv.cgi?'.$up.'page=track&album='.
		$a[0].'&track='.$a[1].'">'.$a[0].' / '.$a[1].'</a>',
		$a[2], $a[3]);
	}
    }
    print "</table></center>\n";
}

sub track {
    my $album = shift;
    my $track = shift;
    @data = data("$cmd 'info $album $track'");
    $response = shift(@data);
    ($code, $responsestr) = split(/\t/, $response);
    if ($code == 508 || $code == 509) {
	print "<p><b>$responsestr</p>\n";
	return;
    } elsif ($code != 246) {
	print "<p>Unknown reply: $code</p>\n";
	print "<p><b>$responsestr</p>\n";
	return;
    }
    $info = shift(@data);
    @info = split(/\t/, $info);
    print "<h1>Track $album/$track</h1>\n";
    print "<center>\n";
    print "<table width=400 cellpadding=2 bgcolor=\"#eeeeee\" border=1>\n";
    namevalue("Album \#", '<a href="mserv.cgi?'.$up.'page=album&album='.
	      $info[0].'">'.$info[0].'</a>');
    namevalue("Name", $info[3]);
    namevalue("Author", $info[2]);
    print "</table></center>\n";
    print "<br><center>\n";
    print "<table width=400 cellpadding=2 bgcolor=\"#eeeeee\" border=1>\n";
    namevalue("Track \#", $info[1]);
    namevalue("Name", '<b>'.$info[5].'</b>');
    namevalue("Author", '<b>'.$info[4].'</b>');
    namevalue("Dated", $info[6]);
    namevalue("Duration", $info[14]. "(".$info[15].")");
    namevalue("Genre", $info[12]);
    namevalue("Played", $info[8]);
    namevalue("Filter", $info[13]);
    if ($user) {
	namevalue("Rated", $info[11].'; mean '.$info[9].'<br>'.
		  'temporally adjusted to '.$info[10]);
    } else {
	namevalue("Rated", 'mean '.$info[9].'<br>'.
		  'temporally adjusted to '.$info[10]);
    }
    print "</table></center>\n";
    @data = data("$cmd 'ratings $album $track'");
    $response = shift(@data);
    ($code, $responsestr) = split(/\t/, $response);
    if ($code == 508 || $code == 509) {
	print "<p><b>$responsestr</p>\n";
	return;
    } elsif ($code != 229) {
	print "<p>Unknown reply: $code</p>\n";
	print "<p><b>$responsestr</p>\n";
	return;
    }
    print "<center>\n";
    $currate = "";
    $info = shift(@data);
    @info = split(/\t/, $info);
    while($line = shift(@data)) {
	if (!$flag) {
	    print "<br>\n";
	    print "<table width=300 cellpadding=2 bgcolor=\"#eeeeee\" ".
		"border=1>\n";
	    row("bbeeff", "User", "Rating");
	    $flag = 1;
	}
	@a = split(/\t/, $line);
	if ($a[0] eq $user) {
	    row("", $a[0], '<b>'.$a[1].'</b>');
	    $currate = $a[1];
	} else {
	    row("", $a[0], $a[1]);
	}
    }
    if ($flag) {
	print "</table>\n";
    }
    if ($user) {
	print "<p>Rate: ";
	foreach $a ("AWFUL", "BAD", "NEUTRAL", "GOOD", "SUPERB") {
	    if ($currate eq $a) {
		print "$a | ";
	    } else {
		print '<a href="mserv.cgi?'.$up."page=rate&album=$album&".
		    "track=$track&rate=$a\">$a</a>";
		if ($a ne "SUPERB") {
		    print " |\n";
		}
	    }
	}
	print "</p>\n";
    }
    if ($user) {
	print '<p><a href="mserv.cgi?'.$up."page=queueadd&album=$album&".
	    "track=$track\">QUEUE SONG</a></p>\n";
    }
    print "</center>\n";
}

sub queue {
    @data = data("$cmd 'queue'");
    $response = shift(@data);
    ($code, $responsestr) = split(/\t/, $response);
    print "<h1>Queue</h1>\n";
    if ($code == 404) {
	print "<p>$responsestr</p>\n";
	return;
    }
    if ($code != 225) {
	print "<p>Unknown reply: $code</p>\n";
	print "<p><b>$responsestr</p>\n";
	return;
    }
    print "<center>\n";
    print "<table width=550 cellpadding=2 bgcolor=\"#eeeeee\" border=1>\n";
    if ($user) {
	row("bbeeff", "", "Queued by", "Track \#", "Author", "Name", "Rating");
    } else {
	row("bbeeff", "Queued by", "Track \#", "Author", "Name");
    }
    while($line = shift(@data)) {
	@a = split(/\t/, $line);
	if ($user) {
	    row("", '<a href="mserv.cgi?'.$up.'page=unqueue&album='.$a[1].
		'&track='.$a[2].'">unqueue</a>', $a[0],
		'<a href="mserv.cgi?'.$up.'page=track&album='.
		$a[1].'&track='.$a[2].'">'.$a[1].' / '.$a[2].'</a>',
		$a[3], $a[4], $a[5]);
	} else {
	    row("", $a[0],
		'<a href="mserv.cgi?'.$up.'page=track&album='.
		$a[1].'&track='.$a[2].'">'.$a[1].' / '.$a[2].'</a>',
		$a[3], $a[4]);
	}
    }
    print "</table></center>\n";
}

sub queueadd {
    my $album = shift;
    my $track = shift;
    @data = data("$cmd 'queue $album $track'");
    $response = shift(@data);
    ($code, $responsestr) = split(/\t/, $response);
    if ($code == 507) {
	print "<p>$responsestr</p>\n";
	return;
    }
    if ($code != 247) {
	print "<p>Unknown reply: $code</p>\n";
	print "<p><b>$responsestr</p>\n";
	return;
    }
    queue();
}

sub unqueue {
    my $album = shift;
    my $track = shift;
    @data = data("$cmd 'unqueue $album $track'");
    $response = shift(@data);
    ($code, $responsestr) = split(/\t/, $response);
    if ($code == 508 || $code == 509 || $code == 409) {
	print "<p><b>$responsestr</p>\n";
	return;
    }
    if ($code != 254) {
	print "<p>Unknown reply: $code</p>\n";
	print "<p><b>$responsestr</p>\n";
	return;
    }
    queue();
}

sub history {
    @data = data("$cmd 'history'");
    $response = shift(@data);
    ($code, $responsestr) = split(/\t/, $response);
    print "<h1>History</h1>\n";
    if ($code != 241) {
	print "<p>Unknown reply: $code</p>\n";
	print "<p><b>$responsestr</p>\n";
	return;
    }
    print "<center>\n";
    print "<table width=550 cellpadding=2 bgcolor=\"#eeeeee\" border=1>\n";
    if ($user) {
	row("bbeeff", "Queued by", "Track \#", "Author", "Name", "Rating");
    } else {
	row("bbeeff", "Queued by", "Track \#", "Author", "Name");
    }
    while($line = shift(@data)) {
	@a = split(/\t/, $line);
	if ($user) {
	    row("", $a[0], '<a href="mserv.cgi?'.$up.'page=track&album='.
		$a[1].'&track='.$a[2].'">'.$a[1].' / '.$a[2].'</a>',
		$a[3], $a[4], $a[5]);
	} else {
	    row("", $a[0], '<a href="mserv.cgi?'.$up.'page=track&album='.
		$a[1].'&track='.$a[2].'">'.$a[1].' / '.$a[2].'</a>',
		$a[3], $a[4]);
	}
    }
    print "</table></center>\n";
}

sub top {
    @data = data("$cmd 'top 15'");
    $response = shift(@data);
    ($code, $responsestr) = split(/\t/, $response);
    print "<h1>Top</h1>\n";
    if ($code != 234) {
	print "<p>Unknown reply: $code</p>\n";
	print "<p><b>$responsestr</p>\n";
	return;
    }
    print "<p>$responsestr</p>\n";
    print "<center>\n";
    print "<table width=550 cellpadding=2 bgcolor=\"#eeeeee\" border=1>\n";
    if ($user) {
	row("bbeeff", "Probability", "Track \#", "Author", "Name", "Rating");
    } else {
	row("bbeeff", "Probability", "Track \#", "Author", "Name");
    }
    while($line = shift(@data)) {
	@a = split(/\t/, $line);
	if ($user) {
	    row("", $a[0], '<a href="mserv.cgi?'.$up.'page=track&album='.
		$a[1].'&track='.$a[2].'">'.$a[1].' / '.$a[2].'</a>',
		$a[3], $a[4], $a[5]);
	} else {
	    row("", $a[0], '<a href="mserv.cgi?'.$up.'page=track&album='.
		$a[1].'&track='.$a[2].'">'.$a[1].' / '.$a[2].'</a>',
		$a[3], $a[4]);
	}
    }
    print "</table></center>\n";
}

sub authors {
    @data = data("$cmd 'x authors'");
    $response = shift(@data);
    ($code, $responsestr) = split(/\t/, $response);
    if ($code != 250) {
	print "<p>Unknown reply: $code</p>\n";
	print "<p><b>$responsestr</p>\n";
	return;
    }
    print "<h1>Authors available</h1>\n";
    print "<center>\n";
    print "<table width=550 cellpadding=2 bgcolor=\"#eeeeee\" border=1>\n";
    row("bbeeff", "Author", "Tracks");
    while($line = shift(@data)) {
	@a = split(/\t/, $line);
	row("", '<a href="mserv.cgi?'.$up.'page=author&author='.
	    $a[0].'">'.$a[1].'</a>', $a[2]);
    }
    print "</table></center>\n";
}

sub author {
    my $author = shift;
    @data = data("$cmd 'x authortracks $author'");
    $response = shift(@data);
    ($code, $responsestr) = split(/\t/, $response);
    if ($code == 526) {
	print "<p><b>$responsestr</p>\n";
	return;
    } elsif ($code != 252) {
	print "<p>Unknown reply: $code</p>\n";
	print "<p><b>$responsestr</p>\n";
	return;
    }
    $info = shift(@data);
    @info = split(/\t/, $info);
    print "<h1>Author $info[1]</h1>\n";
    print "<center>\n";
    print "<table width=550 cellpadding=2 bgcolor=\"#eeeeee\" border=1>\n";
    if ($user) {
	row("bbeeff", "Track \#", "Author", "Name", "Rating");
    } else {
	row("bbeeff", "Track \#", "Author", "Name");
    }
    while($line = shift(@data)) {
	@a = split(/\t/, $line);
	if ($user) {
	    row("", '<a href="mserv.cgi?'.$up.'page=track&album='.
		$a[1].'&track='.$a[2].'">'.$a[1].' / '.$a[2].'</a>',
		$a[3], $a[4], $a[5]);
	} else {
	    row("", '<a href="mserv.cgi?'.$up.'page=track&album='.
		$a[1].'&track='.$a[2].'">'.$a[1].' / '.$a[2].'</a>',
		$a[3], $a[4]);
	}
    }
    print "</table></center>\n";
}

sub rate {
    my $album = shift;
    my $track = shift;
    my $rate = shift;
    @data = data("$cmd 'rate $album $track $rate'");
    $response = shift(@data);
    ($code, $responsestr) = split(/\t/, $response);
    if ($code == 508 || $code == 509) {
	print "<p><b>$responsestr</p>\n";
	return;
    }
    if ($code != 270) {
	print "<p>Unknown reply: $code</p>\n";
	print "<p><b>$responsestr</p>\n";
	return;
    }
    track($album, $track);
}

sub who {
    @data = data("$cmd 'who'");
    $response = shift(@data);
    ($code, $responsestr) = split(/\t/, $response);
    if ($code == 500) {
	if ($user) {
	    print "<p><b>You ($user) are not permitted to perform this ".
		"action</p>\n";
	    return;
	} else {
	    print "<p><b>Guests are not permitted to perform this ".
		"action</p>\n";
	    return;
	}
    } elsif ($code != 226) {
	print "<p>Unknown reply: $code</p>\n";
	print "<p><b>$responsestr</p>\n";
	return;
    }
    print "<h1>$responsestr</h1>\n";
    print "<center>\n";
    print "<table width=550 cellpadding=2 bgcolor=\"#eeeeee\" border=1>\n";
    row("bbeeff", "User", "Level", "Idle", "Connected from");
    while($line = shift(@data)) {
	@a = split(/\t/, $line);
	row("", $a[0], $a[1], $a[2], $a[3]);
    }
    print "</table></center>\n";
}

sub genres {
    @data = data("$cmd 'x genres'");
    $response = shift(@data);
    ($code, $responsestr) = split(/\t/, $response);
    if ($code != 271) {
	print "<p>Unknown reply: $code</p>\n";
	print "<p><b>$responsestr</p>\n";
	return;
    }
    print "<h1>Genres available</h1>\n";
    print "<center>\n";
    print "<table width=550 cellpadding=2 bgcolor=\"#eeeeee\" border=1>\n";
    row("bbeeff", "Genre", "Tracks");
    while($line = shift(@data)) {
	@a = split(/\t/, $line);
	$a[1] = "none" if ($a[1] eq "");
	row("", '<a href="mserv.cgi?'.$up.'page=genre&genre='.
	    $a[0].'">'.$a[1].'</a>', $a[2]);
    }
    print "</table></center>\n";
}

sub genre {
    my $genre = shift;
    @data = data("$cmd 'x genretracks $genre'");
    $response = shift(@data);
    ($code, $responsestr) = split(/\t/, $response);
    if ($code == 536) {
	print "<p><b>$responsestr</p>\n";
	return;
    } elsif ($code != 272) {
	print "<p>Unknown reply: $code</p>\n";
	print "<p><b>$responsestr</p>\n";
	return;
    }
    $info = shift(@data);
    @info = split(/\t/, $info);
    print "<h1>Tracks in genre $info[1]</h1>\n";
    print "<center>\n";
    print "<table width=550 cellpadding=2 bgcolor=\"#eeeeee\" border=1>\n";
    if ($user) {
	row("bbeeff", "Track \#", "Author", "Name", "Rating");
    } else {
	row("bbeeff", "Track \#", "Author", "Name");
    }
    while($line = shift(@data)) {
	@a = split(/\t/, $line);
	if ($user) {
	    row("", '<a href="mserv.cgi?'.$up.'page=track&album='.
		$a[1].'&track='.$a[2].'">'.$a[1].' / '.$a[2].'</a>',
		$a[3], $a[4], $a[5]);
	} else {
	    row("", '<a href="mserv.cgi?'.$up.'page=track&album='.
		$a[1].'&track='.$a[2].'">'.$a[1].' / '.$a[2].'</a>',
		$a[3], $a[4]);
	}
    }
    print "</table></center>\n";
}

sub random {
    my $state = shift;
    if (lc($state) eq 'on') {
	@data = data("$cmd 'random on'");
    } else {
	@data = data("$cmd 'random off'");
    }
    $response = shift(@data);
    ($code, $responsestr) = split(/\t/, $response);
    if ($code == 507 && !$user) {
	print "<p><b>Guests are not permitted to perform this action</p>\n";
	return;
    } elsif ($code != 267 && $code != 410) {
	print "<p>Unknown reply: $code</p>\n";
	print "<p><b>$responsestr</p>\n";
	return;
    }
    status();
}

sub play {
    @data = data("$cmd 'play'");
    $response = shift(@data);
    ($code, $responsestr) = split(/\t/, $response);
    if (($code == 507 || $code == 500) && !$user) {
	print "<p><b>Guests are not permitted to perform this action</p>\n";
	return;
    } elsif ($code == 402 || $code == 403) {
	print "<p>$responsestr</p>\n";
	status();
	return;
    } elsif ($code != 230) {
	print "<p>Unknown reply: $code</p>\n";
	print "<p><b>$responsestr</p>\n";
	return;
    }
    print "<p>Play commenced</p>\n";
    status();
}

sub pause {
    @data = data("$cmd 'pause'");
    $response = shift(@data);
    ($code, $responsestr) = split(/\t/, $response);
    if (($code == 507 || $code == 500) && !$user) {
	print "<p><b>Guests are not permitted to perform this action</p>\n";
	return;
    } elsif ($code == 401 || $code == 406) {
	print "<p>$responsestr</p>\n";
	&status();
	return;
    } elsif ($code != 232) {
	print "<p>Unknown reply: $code</p>\n";
	print "<p><b>$responsestr</p>\n";
	return;
    }
    print "<p>Play paused</p>\n";
    &status();
}

sub stop {
    @data = data("$cmd 'stop'");
    $response = shift(@data);
    ($code, $responsestr) = split(/\t/, $response);
    if (($code == 507 || $code == 500) && !$user) {
	print "<p><b>Guests are not permitted to perform this action</p>\n";
	return;
    } elsif ($code == 401) {
	print "<p>$responsestr</p>\n";
	&status();
	return;
    } elsif ($code != 231) {
	print "<p>Unknown reply: $code</p>\n";
	print "<p><b>$responsestr</p>\n";
	return;
    }
    print "<p>Play stopped</p>\n";
    &status();
}

sub mynext {
    @data = data("$cmd 'next'");
    $response = shift(@data);
    ($code, $responsestr) = split(/\t/, $response);
    if (($code == 507 || $code == 500) && !$user) {
	print "<p><b>Guests are not permitted to perform this action</p>\n";
	return;
    } elsif ($code == 403) {
	print "<p>$responsestr</p>\n";
	status();
	return;
    } elsif ($code != 240) {
	print "<p>Unknown reply: $code</p>\n";
	print "<p><b>$responsestr</p>\n";
	return;
    }
    print "<p>Moved to next track</p>\n";
    &status();
}
