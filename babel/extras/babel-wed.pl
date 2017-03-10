#! perl
# babel-wed.pl
# L. Ross Raszewski
# usage: babel-wed.pl [babel cache directory] file
#
# tries to find ifiction and cover art, then blorbs it with
# the story file.

if ($#ARGV<0)
{
 print "usage: babel-wed [ancillary data directory] story-file\n";
 exit(0);
}

$DIR_SEP="\\";
if ($#ARGV)
 { $fromdir=$ARGV[0]; }
else
 { $fromdir="."; }
$infile=$ARGV[$#ARGV];
$str = "babel -ifid $infile";
$ifid = `$str`;
$ifid =~ s/IFID: //;
$rfn=$infile;
$rfn =~ s/\.(.+)$//;
chomp($ifid);
if (-e "$fromdir$DIR_SEP$ifid.iFiction")
{
$ifi="$fromdir$DIR_SEP$ifid.iFiction";
}
elsif (-e "$fromdir$DIR_SEP$rfn.iFiction")
{
$ifi="$fromdir$DIR_SEP$rfn.iFiction";
}
else
{
die("Could not find ifiction file.");
}
if (-e "$fromdir$DIR_SEP$ifid.png")
{ $cvr="$fromdir$DIR_SEP$ifid.png"; }
elsif (-e "$fromdir$DIR_SEP$ifid.jpg")
{ $cvr="$fromdir$DIR_SEP$ifid.jpg"; }
elsif (-e "$fromdir$DIR_SEP$rfn.png")
{ $cvr="$fromdir$DIR_SEP$rfn.png"; }
elsif (-e "$fromdir$DIR_SEP$rfn.jpg")
{ $cvr="$fromdir$DIR_SEP$rfn.jpg"; }

else
{ $cvr=""; }

$str = "babel -blorbs $infile $ifi $cvr";
print "Executing '$str'\n";
system($str);


