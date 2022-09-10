//
//  woz2dsk.c
//  scott
//
//  Created by Administrator on 2022-09-09.
//
#include "scott.h"
#include "woz2dsk.h"

#define debug_print(fmt, ...) \
do { if (DEBUG_ACTIONS) fprintf(stderr, fmt, ##__VA_ARGS__); } while (0)

#define STANDARD_TRACKS_PER_DISK 35
#define SECTORS_PER_TRACK 16
#define BYTES_PER_SECTOR 256
#define NIBBLES_PER_TRACK 6656

typedef enum {
    DOS,
    ProDOS
} SectorOrderType;

static const uint8_t decode_6x2[] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // <= 0x80
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x01, // <= 0x90
    0xff, 0xff, 0x02, 0x03, 0xff, 0x04, 0x05, 0x06,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x07, 0x08, // <= 0xa0
    0xff, 0xff, 0xff, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,
    0xff, 0xff, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, // <= 0xb0
    0xff, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // <= 0xc0
    0xff, 0xff, 0xff, 0x1b, 0xff, 0x1c, 0x1d, 0x1e,
    0xff, 0xff, 0xff, 0x1f, 0xff, 0xff, 0x20, 0x21, // <= 0xd0
    0xff, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0xff, 0xff, 0xff, 0xff, 0xff, 0x29, 0x2a, 0x2b, // <= 0xe0
    0xff, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32,
    0xff, 0xff, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, // <= 0xf0
    0xff, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f };

const char address_prologue[] = "\xD5\xAA\x96";
const char data_prologue[] = "\xD5\xAA\xAD";

// see: http://www.applelogic.org/TheAppleIIEGettingStarted.html
#define SECTORS_PHY2LOG_DOS    "\x0\x7\xE\x6\xD\x5\xC\x4\xB\x3\xA\x2\x9\x1\x8\xF"
#define SECTORS_PHY2LOG_ProDOS "\x0\x8\x1\x9\x2\xA\x3\xB\x4\xC\x5\xD\x6\xE\x7\xF"

#define WOZ_BLOCK_SIZE 512


//                                                          my ($opt, $usage) = describe_options
//                                                          (
//                                                           q{%c %o <some-arg>
//
//                                                               Options:},
//                                                           [ 'format|f=s', "output format {auto|DOS|ProDOS|nibble}",
//                                                             { default  => 'auto' } ],
//                                                           [ 'output-40-tracks',
//                                                             "output 40 tracks (instead of the standard of 35)" ],
//                                                           [],
//                                                           [ 'quiet|q',    "suppress output messages" ],
//                                                           [ 'debug|d',    "show debug messages" ],
//                                                           [ 'help|h',     "print usage message and exit", { shortcircuit => 1 } ],
//                                                           );
//
//
//                                                          die $usage->text
//                                                          if $opt->help || @ARGV != 2;
//
//                                                          my ($in_file, $out_file) = @ARGV;
//
//                                                          die "$in_file: cannot read\n"
//                                                          unless -r $in_file;
//
//                                                          die "$out_file: cannot write\n"
//                                                          unless !-e $out_file || -w $out_file;
//
//
//                                                          sub debug {
//                                                              return unless $opt->debug;
//                                                              print STDERR @_
//                                                          }

//                                                          my $format = $opt->format;
//                                                          if ($format eq 'auto') {
//                                                              my ($ext) = $out_file =~ m{\.([^.]*)$};
//
//                                                              if (defined($ext)) {
//                                                                  if ($ext =~ /^nib/i) {
//                                                                      $format = 'nibble';
//                                                                  } else {
//                                                                      $format = ($ext =~ /^p/i)? 'ProDOS' : 'DOS';
//                                                                  }
//                                                              } else {

                                                                  SectorOrderType format = DOS;

//                                                              }
//
//                                                              my $description = $format eq 'nibble'? "nibble format" :
//                                                              "$format-ordered logical sectors";
//
//                                                              print "using $description for output\n"
//                                                              unless $opt->quiet;
//                                                          }
//
//                                                          my $sectors_phy2log = SECTORS_PHY2LOG->{$format};
//                                                          Fatal("unknown format: $format\n"
//                                                          unless defined $sectors_phy2log || $format eq 'nibble';
//
//                                                          my $nr_tracks = $opt->output_40_tracks? 40 : STANDARD_TRACKS_PER_DISK;
//
//
//                                                          my $woz_buffer = slurp($in_file);
//                                                          my $woz_size = length($woz_buffer);
//                                                          my $woz_unpack_pos = 0;
//
//                                                          my ($magic, $fixed, $crc32) =
//                                                          unpack("a4 a4 L<", substr($woz_buffer, $woz_unpack_pos));
//                                                          $woz_unpack_pos += 12;
//
//                                                                Fatal("$in_file: WOZ2 header not found\n");
//                                                          unless $magic =~  m{^WOZ[12]$};
//                                                                Fatal("$in_file: bad header\n"):
//                                                          unless $fixed eq "\xFF\x0A\x0D\x0A";
//
//                                                          if ($crc32 != 0) {
//                                                              my $computed_crc32 = crc32(substr($woz_buffer, $woz_unpack_pos));
//                                                              Fatal(sprintf("$in_file: CRC mismatch (stored=%08x; computed=%08x)\n",
//                                                                          $crc32, $computed_crc32)
//                                                              unless $crc32 == $computed_crc32;
//                                                          }
//
//                                                          my $info;
//                                                          my @tmap;
//                                                          my @trks;
//                                                          while ($woz_unpack_pos < $woz_size) {
//                                                              my ($chunk_id, $chunk_size) =
//                                                              unpack("a4 L<", substr($woz_buffer, $woz_unpack_pos));
//                                                              my $chunk_data = substr($woz_buffer, 8+$woz_unpack_pos, $chunk_size);
//                                                              $woz_unpack_pos += 8+$chunk_size;
//
//                                                              for ($chunk_id) {
//                                                                  when ('INFO') {
//                                                                      Fatal("$in_file: INFO chunk size is not 60 (but $chunk_size) bytes\n"
//                                                                      unless $chunk_size == 60;
//                                                                      $info = 1;
//
//                                                                      my ($version, $disk_type, $_wp, $_sync, $_cleaned, $creator, $d2) =
//                                                                      unpack("CCCCC a32 a*", $chunk_data);
//                                                                      my $disk_type_string =
//                                                                      $disk_type == 1? "5.25\"" :
//                                                                      $disk_type == 2? "3.5\"" :
//                                                                      "unknown";
//                                                                      debug_print "INFO: ver $version, type $disk_type_string, creator: $creator\n";
//                                                                      print "WOZ file version $version\n"
//                                                                      unless $opt->quiet;
//
//                                                                      Fatal("$in_file: only 5.25\" disks are supported\n"
//                                                                      unless $disk_type == 1;
//
//                                                                      if ($version >= 2) {
//                                                                          my ($disk_sides, $_bsf, $_obt, $_ch, $_rR, $_lt) =
//                                                                          unpack("CCC S< S< S<", $d2);
//                                                                          debug_print "    sides: $disk_sides\n";
//
//                                                                          Fatal("$in_file: only 1-sided images are supported\n"
//                                                                          unless $disk_sides == 1;
//                                                                      }
//                                                                  }
//
//                                                                  when ('TMAP') {
//                                                                      Fatal("$in_file: TMP chunk size is not 160 (but $chunk_size) bytes\n"
//                                                                      unless $chunk_size == 160;
//
//// we are only interested in integer-numbered tracks...
//                                                                      @tmap = unpack("C160", $chunk_data);
//                                                                      @tmap = @tmap[map { 4*$_ } 0..($nr_tracks-1)];
//
//                                                                      debug_print "TMAP: ", join(',', @tmap), "\n";
//                                                                  }
//
//                                                                  when ('TRKS') {
//                                                                      debug_print "TRKS: $chunk_size bytes\n";
//                                                                      if ($magic eq 'WOZ2') {
//                                                                          @trks = unpack("(a8)160", $chunk_data);
//                                                                          map { $_ = [ unpack("S< S< L<", $_) ] } @trks;
//
//                                                                          debug_print "  trks: ", join('/', map {
//                                                                              join(",", @$_)
//                                                                          } @trks), "\n";
//                                                                      } else {
//                                                                          Fatal("bug!" unless $magic eq 'WOZ1';
//                                                                          while (length($chunk_data) >= 6656) {
//                                                                              my $bitstream = substr($chunk_data, 0, 6646);
//                                                                              my ($bytes_used, $bit_count) =
//                                                                              unpack("S< S<", substr($chunk_data, 6646, 4));
//                                                                              push @trks, [ $bytes_used, $bit_count, $bitstream];
//                                                                              $chunk_data = substr($chunk_data, 6656);
//                                                                          }
//
//                                                                          debug_print "  trks: ", join('/', map {
//                                                                              join(",", $_->[0], $_->[1])
//                                                                          } @trks), "\n";
//                                                                      }
//                                                                  }
//
//                                                                  when ('WRIT') {
//                                                                      debug_print "WRIT: $chunk_size bytes\n";
//// ignore WRIT chunks
//                                                                  }
//
//                                                                  when ('META') {
//                                                                      debug_print "META: $chunk_size bytes\n";
//                                                                      my @entries = split("\n", $chunk_data);
//                                                                      for my $e (@entries) {
//                                                                          my ($key, @value) = split("\t", $e);
//                                                                          my $value = join("\t", @value);
//                                                                          print "  $key => $value\n"
//                                                                          unless $opt->quiet;
//                                                                      }
//                                                                  }
//
//                                                              default {
//                                                                  warn "ignoring unknown chunk: id=$chunk_id; size=$chunk_size bytes\n";
//                                                              }
//                                                              }
//                                                          }
//
//                                                          Fatal("$in_file: INFO chunk not found\n");
//                                                          unless $info;
//                                                          Fatal("$in_file: TMAP chunk not found\n"
//                                                          unless @tmap;
//                                                          Fatal("$in_file: TRKS chunk not found\n"
//                                                          unless @trks;
//
//
//                                                          my $out_fh = IO::File->new($out_file, "w") || Fatal("$out_file: $!\n";
//                                                          my $sector_count = 0;
//                                                          my $out_sector_count = 0;
//
//                                                          {
//                                                              my $out_file_size = $nr_tracks *
//                                                              ($format eq 'nibble'? NIBBLES_PER_TRACK :
//                                                               (SECTORS_PER_TRACK * BYTES_PER_SECTOR));
//                                                              $out_fh->truncate($out_file_size)
//                                                              || Fatal("$out_file: $!\n";
//                                                          }
//
//
//                                                          my $nr_empty_nonstandard_tracks = 0;
//                                                          track: for my $track (0..($nr_tracks-1)) {
//                                                              my $t_hex = sprintf('$%02x', $track);
//
//                                                              my $trks_index = $tmap[$track];
//                                                              if ($trks_index == 0xFF) { // empty track
//                                                                  debug_print "T[$t_hex]: empty track\n";
//                                                                  if ($format eq 'nibble') {
//                                                                      $out_fh->seek(NIBBLES_PER_TRACK, SEEK_CUR);
//                                                                  }
//                                                                  ++$nr_empty_nonstandard_tracks if $track > STANDARD_TRACKS_PER_DISK;
//                                                                  next track;
//                                                              }
//
//                                                              my $trk = $trks[$trks_index];
//                                                              Fatal("T[$t_hex]: bad TMAP value\n"
//                                                              unless defined($trk);
//                                                              my $bit_count;
//                                                              if ($magic eq 'WOZ2') {
//                                                                  my ($start_block, $block_count, $bc) = @$trk;
//                                                                  $bit_count = $bc;
//
//                                                                  my $start = $start_block * WOZ_BLOCK_SIZE;
//                                                                  Fatal("$in_file: start block offset ($start) of track $t_hex is beyond file size ($woz_size)\n"
//                                                                  unless $start < $woz_size;
//                                                                  my $length = $block_count * WOZ_BLOCK_SIZE;
//                                                                  Fatal("$in_file: length ($length) of track $t_hex is extends beyond end of file\n"
//                                                                  unless $start + $length <= $woz_size;
//
//                                                                  $trk = substr($woz_buffer, $start, $length);
//                                                              } else {
//                                                                  Fatal("bug!" unless $magic eq 'WOZ1';
//                                                                  my ($bytes_used, $bc, $bitstream) = @$trk;
//                                                                  $bit_count = $bc;
//                                                                  $trk = $bitstream;
//                                                              }
//
//                                                              $trk = unpack("B$bit_count", $trk);
//                                                              Fatal("$in_file: track $t_hex has fewer bits of data (" . length($trk) . ") than specified ($bit_count)\n"
//                                                              unless length($trk) >= $bit_count;
//
//                                                              { // synchronize
//                                                                  use constant sync_bytes =>
//                                                                  {
//                                                                      16 => "1111111100" x 5, // for 16-sector tracks
//                                                                          13 => "111111110" x 8, // for 13-sector tracks
//                                                                              };
//
//                                                                  my $nr_sectors;
//                                                              sync: for my $ns (16, 13) {
//                                                                  my $sync = sync_bytes->{$ns};
//                                                                  my ($a, $b) = $trk =~ m{^(.*?$sync)(.*)$}; // looking for sync bytes...
//                                                                      if (defined($a) && defined($b)) {
//                                                                          $trk = $b . $a; // rotate
//                                                                          $nr_sectors = $ns;
//                                                                          last sync;
//                                                                      }
//
//// has been split when linearizing the circular track?
//                                                                  my $s = length($sync);
//                                                                  debug_print "rotate and retry by $s bits\n";
//                                                                  $trk = substr($trk, length($trk)-$s) . substr($trk, 0, length($trk)-$s);
//                                                                  my ($a2, $b2) = $trk =~ m{^(.*?$sync)(.*)$}; //try once more...
//                                                                  if (defined($a2) && defined($b2)) {
//                                                                      $trk = $b2 . $a2; // rotate
//                                                                      $nr_sectors = $ns;
//                                                                      last sync;
//                                                                  }
//                                                              }
//
//                                                                  Fatal("$in_file: track $t_hex has no sync bytes\n"
//                                                                  unless defined $nr_sectors;
//
//                                                                  Fatal("$in_file: uses 13-sector (DOS 3.2) format; please use --format=nibble\n"
//                                                                  if $nr_sectors == 13 && $format ne 'nibble';
//                                                              }
//
//// convert bits to nibbles
//                                                              my $nibbles = '';
//                                                              for (;;) {
//                                                                  last unless $trk =~ s{^0*(1.{7})}{}o;
//                                                                  $nibbles .= $1;
//                                                              }
//                                                              $nibbles = pack("B*", $nibbles);
//
//
//                                                              if ($format eq 'nibble') {
//                                                                  debug_print "T[$t_hex]: ". length($nibbles) ." nibbles ";
//                                                                  if (length($nibbles) < NIBBLES_PER_TRACK) {
//                                                                      $nibbles .= "\0" x (NIBBLES_PER_TRACK - length($nibbles));
//                                                                      debug_print "padded to ". length($nibbles) ." nibbles\n";
//                                                                  } elsif (length($nibbles) > NIBBLES_PER_TRACK) {
//                                                                      $nibbles = substr($nibbles, 0, NIBBLES_PER_TRACK);
//                                                                      debug_print "truncated to ". length($nibbles) ." nibbles\n";
//                                                                  } else {
//                                                                      debug_print "(just made!)\n";
//                                                                  }
//
//                                                                  $out_fh->write($nibbles, NIBBLES_PER_TRACK);
//                                                                  next track;
//                                                              }
//
//// DOS or ProDOS format...
//
//                                                              sub decode4x4 {
//                                                                  my $a = (shift() << 1) | 0x1;
//                                                                  my $b = shift();
//                                                                  $a & $b
//                                                              }
//
//                                                              my $track_sector_count = 0;
//                                                          sector: for (;;) {
//                                                              last unless $nibbles =~ s/^.*?$address_prologue(........)//;
//                                                              my @addr_nibs = unpack("CCCCCCCC", $1);
//                                                              my $volume = decode4x4(@addr_nibs[0..1]);
//                                                              my $a_track = decode4x4(@addr_nibs[2..3]);
//                                                              my $sector = decode4x4(@addr_nibs[4..5]);
//                                                              my $checksum = decode4x4(@addr_nibs[6..7]);
//
//                                                              my $calc_sum = $volume ^ $track ^ $sector;
//                                                              my $s_hex = sprintf('$%x', $sector);
//
//                                                              unless ($checksum == $calc_sum) {
//                                                                  warn sprintf("T[$t_hex]S[$s_hex]: address checksum mismatch (stored=%02x; computed=%02x)\n",
//                                                                               $checksum, $calc_sum);
//                                                                  next sector;
//                                                              }
//                                                              unless ($a_track == $track) {
//                                                                  warn sprintf("T[$t_hex]S[$s_hex]: track mismatch (stored=%02x)\n",
//                                                                               $a_track);
//                                                                  next sector;
//                                                              }
//
//                                                              debug_print "T[$t_hex]: $volume $track $sector\n";
//
//                                                              unless ($nibbles =~ s/^...(.*?)$data_prologue//) {
//                                                                      warn sprintf("T[$t_hex]S[$s_hex]: missing data\n");
//                                                                      next sector;
//                                                                      }
//                                                                      if (length($1) > 200) { // too far away!
//                                                                  warn sprintf("T[$t_hex]S[$s_hex]: missing nearby data\n");
//                                                                  next sector;
//                                                              }
//
//                                                                      my @sector_nibbles = unpack("C343", substr($nibbles, 0, 343));
//                                                                      $nibbles = substr($nibbles, 343);
//
//                                                                      map { $_ = decode_6x2->[$_ & 0x7F] } @sector_nibbles;
//                                                                      for my $i (1..342) {
//                                                                  $sector_nibbles[$i] ^= $sector_nibbles[$i - 1];
//                                                              }
//                                                                      my $data_checksum = pop @sector_nibbles;
//                                                                      unless ($data_checksum == 0) {
//                                                                  warn sprintf("T[$t_hex]S[$s_hex]: bad data checksum\n");
//                                                                  next sector;
//                                                              }
//
//                                                                      my @bytes;
//                                                                      for my $i (0..255) {
//                                                                  use constant bit_flip => [0, 2, 1, 3];
//                                                                  my $h6 = $sector_nibbles[86+$i];
//                                                                  my $l2 = bit_flip->[($sector_nibbles[$i%86] >> (int($i/86) * 2)) & 0x3];
//                                                                  push @bytes, $h6<<2 | $l2;
//                                                              }
//
//
//                                                                      my $logical_sector = $sectors_phy2log->[$sector];
//                                                                      my $all_zeros = 1;
//                                                                      map { $all_zeros = 0 if $_ != 0 } @bytes;
//
//                                                                      ++$sector_count;
//                                                                      ++$track_sector_count;
//
//                                                                      debug_print sprintf('==>S[$%x]:', $logical_sector);
//                                                                      if ($all_zeros) {
//                                                                  debug_print " empty\n";
//                                                                  next sector;
//                                                              }
//                                                                      debug_print "\n";
//
//                                                                      my $bytes = pack('C*', @bytes);
//
//                                                                      $out_fh->seek(($track * SECTORS_PER_TRACK + $logical_sector)
//                                                                                    * BYTES_PER_SECTOR,
//                                                                                    SEEK_SET)
//                                                                      || Fatal("$out_file: $!\n");
//                                                                      $out_fh->write($bytes, BYTES_PER_SECTOR);
//                                                                      ++$out_sector_count;
//                                                                      }
//
//                                                                      warn "T[$t_hex]: no sectors found\n"
//                                                                      unless $track_sector_count;
//                                                                      }
//
//                                                                      $out_fh->close || Fatal("$out_file: $!\n";
//
//                                                                      unless ($format eq 'nibble') {
//                                                                  warn "$in_file: no sectors found!\n"
//                                                                  unless $sector_count;
//
//                                                                  warn "$in_file: tracks > \$22 are empty; maybe, you didn't need --output_40_tracks?\n"
//                                                                  unless STANDARD_TRACKS_PER_DISK + $nr_empty_nonstandard_tracks
//                                                                  == $nr_tracks;
//
//                                                                  print "written $out_sector_count/$sector_count sectors\n"
//                                                                  unless $opt->quiet;
//                                                              }
