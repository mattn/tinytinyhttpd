#!/bin/bash
# smoke test for tthttpd: static files, directory listing, CGI, keep-alive.
# usage: t/smoke.sh [path-to-tthttpd]
set -u

BIN=${1:-./tthttpd}
PORT=${TTHTTPD_TEST_PORT:-18080}
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

DOCROOT="$WORK/docroot"
mkdir -p "$DOCROOT/subdir"
echo "hello world" > "$DOCROOT/index.html"
echo "plain text file" > "$DOCROOT/subdir/file.txt"
head -c 1000000 /dev/urandom > "$DOCROOT/big.bin"

cat > "$DOCROOT/test.cgi" <<'EOF'
#!/usr/bin/perl
read(STDIN, my $body, $ENV{CONTENT_LENGTH} || 0);
print "Content-Type: text/plain\r\n\r\n";
print "method=$ENV{REQUEST_METHOD} qs=$ENV{QUERY_STRING} len=" . length($body) . "\n";
EOF
chmod +x "$DOCROOT/test.cgi"

"$BIN" -p "$PORT" -d "$DOCROOT" &
PID=$!
trap 'kill -9 $PID 2>/dev/null; rm -rf "$WORK"' EXIT

for i in $(seq 1 50); do
  curl -s -o /dev/null "http://127.0.0.1:$PORT/index.html" && break
  sleep 0.1
done

fail=0
check() { # name expected actual
  if [ "$2" != "$3" ]; then
    echo "FAIL: $1: expected [$2] got [$3]"
    fail=1
  else
    echo "ok: $1"
  fi
}

check "static file" "hello world" "$(curl -s http://127.0.0.1:$PORT/index.html)"
check "default page" "hello world" "$(curl -s http://127.0.0.1:$PORT/)"
check "subdir file" "plain text file" "$(curl -s http://127.0.0.1:$PORT/subdir/file.txt)"
check "404" "404" "$(curl -s -o /dev/null -w '%{http_code}' http://127.0.0.1:$PORT/nope.txt)"
check "content type" "200 text/html;" "$(curl -s -o /dev/null -w '%{http_code} %{content_type}' http://127.0.0.1:$PORT/index.html)"
check "dir listing" "2" "$(curl -s http://127.0.0.1:$PORT/subdir/ | grep -o 'file.txt' | wc -l | tr -d ' ')"
check "head" "200" "$(curl -s -I -o /dev/null -w '%{http_code}' http://127.0.0.1:$PORT/index.html)"
check "keepalive" "hello world hello world" "$(curl -s http://127.0.0.1:$PORT/index.html http://127.0.0.1:$PORT/index.html | tr '\n' ' ' | sed 's/ $//')"

a=$(md5sum < "$DOCROOT/big.bin" | cut -d' ' -f1)
b=$(curl -s http://127.0.0.1:$PORT/big.bin | md5sum | cut -d' ' -f1)
check "1MB file" "$a" "$b"

if [ -x /usr/bin/perl ]; then
  check "cgi get" "method=GET qs=a=1 len=0" "$(curl -s "http://127.0.0.1:$PORT/test.cgi?a=1")"
  for i in $(seq 1 10); do
    check "cgi post ($i)" "method=POST qs= len=4" "$(curl -s -d 'x=42' http://127.0.0.1:$PORT/test.cgi)"
  done
  head -c 300000 /dev/zero | tr '\0' 'a' > "$WORK/bigbody"
  check "cgi big post" "method=POST qs= len=300000" "$(curl -s --data-binary @"$WORK/bigbody" http://127.0.0.1:$PORT/test.cgi)"
else
  echo "skip: cgi tests (no /usr/bin/perl)"
fi

kill -TERM $PID
for i in $(seq 1 50); do
  kill -0 $PID 2>/dev/null || break
  sleep 0.1
done
if kill -0 $PID 2>/dev/null; then
  echo "FAIL: server did not exit on SIGTERM"
  fail=1
else
  echo "ok: clean shutdown"
fi

exit $fail
