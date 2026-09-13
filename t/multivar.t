# vi:set ft= ts=4 sw=4 et fdm=marker:

# set_form_input_multi has to leave a readable array behind on every
# request, not only on the ones that carry a form body.  array_join
# refuses anything that is not an array and fails the request, so a
# request the module skips used to answer with a 500.

use lib 'lib';
use Test::Nginx::Socket;

plan tests => repeat_each() * 3 * blocks();

no_long_string();

run_tests();

__DATA__

=== TEST 1: GET
--- config
    location /t {
        set_form_input_multi $tags tag;
        array_join ',' $tags;
        echo "[$tags]";
    }
--- request
GET /t
--- response_body
[]
--- no_error_log
[error]



=== TEST 2: POST without a content type
--- config
    location /t {
        set_form_input_multi $tags tag;
        array_join ',' $tags;
        echo "[$tags]";
    }
--- request
POST /t
tag=a&tag=b
--- response_body
[]
--- no_error_log
[error]



=== TEST 3: POST with an unrelated content type
--- config
    location /t {
        set_form_input_multi $tags tag;
        array_join ',' $tags;
        echo "[$tags]";
    }
--- more_headers
Content-Type: text/plain
--- request
POST /t
tag=a&tag=b
--- response_body
[]
--- no_error_log
[error]



=== TEST 4: DELETE
--- config
    location /t {
        set_form_input_multi $tags tag;
        array_join ',' $tags;
        echo "[$tags]";
    }
--- more_headers
Content-Type: application/x-www-form-urlencoded
--- request
DELETE /t
--- response_body
[]
--- no_error_log
[error]



=== TEST 5: the field is missing from a body that is parsed
--- config
    location /t {
        set_form_input_multi $tags tag;
        array_join ',' $tags;
        echo "[$tags]";
    }
--- more_headers
Content-Type: application/x-www-form-urlencoded
--- request
POST /t
other=a
--- response_body
[]
--- no_error_log
[error]



=== TEST 6: the field is there
--- config
    location /t {
        set_form_input_multi $tags tag;
        array_join ',' $tags;
        echo "[$tags]";
    }
--- more_headers
Content-Type: application/x-www-form-urlencoded
--- request
POST /t
tag=a&tag=b&tag=c
--- response_body
[a,b,c]
--- no_error_log
[error]
