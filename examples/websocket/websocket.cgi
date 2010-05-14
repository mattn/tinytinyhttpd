#!perl

binmode STDOUT, ':raw';
$| = 1;
my $hs = join "\015\012",
    "HTTP/1.1 101 Web Socket Protocol Handshake",
    "Upgrade: WebSocket",
    "Connection: Upgrade",
    "WebSocket-Origin: null",
    "WebSocket-Location: ws://127.0.0.1:8080/websocket.cgi",
    '', '';

print $hs;
while (<>) {
	print "\x00$_\xff";
}
