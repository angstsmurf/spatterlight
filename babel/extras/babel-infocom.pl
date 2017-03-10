#! perl
# Babel-infocom is a special-purpose script for bundling infocom games
#
# usage: babel-infocom.pl [-from dir] file[s]
#
#
# If specified, it will look in the -from dir for the infocom
# metadata collection.  the from-directory should end in your platform's
# directory separator (so "babel-infocom.pl -from data/ foo.z5")
#
# This program will search the infocom metadata collection for
# the corresponding metadata for your game, and create a
# blorb bundle containing cover art and metadata.
#
# Requires babel, babel-get, and ifiction-xtract from babel.
#
# For the infocom games with multimedia, babel-infocom will also combine
# the story file with its multimedia resources.  This functionality requires
# bmerge, which is available at http://www.trenchcoatsoft.com/downloads/iblorbs.zip
#


$held_mode=0;
$fromdir= "";
$BABEL = "babel";
$BMERGE = "bmerge";
$IFIX = "ifiction-xtract";
$BABELGET = "babel-get";

foreach $file (@ARGV)
{
 if ($file eq "-from")
 {
  $held_mode=1;
  next;
 }
 if ($held_mode)
 {
  $fromdir=$file;
  $held_mode=0;
  next;
 }
next if $file =~ m/.iFiction$/i;
next if $file =~ m/.png$/i;
next if $file =~ m/.jpg$/i;
next if $file =~ m/.[zg]?blorb$/i;

$fmt = `$BABEL -format $file`;
next if $fmt =~ m/Error:/;
next if $fmt =~ m/non-authoritative/;
next if $fmt =~ m/Format: executable/;
next if !($fmt =~ m/.+/);

print "$file: ";

$ifid = `$BABEL -ifid $file`;
$ifid =~ s/^IFID: //;
$ifid =~ s/IFID: .+//;
$ifid =~ s/\n//;
chomp($ifid);

# find ifiction
if (-e "$fromdir$ifid.iFiction")
{
 $ifiction="$fromdir$ifid.iFiction";
}
elsif (-e "$fromdir"."babel.iFiction")
{
 $str="$BABELGET -ifiction $ifid -ifiction $fromdir"."babel.iFiction > temp.iFiction";

 system($str);
 $ifiction="temp.iFiction";
}
else
{
 print "No iFiction record found.\n";
 next;
}
$cifid=`$IFIX $ifiction identification ifid`;
#print "$IFIX $ifiction identification ifid\n";
#print "$cifid\n";
chomp($cifid);
# find cover
if (-e "$fromdir$ifid.png")
{
 $cvr="$fromdir$ifid.png";
}
elsif (-e "$fromdir$ifid.jpg")
{
 $cvr="$fromdir$ifid.jpg";
}
elsif (-e "$fromdir$cifid.png")
{
 $cvr="$fromdir$cifid.png";
}
elsif (-e "$fromdir$cifid.jpg")
{
 $cvr="$fromdir$cifid.jpg";
}
else { $cvr=""; }

$blorb_mode=0;
print "$fromdir$cifid.blorb\n";
if (-e "$fromdir$cifid.blorb")
{
 $blorb_mode="$fromdir$cifid.blorb";
}
print "Bundling with $ifiction $cvr\n";
if ($blorb_mode)
{
 system("$BABEL -blorb $file $ifiction");
 $file =~ s/\..+$/\.zblorb/;
 system("$BMERGE $ifid.zblorb $blorb_mode $file");
 unlink("$ifid.zblorb");
}
else
{
 system("$BABEL -blorbs $file $ifiction $cvr");
}
if ($ifiction eq "temp.iFiction") { unlink($ifiction); }


}
