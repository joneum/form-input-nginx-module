# vi:set ft= ts=4 sw=4 et fdm=marker:

use lib 'lib';
use Test::Nginx::Socket;

plan tests => repeat_each() * 2 * blocks();

no_long_string();

run_tests();

__DATA__

=== TEST 1: empty field name given literally
--- config
    location /empty1 {
        set_form_input $data "";
        echo "[$data]";
    }
--- more_headers
Content-Type: application/x-www-form-urlencoded
--- request
POST /empty1
data=value&x=1
--- response_body
[]



=== TEST 2: empty field name coming from a variable
--- config
    location /empty2 {
        set_form_input $data $arg_field;
        echo "[$data]";
    }
--- more_headers
Content-Type: application/x-www-form-urlencoded
--- request
POST /empty2?field=
data=value&x=1
--- response_body
[]



=== TEST 3: field name coming from a variable, non-empty
--- config
    location /empty3 {
        set_form_input $data $arg_field;
        echo "[$data]";
    }
--- more_headers
Content-Type: application/x-www-form-urlencoded
--- request
POST /empty3?field=data
data=value&x=1
--- response_body
[value]



=== TEST 4: empty field name with a NUL byte in the body
--- config
    location /empty4 {
        set_form_input $data "";
        echo "[$data]";
    }
--- more_headers
Content-Type: application/x-www-form-urlencoded
--- request eval
"POST /empty4
data=value&x=\0&y=1"
--- response_body
[]



=== TEST 5: field names are matched without regard to case
--- config
    location /case1 {
        set_form_input $data data;
        echo "[$data]";
    }
--- more_headers
Content-Type: application/x-www-form-urlencoded
--- request
POST /case1
DATA=uppercase-matches
--- response_body
[uppercase-matches]
