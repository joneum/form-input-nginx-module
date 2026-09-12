# vi:set ft= ts=4 sw=4 et fdm=marker:

use lib 'lib';
use Test::Nginx::Socket;

plan tests => repeat_each() * blocks();

no_long_string();

run_tests();

__DATA__

=== TEST 1: set_form_input is refused outside a location
--- config
    set_form_input $data data;

    location /srv {
        echo ok;
    }
--- request
GET /srv
--- must_die



=== TEST 2: set_form_input_multi is refused outside a location
--- config
    set_form_input_multi $data data;

    location /srv {
        echo ok;
    }
--- request
GET /srv
--- must_die
