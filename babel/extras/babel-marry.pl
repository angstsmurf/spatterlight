#! /usr/bin/perl
# babel-marry.pl
# Copyright 2006 by L. Ross Raszewski
#
# A demonstration of using babel with pipes
#
# This program is the converse of babel-cache; it scans every
# story file in the directory and creates a blorb file containing
# metadata and cover art for each
#
# This could be used in tandem with a story browser to prefetch
# ifiction records and cover art.
#
# Requires babel, babel-get
#

# Customise the next six lines to control operation:

$ROUTE_TO_BABEL="babel";
$ROUTE_TO_BABEL_GET="babel-get";
# customize these if the babel suite isn't in your path

$SENSIBLE_NAME=1;
# set to 0 to leave output file with an IFID-based name
$BABEL_SERVER="http://babel.ifarchive.org";
 # the ifiction server.  Set to 0 to disable
$FILE_MODE = "babel.ifiction";
 # If this is set to a file name, ifiction for every game in the directory
 # will sought from the named file.  Set this to zero to use
 # separate files for each game
$CACHE_DIR = ".";
 # If FILE_MODE is zero *or* the ifiction record is not found in that file,
 # This directory will be checked for the IFID.ifiction file corresponding
 # to the game file
$DIR_SEP = "/";


@stories= <*>;
foreach $story (@stories)
{
next if (-d $story);
next if $story =~ m/.iFiction$/i;
next if $story =~ m/.png$/i;
next if $story =~ m/.jpg$/i;
next if $story =~ m/.[zg]?blorb$/i;
$rfn = $story;
$rfn =~ s/\.(.+)$//;
#print "Trying $story\n";
$fmt=`$ROUTE_TO_BABEL -format $story`;
#print $fmt, "\n";
next if $fmt =~ m/Error:/;
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

if (($ifiction =~ m/^Requested information/) &&$FILE_MODE)
{
$ifiction=`$ROUTE_TO_BABEL_GET -ifiction $ifid -ifiction $FILE_MODE`;
}
if ($ifiction =~ m/^Requested information/)
{
$ifiction=`$ROUTE_TO_BABEL_GET -ifiction $ifid -dir $CACHE_DIR`;
}
if (($ifiction =~ m/^Requested information/) && -e "$CACHE_DIR$DIR_SEP$rfn.iFiction" )
{
 open(FH, "<$CACHE_DIR$DIR_SEP$rfn.iFiction");
 @lines=<FH>;
 $ifiction=join("",@lines);
 undef @lines;
 close(FH);
}
if ($ifiction =~ m/^Requested information/ && $BABEL_SERVER)
{ $ifiction=`$ROUTE_TO_BABEL_GET -ifiction $ifid -url $BABEL_SERVER`;

}


if (!($ifiction =~ m/^Requested information/))
{
 open(FH, ">temp.ifiction");
 print FH $ifiction;
 close(FH);
 $cf="";
 $cd=1;
 $cf=`$ROUTE_TO_BABEL_GET -cover $ifid -story $story`;
 if ($cf =~ m/Requested information/)
 {
     if (-e "$CACHE_DIR$DIR_SEP$ifid.jpg")
     { $cd=0; $cf="$CACHE_DIR$DIR_SEP$ifid.jpg"; }
    elsif (-e "$CACHE_DIR$DIR_SEP$ifid.png")
     { $cd=0; $cf="$CACHE_DIR$DIR_SEP$ifid.png"; }
    elsif (-e "$CACHE_DIR$DIR_SEP$rfn.png")
     { $cd=0; $cf="$CACHE_DIR$DIR_SEP$rfn.png"; }
    elsif (-e "$CACHE_DIR$DIR_SEP$rfn.jpg")
     { $cd=0; $cf="$CACHE_DIR$DIR_SEP$rfn.jpg"; }

  if ($cd && $BABEL_SERVER)
   {
    $cf=`$ROUTE_TO_BABEL_GET -cover $ifid -url $BABEL_SERVER`;
    if ($cf =~ m/Requested information/)
    {
     $cd=0;
     $cf="";
    }
   }

 }

 if ($cd) {
 if (-e "$ifid.jpg") { $cf="$ifid.jpg"; }
 else { $cf="$ifid.png"; }
 } 
# print "$ROUTE_TO_BABEL -blorb $story temp.ifiction $cf\n"; 
 print `$ROUTE_TO_BABEL -blorb $story temp.ifiction $cf`;
 if ($cd) { unlink($cf); }
 if ($SENSIBLE_NAME)
 {
  rename("$ifid.blorb","$rfn.blorb") if (-e "$ifid.blorb");
  rename("$ifid.gblorb","$rfn.gblorb") if (-e "$ifid.gblorb");
  rename("$ifid.zblorb","$rfn.zblorb") if (-e "$ifid.zblorb");
 }
 unlink("temp.ifiction");

}
else
{
 print ": No metadata found";
}
print "\n";
}
