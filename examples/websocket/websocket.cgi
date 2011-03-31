#!perl

use strict;
use warnings;
use File::Basename;
use Filesys::Notify::Simple;
use JSON;
use Time::Piece;
use Digest::MD5 qw(md5);

my $file = dirname(__FILE__).'/websocket.log';

unless (-e $file) {
  open my $fh, '>', $file;
  close $fh;
}

binmode STDIN, ':raw';
binmode STDOUT, ':raw';
$| = 1;

sub decode_key {
  my $key = shift;
  return unless $key;
  my ($n, $s) = ($key, $key);
  $n =~ s/[^0-9]//g;
  $s =~ s/[^ ]//g;
  return int(scalar $n / length($s));
}

sub encode_str {
  my ($key1, $key2, $str) = @_;
  my $r = substr md5(pack('N', $key1).pack('N', $key2).$str), 0, 16, '';
  return $r;
}

my $key1 = decode_key $ENV{HTTP_SEC_WEBSOCKET_KEY1};
my $key2 = decode_key $ENV{HTTP_SEC_WEBSOCKET_KEY2};
if ($key1 && $key2) {
  print join("\015\012",
    "HTTP/1.1 101 WebSocket Protocol Handshake",
    "Upgrade: $ENV{HTTP_UPGRADE}",
    "Connection: $ENV{HTTP_CONNECTION}",
    "Sec-WebSocket-Origin: $ENV{HTTP_ORIGIN}",
    "Sec-WebSocket-Location: ws://$ENV{HTTP_HOST}$ENV{PATH_INFO}",
    "", "");
  read STDIN, my $key3, 8;
  print encode_str($key1, $key2, $key3);
} else {
  print join("\015\012",
    "HTTP/1.1 101 WebSocket Protocol Handshake",
    "Upgrade: $ENV{HTTP_UPGRADE}",
    "Connection: $ENV{HTTP_CONNECTION}",
    "WebSocket-Origin: $ENV{HTTP_ORIGIN}",
    "WebSocket-Location: ws://$ENV{HTTP_HOST}$ENV{PATH_INFO}",
    "", "");
}

my $pid = fork();
die "fork() failed: $!" unless defined $pid;
if ($pid) {
  # tail log
  my $watcher = Filesys::Notify::Simple->new([ $file ]);
  while (1) {
    open my $fh, '<', $file;
    seek $fh, 0, 2;
    my $pos = tell($fh);
    close $fh;
    $watcher->wait(sub {
      open my $fh, '<', $file;
      seek $fh, $pos, 0;
      while (<$fh>) {
        print "\x00$_\xff";
      }
      $pos = tell($fh);
      close $fh;
    });
  }
} else {
  local $/ = "\xff";
  while (<STDIN>) {
    open my $fh, '>>', $file;
    binmode $fh;
    my $json = to_json { message => substr($_, 1, -1), time => Time::Piece::localtime()->datetime };
    print $fh "$json\n";
    close $fh;
  }
  kill 'KILL' => $pid;
}
