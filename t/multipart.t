# vi:set ft= ts=4 sw=4 et fdm=marker:

use lib 'lib';
use Test::Nginx::Socket;

plan tests => repeat_each() * 3 * blocks();

no_long_string();

run_tests();

__DATA__

=== TEST 1: without the switch a multipart body is left alone
--- config
    location /t {
        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=X
--- request eval
"POST /t\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"data\"\r\n"
. "\r\n"
. "value\r\n"
. "--X--\r\n"
--- response_body
[]
--- no_error_log
[error]



=== TEST 2: one field
--- config
    location /t {
        form_input_multipart on;

        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=X
--- request eval
"POST /t\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"data\"\r\n"
. "\r\n"
. "value\r\n"
. "--X--\r\n"
--- response_body
[value]
--- no_error_log
[error]



=== TEST 3: the field is the second of three
--- config
    location /t {
        form_input_multipart on;

        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=X
--- request eval
"POST /t\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"first\"\r\n"
. "\r\n"
. "one\r\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"data\"\r\n"
. "\r\n"
. "value\r\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"last\"\r\n"
. "\r\n"
. "three\r\n"
. "--X--\r\n"
--- response_body
[value]
--- no_error_log
[error]



=== TEST 4: the field is not there
--- config
    location /t {
        form_input_multipart on;

        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=X
--- request eval
"POST /t\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"other\"\r\n"
. "\r\n"
. "value\r\n"
. "--X--\r\n"
--- response_body
[]
--- no_error_log
[error]



=== TEST 5: an empty value
--- config
    location /t {
        form_input_multipart on;

        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=X
--- request eval
"POST /t\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"data\"\r\n"
. "\r\n"
. "\r\n"
. "--X--\r\n"
--- response_body
[]
--- no_error_log
[error]



=== TEST 6: a file part of the same name is skipped
--- config
    location /t {
        form_input_multipart on;

        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=X
--- request eval
"POST /t\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"data\"; filename=\"a.txt\"\r\n"
. "Content-Type: text/plain\r\n"
. "\r\n"
. "file content\r\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"data\"\r\n"
. "\r\n"
. "field value\r\n"
. "--X--\r\n"
--- response_body
[field value]
--- no_error_log
[error]



=== TEST 20: a file part named the RFC 5987 way is skipped as well
--- config
    location /t {
        form_input_multipart on;

        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=X
--- request eval
"POST /t\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"data\"; filename*=UTF-8''a.txt\r\n"
. "Content-Type: text/plain\r\n"
. "\r\n"
. "file content\r\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"data\"\r\n"
. "\r\n"
. "field value\r\n"
. "--X--\r\n"
--- response_body
[field value]
--- no_error_log
[error]



=== TEST 7: values are literal, not percent encoded
--- config
    location /t {
        form_input_multipart on;

        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=X
--- request eval
"POST /t\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"data\"\r\n"
. "\r\n"
. "a+b%20c&d=e\r\n"
. "--X--\r\n"
--- response_body
[a+b%20c&d=e]
--- no_error_log
[error]



=== TEST 8: a quoted boundary, and parameters in front of it
--- config
    location /t {
        form_input_multipart on;

        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: multipart/form-data; charset=utf-8; boundary="a b"
--- request eval
"POST /t\n"
. "--a b\r\n"
. "Content-Disposition: form-data; name=\"data\"\r\n"
. "\r\n"
. "value\r\n"
. "--a b--\r\n"
--- response_body
[value]
--- no_error_log
[error]



=== TEST 9: no boundary parameter at all
--- config
    location /t {
        form_input_multipart on;

        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: multipart/form-data
--- request eval
"POST /t\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"data\"\r\n"
. "\r\n"
. "value\r\n"
. "--X--\r\n"
--- response_body
[]
--- no_error_log
[error]



=== TEST 10: every occurrence through set_form_input_multi
--- config
    location /t {
        form_input_multipart on;

        set_form_input_multi $tags tag;
        array_join ',' $tags;
        echo "[$tags]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=X
--- request eval
"POST /t\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"tag\"\r\n"
. "\r\n"
. "a\r\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"other\"\r\n"
. "\r\n"
. "skip\r\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"tag\"\r\n"
. "\r\n"
. "b\r\n"
. "--X--\r\n"
--- response_body
[a,b]
--- no_error_log
[error]



=== TEST 11: urlencoded still works with the switch on
--- config
    location /t {
        form_input_multipart on;

        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: application/x-www-form-urlencoded
--- request
POST /t
data=value&x=1
--- response_body
[value]
--- no_error_log
[error]



=== TEST 12: the switch can be set for the whole server
--- config
    form_input_multipart on;

    location /t {
        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=X
--- request eval
"POST /t\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"data\"\r\n"
. "\r\n"
. "value\r\n"
. "--X--\r\n"
--- response_body
[value]
--- no_error_log
[error]



=== TEST 14: the body stops in the middle of a value
--- config
    location /t {
        form_input_multipart on;

        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=X
--- request eval
"POST /t\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"data\"\r\n"
. "\r\n"
. "value"
--- response_body
[]
--- no_error_log
[error]



=== TEST 15: the headers of a part are never closed
--- config
    location /t {
        form_input_multipart on;

        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=X
--- request eval
"POST /t\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"data\""
--- response_body
[]
--- no_error_log
[error]



=== TEST 16: the body carries no delimiter at all
--- config
    location /t {
        form_input_multipart on;

        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=X
--- request
POST /t
data=value
--- response_body
[]
--- no_error_log
[error]



=== TEST 17: nothing but the closing delimiter
--- config
    location /t {
        form_input_multipart on;

        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=X
--- request eval
"POST /t\n"
. "--X--\r\n"
--- response_body
[]
--- no_error_log
[error]



=== TEST 18: the body ends right behind an opening delimiter
--- config
    location /t {
        form_input_multipart on;

        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=X
--- request eval
"POST /t\n"
. "--X"
--- response_body
[]
--- no_error_log
[error]



=== TEST 19: a value that carries line breaks of its own
--- config
    location /t {
        form_input_multipart on;

        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=X
--- request eval
"POST /t\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"data\"\r\n"
. "\r\n"
. "one\r\ntwo\r\n"
. "--X--\r\n"
--- response_body eval
"[one\r\ntwo]\n"
--- no_error_log
[error]



=== TEST 21: a delimiter in the middle of a line opens nothing
--- config
    location /t {
        form_input_multipart on;

        set_form_input $user;
        echo "[$user]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=B
--- request eval
"POST /t\n"
. "X--B\r\n"
. "Content-Disposition: form-data; name=\"user\"\r\n"
. "\r\n"
. "admin\r\n"
. "--B\r\n"
. "Content-Disposition: form-data; name=\"user\"\r\n"
. "\r\n"
. "guest\r\n"
. "--B--\r\n"
--- response_body
[guest]
--- no_error_log
[error]



=== TEST 22: the boundary inside a value ends nothing
--- config
    location /t {
        form_input_multipart on;

        set_form_input $second;
        echo "[$second]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=X
--- request eval
"POST /t\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"first\"\r\n"
. "\r\n"
. "val--Xmore\r\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"second\"\r\n"
. "\r\n"
. "found\r\n"
. "--X--\r\n"
--- response_body
[found]
--- no_error_log
[error]



=== TEST 23: a filename on a folded continuation line
--- config
    location /t {
        form_input_multipart on;

        set_form_input $a;
        echo "[$a]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=X
--- request eval
"POST /t\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"a\";\r\n"
. " filename=\"x.txt\"\r\n"
. "\r\n"
. "SECRETFILEDATA\r\n"
. "--X--\r\n"
--- response_body
[]
--- no_error_log
[error]



=== TEST 24: two Content-Disposition lines in one part
--- config
    location /t {
        form_input_multipart on;

        set_form_input $a;
        echo "[$a]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=X
--- request eval
"POST /t\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"a\"\r\n"
. "Content-Disposition: form-data; name=\"a\"; filename=\"x.txt\"\r\n"
. "\r\n"
. "SECRETFILEDATA\r\n"
. "--X--\r\n"
--- response_body
[]
--- no_error_log
[error]



=== TEST 25: a semicolon between quotes separates no parameter
--- config
    location /t {
        form_input_multipart on;

        set_form_input $a;
        set_form_input $zz;
        echo "a=[$a] zz=[$zz]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=X
--- request eval
"POST /t\n"
. "--X\r\n"
. "Content-Disposition: form-data; q=\"; name=a; z\"; name=\"zz\"\r\n"
. "\r\n"
. "SMUGGLED\r\n"
. "--X--\r\n"
--- response_body
a=[] zz=[SMUGGLED]
--- no_error_log
[error]



=== TEST 26: a part without any headers invents no field
--- config
    location /t {
        form_input_multipart on;

        set_form_input $user;
        echo "[$user]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=b
--- request eval
"POST /t\n"
. "--b\r\n"
. "\r\n"
. "Content-Disposition: form-data; name=\"user\"\r\n"
. "\r\n"
. "admin\r\n"
. "--b--\r\n"
--- response_body
[]
--- no_error_log
[error]



=== TEST 27: a longer boundary that starts like the real one
--- config
    location /t {
        form_input_multipart on;

        set_form_input $user;
        echo "[$user]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=B
--- request eval
"POST /t\n"
. "--B\r\n"
. "Content-Disposition: form-data; name=\"user\"\r\n"
. "\r\n"
. "guest\r\n"
. "--BX\r\n"
. "MEHR\r\n"
. "--B--\r\n"
--- response_body eval
"[guest\r\n--BX\r\nMEHR]\n"
--- no_error_log
[error]



=== TEST 28: a close delimiter with something behind it is not one
--- config
    location /t {
        form_input_multipart on;

        set_form_input $user;
        echo "[$user]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=B
--- request eval
"POST /t\n"
. "--B\r\n"
. "Content-Disposition: form-data; name=\"pad\"\r\n"
. "\r\n"
. "x\r\n"
. "--B--X\r\n"
. "--B\r\n"
. "Content-Disposition: form-data; name=\"user\"\r\n"
. "\r\n"
. "admin\r\n"
. "--B--\r\n"
--- response_body
[admin]
--- no_error_log
[error]



=== TEST 29: a preamble that starts like the boundary
--- config
    location /t {
        form_input_multipart on;

        set_form_input $a;
        echo "[$a]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=b
--- request eval
"POST /t\n"
. "--bXYZ\r\n"
. "\r\n"
. "--b\r\n"
. "Content-Disposition: form-data; name=\"a\"\r\n"
. "\r\n"
. "1\r\n"
. "--b--\r\n"
--- response_body
[1]
--- no_error_log
[error]



=== TEST 30: whitespace in front of the equals sign of a filename
--- config
    location /t {
        form_input_multipart on;

        set_form_input $a;
        echo "[$a]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=X
--- request eval
"POST /t\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"a\"; filename =\"x.txt\"\r\n"
. "\r\n"
. "SECRETFILEDATA\r\n"
. "--X--\r\n"
--- response_body
[]
--- no_error_log
[error]



=== TEST 31: a boundary longer than the 70 characters RFC 2046 allows
--- config
    location /t {
        form_input_multipart on;

        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=aaaaaaaaaabbbbbbbbbbccccccccccddddddddddeeeeeeeeeeffffffffffgggggggggghhhh
--- request eval
"POST /t\n"
. "--aaaaaaaaaabbbbbbbbbbccccccccccddddddddddeeeeeeeeeeffffffffffgggggggggghhhh\r\n"
. "Content-Disposition: form-data; name=\"data\"\r\n"
. "\r\n"
. "value\r\n"
. "--aaaaaaaaaabbbbbbbbbbccccccccccddddddddddeeeeeeeeeeffffffffffgggggggggghhhh--\r\n"
--- response_body
[]
--- no_error_log
[error]



=== TEST 32: a boundary of exactly 70 characters is still read
--- config
    location /t {
        form_input_multipart on;

        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=aaaaaaaaaabbbbbbbbbbccccccccccddddddddddeeeeeeeeeeffffffffffgggggggggg
--- request eval
"POST /t\n"
. "--aaaaaaaaaabbbbbbbbbbccccccccccddddddddddeeeeeeeeeeffffffffffgggggggggg\r\n"
. "Content-Disposition: form-data; name=\"data\"\r\n"
. "\r\n"
. "value\r\n"
. "--aaaaaaaaaabbbbbbbbbbccccccccccddddddddddeeeeeeeeeeffffffffffgggggggggg--\r\n"
--- response_body
[value]
--- no_error_log
[error]



=== TEST 33: multipart field names are matched without regard to case
--- config
    location /t {
        form_input_multipart on;

        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=X
--- request eval
"POST /t\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"DATA\"\r\n"
. "\r\n"
. "uppercase-matches\r\n"
. "--X--\r\n"
--- response_body
[uppercase-matches]
--- no_error_log
[error]



=== TEST 34: a part encoded in base64 is not handed out as text
--- config
    location /t {
        form_input_multipart on;

        set_form_input $user;
        echo "[$user]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=X
--- request eval
"POST /t\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"user\"\r\n"
. "Content-Transfer-Encoding: base64\r\n"
. "\r\n"
. "YWRtaW4=\r\n"
. "--X--\r\n"
--- response_body
[]
--- no_error_log
[error]



=== TEST 35: the encodings that leave the bytes alone are fine
--- config
    location /t {
        form_input_multipart on;

        set_form_input $user;
        echo "[$user]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=X
--- request eval
"POST /t\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"user\"\r\n"
. "Content-Transfer-Encoding: 8bit\r\n"
. "\r\n"
. "admin\r\n"
. "--X--\r\n"
--- response_body
[admin]
--- no_error_log
[error]



=== TEST 36: a part that does not call itself form-data
--- config
    location /t {
        form_input_multipart on;

        set_form_input $user;
        echo "[$user]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=X
--- request eval
"POST /t\n"
. "--X\r\n"
. "Content-Disposition: attachment; name=\"user\"\r\n"
. "\r\n"
. "admin\r\n"
. "--X--\r\n"
--- response_body
[]
--- no_error_log
[error]



=== TEST 13: a part whose name is only a prefix of the one asked for
--- config
    location /t {
        form_input_multipart on;

        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: multipart/form-data; boundary=X
--- request eval
"POST /t\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"datafoo\"\r\n"
. "\r\n"
. "wrong\r\n"
. "--X\r\n"
. "Content-Disposition: form-data; name=\"data\"\r\n"
. "\r\n"
. "right\r\n"
. "--X--\r\n"
--- response_body
[right]
--- no_error_log
[error]
