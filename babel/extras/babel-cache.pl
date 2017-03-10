#! /usr/bin/perl
# babel-cache.pl
# Copyright 2006 by L. Ross Raszewski
#
# A demonstration of using babel with pipes
#
# This program scans the current directory for story files, and
# attempts to build an ifiction database.
#
# This could be used in tandem with a story browser to prefetch
# ifiction records and cover art.
#
# Requires babel, babel-get, and ifiction-aggregate
#

# Customise the next six lines to control operation:


$ROUTE_TO_BABEL="babel";
$ROUTE_TO_BABEL_GET="babel-get";
$ROUTE_TO_IFICTION_AGGREGATE="ifiction-aggregate";

$BABEL_SERVER="http://babel.ifarchive.org";
 # the ifiction server
$FILE_MODE = "babel.ifiction";
 # If this is set to a file name, ifiction for every game in the directory
 # will be aggregated into the named file.  Set this to zero to use
 # separate files for each game
$CACHE_DIR = "/babel";
 # if FILE_MODE is 0, $CACHE_DIR/ifiction/<IFID>.iFiction will be
 # created for each file in the directory
$DIR_SEP = "/";


@stories= <*>;
foreach $story (@stories)
{
$fmt=`$ROUTE_TO_BABEL -format $story`;
next if $fmt =~ m/non-authoritative/;
next if $fmt =~ m/Format: executable/;
next if !($fmt =~ m/.+/);

$ifid=`$ROUTE_TO_BABEL -ifid $story`;
$ifid =~ s/^IFID: //;
$ifid =~ s/IFID: .+//;
$ifid =~ s/\n//;
chomp($ifid);
print "$story ($ifid)";
$ifiction=`$ROUTE_TO_BABEL_GET -ifiction $ifid -story $story`;
if (!($ifiction =~ m/.+/))
{ $ifiction=`$ROUTE_TO_BABEL_GET -ifiction $ifid -url $BABEL_SERVER`;

}
$oc=`$ROUTE_TO_BABEL_GET -cover $ifid -story $story -to $CACHE_DIR`;
if ($oc =~ m/Requested information/)
{ $ifiction=`$ROUTE_TO_BABEL_GET -cover $ifid -url $BABEL_SERVER -to $CACHE_DIR`;

}


if (($ifiction =~ m/.+/))
{
if ($FILE_MODE)
{
 open(FH,"|$ROUTE_TO_IFICTION_AGGREGATE $FILE_MODE");
}
else
{
 open(FH,">$CACHE_DIR$DIR_SEP$ifid.iFiction");
}
print FH $ifiction;
close(FH);
}
else
{
 print ": No metadata found";
}
print "\n";
}
