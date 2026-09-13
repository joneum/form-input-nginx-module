#!/bin/sh
#
# set_form_input_multi produces a value that only array-var-nginx-module
# can read, so nginx refuses to start when the directive appears in a
# build without it.  The test suite cannot reach that case, it always
# builds array-var in, which is why this check exists on its own.
#
# usage: ci/check-no-array-var.sh <install prefix of a no-array-var build>

set -eu

PREFIX=${1:?install prefix missing}
NGINX=$PREFIX/sbin/nginx
T=${CI_WORK:-.}/no-array-var

rm -rf "$T"
mkdir -p "$T/conf" "$T/logs"

# nginx resolves a relative path in the configuration against the prefix
# it was given with -p, so a relative one here would be applied twice

T=$(cd "$T" && pwd)

write_conf() {
	cat > "$T/conf/nginx.conf" <<EOF
worker_processes 1;
error_log $T/logs/error.log;
pid $T/logs/nginx.pid;
events { worker_connections 64; }
http {
    server {
        listen 127.0.0.1:8199;
        location /t {
            $1
            echo "[\$a]";
        }
    }
}
EOF
}

fail=0

echo "--- set_form_input_multi has to be refused ---"
write_conf 'set_form_input_multi $a data;'

if out=$("$NGINX" -p "$T" -c conf/nginx.conf -t 2>&1); then
	echo "$out"
	echo "FAIL: nginx accepted set_form_input_multi without array-var" >&2
	fail=1
else
	echo "$out" | sed 's/^/  /'

	if echo "$out" | grep -q "array-var-nginx-module"; then
		echo "  ok, refused and the message names the missing module"
	else
		echo "FAIL: refused, but not for the expected reason" >&2
		fail=1
	fi
fi

echo "--- set_form_input has to keep working ---"
write_conf 'set_form_input $a data;'

if out=$("$NGINX" -p "$T" -c conf/nginx.conf -t 2>&1); then
	echo "$out" | sed 's/^/  /'
	echo "  ok, accepted"
else
	echo "$out"
	echo "FAIL: set_form_input was refused as well" >&2
	fail=1
fi

exit $fail
