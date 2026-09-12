# vi:set ft= ts=4 sw=4 et fdm=marker:

use lib 'lib';
use Test::Nginx::Socket;

plan tests => repeat_each() * 2 * blocks();

no_long_string();

run_tests();

__DATA__

=== TEST 1: body buffered to a temp file
--- config
    location /bodyfile {
        client_body_buffer_size 1k;
        client_max_body_size    1m;
        set_form_input $data data;
        echo "[$data]";
    }
--- more_headers
Content-Type: application/x-www-form-urlencoded
--- request eval
"POST /bodyfile
data=" . ("a" x 4096)
--- response_body eval
"[" . ("a" x 4096) . "]\n"



=== TEST 2: temp file, wanted field is not the first one
--- config
    location /bodyfile2 {
        client_body_buffer_size 1k;
        client_max_body_size    1m;
        set_form_input $data data;
        echo "[$data]";
    }
--- more_headers
Content-Type: application/x-www-form-urlencoded
--- request eval
"POST /bodyfile2
pad=" . ("b" x 4096) . "&data=hello"
--- response_body
[hello]



=== TEST 3: temp file, field missing
--- config
    location /bodyfile3 {
        client_body_buffer_size 1k;
        client_max_body_size    1m;
        set_form_input $data data;
        echo "[$data]";
    }
--- more_headers
Content-Type: application/x-www-form-urlencoded
--- request eval
"POST /bodyfile3
pad=" . ("c" x 4096)
--- response_body
[]



=== TEST 4: temp file with set_form_input_multi
--- config
    location /bodyfile4 {
        client_body_buffer_size 1k;
        client_max_body_size    1m;
        set_form_input_multi $list data;
        array_join ',' $list;
        echo "[$list]";
    }
--- more_headers
Content-Type: application/x-www-form-urlencoded
--- request eval
"POST /bodyfile4
pad=" . ("d" x 4096) . "&data=one&data=two"
--- response_body
[one,two]



=== TEST 5: temp file with a PUT request
--- config
    location /bodyfile5 {
        client_body_buffer_size 1k;
        client_max_body_size    1m;
        set_form_input $data data;
        echo "[$data]";
    }
--- more_headers
Content-Type: application/x-www-form-urlencoded
--- request eval
"PUT /bodyfile5
pad=" . ("e" x 4096) . "&data=put-works"
--- response_body
[put-works]
