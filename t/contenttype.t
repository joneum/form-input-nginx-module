# vi:set ft= ts=4 sw=4 et fdm=marker:

use lib 'lib';
use Test::Nginx::Socket;

plan tests => repeat_each() * 3 * blocks();

no_long_string();

run_tests();

__DATA__

=== TEST 1: the bare media type
--- config
    location /t {
        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: application/x-www-form-urlencoded
--- request
POST /t
data=value
--- response_body
[value]
--- no_error_log
[error]



=== TEST 2: a charset parameter
--- config
    location /t {
        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: application/x-www-form-urlencoded; charset=UTF-8
--- request
POST /t
data=value
--- response_body
[value]
--- no_error_log
[error]



=== TEST 3: whitespace before the parameter separator
--- config
    location /t {
        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: application/x-www-form-urlencoded ;charset=UTF-8
--- request
POST /t
data=value
--- response_body
[value]
--- no_error_log
[error]



=== TEST 4: a longer media type that starts the same way
--- config
    location /t {
        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: application/x-www-form-urlencodedXYZ
--- request
POST /t
data=value
--- response_body
[]
--- no_error_log
[error]



=== TEST 5: a longer media type separated by a dash
--- config
    location /t {
        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: application/x-www-form-urlencoded-plus
--- request
POST /t
data=value
--- response_body
[]
--- no_error_log
[error]



=== TEST 6: an unrelated media type
--- config
    location /t {
        set_form_input $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: text/plain
--- request
POST /t
data=value
--- response_body
[]
--- no_error_log
[error]



=== TEST 7: a charset parameter in front of set_form_input_multi
--- config
    location /t {
        set_form_input_multi $data;
        array_join ',' $data;
        echo "[$data]";
    }
--- more_headers
Content-Type: application/x-www-form-urlencoded; charset=UTF-8
--- request
POST /t
data=a&data=b
--- response_body
[a,b]
--- no_error_log
[error]
