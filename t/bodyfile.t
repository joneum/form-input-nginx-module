# vi:set ft= ts=4 sw=4 et fdm=marker:

use lib 'lib';
use Test::Nginx::Socket;

plan tests => repeat_each() * 3 * blocks();

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
--- no_error_log
[error]



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
--- no_error_log
[error]



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
--- no_error_log
[error]



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
--- no_error_log
[error]



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
--- no_error_log
[error]



=== TEST 6: several directives read the same temp file
--- config
    location /bodyfile6 {
        client_body_buffer_size 1k;
        client_max_body_size    1m;
        set_form_input $one one;
        set_form_input $two two;
        set_form_input $three three;
        echo "[$one|$two|$three]";
    }
--- more_headers
Content-Type: application/x-www-form-urlencoded
--- request eval
"POST /bodyfile6
pad=" . ("f" x 4096) . "&one=1&two=2&three=3"
--- response_body
[1|2|3]
--- no_error_log
[error]



=== TEST 7: client_body_in_file_only puts even a tiny body in a file
--- config
    location /bodyfile7 {
        client_body_in_file_only on;
        set_form_input $data data;
        echo "[$data]";
    }
--- more_headers
Content-Type: application/x-www-form-urlencoded
--- request
POST /bodyfile7
data=tiny-in-file
--- response_body
[tiny-in-file]
--- no_error_log
[error]
