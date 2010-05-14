#!perl

binmode STDOUT, ':raw';
$| = 1;
my $hs = join "\015\012",
    "HTTP/1.1 101 Web Socket Protocol Handshake",
    "Upgrade: $ENV{HTTP_UPGRADE}",
    "Connection: $ENV{HTTP_CONNECTION}",
    "WebSocket-Origin: $ENV{HTTP_ORIGIN}",
    "WebSocket-Location: ws://$ENV{HTTP_HOST}$ENV{PATH_INFO}",
    '', '';

print $hs;

while (<>) {
	print "\x00$_\xff";
}

