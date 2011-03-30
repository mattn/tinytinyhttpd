#!perl

use strict;
use warnings;
use Digest::MD5 qw(md5);

binmode STDIN, ':raw';
binmode STDOUT, ':raw';
$| = 1;

sub decode_key {
    my $key = shift;
    return unless $key;
    my ($n, $s) = ($key, $key);
    $n =~ s/[^0-9]//g;
    $s =~ s/[^ ]//g;
    return scalar($n) / length($s);
}

sub encode_str {
    my ($key1, $key2, $str) = @_;
    return md5(pack('N' => $key1).pack('N' => $key2).$str);
}

my $key1 = decode_key $ENV{HTTP_SEC_WEBSOCKET_KEY1};
my $key2 = decode_key $ENV{HTTP_SEC_WEBSOCKET_KEY2};
if ($key1 && $key2) {
    print join("\015\012",
        "HTTP/1.1 101 Web Socket Protocol Handshake",
        "Upgrade: $ENV{HTTP_UPGRADE}",
        "Connection: $ENV{HTTP_CONNECTION}",
        "Sec-WebSocket-Origin: $ENV{HTTP_ORIGIN}",
        "Sec-WebSocket-Location: ws://$ENV{HTTP_HOST}$ENV{PATH_INFO}",
        "", "");
    my $key3;
    read STDIN, $key3, 8;
    print encode_str($key1, $key2, $key3);
} else {
    print join("\015\012",
        "HTTP/1.1 101 Web Socket Protocol Handshake",
        "Upgrade: $ENV{HTTP_UPGRADE}",
        "Connection: $ENV{HTTP_CONNECTION}",
        "WebSocket-Origin: $ENV{HTTP_ORIGIN}",
        "WebSocket-Location: ws://$ENV{HTTP_HOST}$ENV{PATH_INFO}",
        "", "");
}

while (<>) {
    print "\x00$_\xff";
}
