Name
====

form-input-nginx-module - NGINX module that reads HTTP POST and PUT request body encoded in "application/x-www-form-urlencoded" and parses the arguments into nginx variables.

Table of Contents
=================

* [Name](#name)
* [Description](#description)
* [Installation](#installation)
    * [Building as a dynamic module](#building-as-a-dynamic-module)
* [Usage](#usage)
* [Directives](#directives)
    * [set_form_input](#set_form_input)
    * [set_form_input_multi](#set_form_input_multi)
* [Limitations](#limitations)
* [Compatibility](#compatibility)
* [Test Suite](#test-suite)
* [Source Repository](#source-repository)
* [Bugs and Patches](#bugs-and-patches)
* [Copyright & License](#copyright--license)

Description
===========

This is an nginx module that reads HTTP POST and PUT request bodies
encoded in "application/x-www-form-urlencoded" and parses the fields of
that body into nginx variables.

This module depends on the ngx_devel_kit (NDK) module.

This is a maintained continuation of
[calio/form-input-nginx-module](https://github.com/calio/form-input-nginx-module),
which has seen no release since 0.12 in 2016.

Installation
============

Download the release tarball of this module from its
[file list](https://github.com/joneum/form-input-nginx-module/tags) and the
tarball for [ngx_devel_kit](https://github.com/openresty/ngx_devel_kit)
from its [file list](https://github.com/openresty/ngx_devel_kit/tags).  Then
grab the nginx source code from [nginx.org](https://nginx.org/), for example
the current mainline version, and build it with both modules:

```bash
wget 'https://nginx.org/download/nginx-1.31.5.tar.gz'
tar -xzvf nginx-1.31.5.tar.gz
cd nginx-1.31.5/

./configure --add-module=/path/to/ngx_devel_kit \
    --add-module=/path/to/form-input-nginx-module

make -j2
make install
```

Read [Compatibility](#compatibility) before picking a version, the current
stable release needs a caveat.

Two further modules are worth adding on the same command line.
[set-misc-nginx-module](https://github.com/openresty/set-misc-nginx-module)
brings `set_unescape_uri`, which decodes the field values, and
[array-var-nginx-module](https://github.com/openresty/array-var-nginx-module)
brings `array_join`, without which `set_form_input_multi` cannot be used at
all.  Both need ngx_devel_kit as well, so put it first in either case.

Building as a dynamic module
----------------------------

Starting from NGINX 1.9.11, you can also compile this module as a dynamic module, by using the `--add-dynamic-module=PATH` option instead of `--add-module=PATH` on the
`./configure` command line above. And then you can explicitly load the module in your `nginx.conf` via the [load_module](https://nginx.org/en/docs/ngx_core_module.html#load_module)
directive, for example,

```nginx
load_module /path/to/modules/ndk_http_module.so;  # assuming NDK is built as a dynamic module too
load_module /path/to/modules/ngx_http_form_input_module.so;
```

[Back to TOC](#table-of-contents)

Usage
=====

```nginx
set_form_input $variable;
set_form_input $variable argument;

set_form_input_multi $variable;
set_form_input_multi $variable argument;
```

example:

```nginx
#nginx.conf

location /foo {
    client_max_body_size 100k;

    set_form_input $data;    # read "data" field into $data
    set_form_input $foo foo; # read "foo" field into $foo
}

location /bar {
    client_max_body_size 1m;

    set_form_input_multi $data; # read all "data" field into $data
    set_form_input_multi $foo data; # read all "data" field into $foo

    array_join ' ' $data; # now $data is a string
    array_join ' ' $foo;  # now $foo is a string
}

location /baz {
    client_max_body_size 100k;

    # values come out of the body as they were sent, that is still
    # percent encoded and with '+' for a space.  set_unescape_uri from
    # set-misc-nginx-module turns "a+b%26c" into "a b&c".
    set_form_input $data;
    set_unescape_uri $data;
}
```

[Back to TOC](#table-of-contents)

Directives
==========

set_form_input
--------------

**syntax:** *set_form_input $variable*

**syntax:** *set_form_input $variable field*

**default:** *no*

**context:** *location*

**phase:** *rewrite*

Reads a field out of the request body and assigns it to `$variable`.
When no field name is given, the name of the variable without the
leading `$` is used, so `set_form_input $data;` reads the field named
`data`.

Only POST and PUT requests carrying a content type of
`application/x-www-form-urlencoded` are looked at.  Everything else
passes through and the variable stays empty.  The variable is also empty
when the field does not occur in the body.

If the field occurs more than once, the first occurrence wins.  Use
`set_form_input_multi` to get all of them.

The value is assigned exactly as it appears in the body, that is still
percent encoded and with `+` standing for a space.  See
[Limitations](#limitations) for how to decode it.

The directive belongs in a `location`.  Older versions of this module
also accepted it in a `server` or `http` block, where it had no effect
and left the variable empty without a warning.  Since 0.12.2 nginx
refuses to start on such a configuration.

[Back to TOC](#table-of-contents)

set_form_input_multi
--------------------

**syntax:** *set_form_input_multi $variable*

**syntax:** *set_form_input_multi $variable field*

**default:** *no*

**context:** *location*

**phase:** *rewrite*

Behaves like `set_form_input`, but collects every occurrence of the
field instead of only the first one.  The note about `server` and `http`
blocks above applies here as well.

The variable does not hold a string afterwards.  It carries an array
that only the directives of
[array-var-nginx-module](https://github.com/openresty/array-var-nginx-module)
can read, `array_join` in particular.  Printing the variable on its own
yields the raw bytes of the array structure, not the field values.

[Back to TOC](#table-of-contents)

Limitations
===========

* Only bodies encoded as `application/x-www-form-urlencoded` are parsed.
Any other content type, `multipart/form-data` in particular, is left alone.

* Field values are handed out exactly as they appear in the body, that is
still percent encoded. Use `set_unescape_uri` from
[set-misc-nginx-module](https://github.com/openresty/set-misc-nginx-module)
to decode them.

Request bodies that nginx buffers to a temporary file, which happens as soon
as they exceed `client_body_buffer_size`, are read back from that file since
version 0.12.1. Earlier versions discarded them, which is why older
documentation asked for `client_max_body_size` and `client_body_buffer_size`
to be set to the same value. That is no longer necessary.

[Back to TOC](#table-of-contents)

Compatibility
=============

This module is kept working with the current nginx releases, mainline
1.31.5 and stable 1.30.4.  The test suite is also run against older
releases down to 1.22 and passes there.

A caveat on the current stable, nginx 1.30.4: form_input itself works on
it, but array-var-nginx-module and set-misc-nginx-module do not.  Every
request passing through `array_join` or `set_unescape_uri` terminates
the worker process, which leaves `set_form_input_multi` and decoding
unusable on that release.  Measured against their own test suites, which
fail completely on 1.30.4 and pass on 1.30.0 to 1.30.3 and on 1.31.x.
The cause was not tracked down further, form_input is not involved: the
same failure occurs in an nginx built without this module.

[Back to TOC](#table-of-contents)

Test Suite
==========

The tests are written for
[Test::Nginx::Socket](https://metacpan.org/dist/Test-Nginx), install it from
CPAN:

```bash
cpanm --notest Test::Nginx::Socket
```

They do not exercise this module on its own.  Build an nginx that carries
ngx_devel_kit, [echo-nginx-module](https://github.com/openresty/echo-nginx-module)
for the output, [set-misc-nginx-module](https://github.com/openresty/set-misc-nginx-module)
and [array-var-nginx-module](https://github.com/openresty/array-var-nginx-module),
plus this module.  ngx_devel_kit has to come before the modules that use it:

```bash
./configure --prefix=/tmp/nginx-test \
    --add-module=/path/to/ngx_devel_kit \
    --add-module=/path/to/echo-nginx-module \
    --add-module=/path/to/form-input-nginx-module \
    --add-module=/path/to/set-misc-nginx-module \
    --add-module=/path/to/array-var-nginx-module
make && make install
```

Then point the suite at that binary and run it from the root of this
repository:

```bash
TEST_NGINX_BINARY=/tmp/nginx-test/sbin/nginx prove -r t/
```

`valgrind.suppress` in the repository root is picked up automatically when
the tests are run with `TEST_NGINX_USE_VALGRIND`.

[Back to TOC](#table-of-contents)

Source Repository
=================

This module is hosted at
[github.com/joneum/form-input-nginx-module](https://github.com/joneum/form-input-nginx-module)
and maintained by Jochen Neumeister.

[Back to TOC](#table-of-contents)

Bugs and Patches
================

Please report bugs and send patches through the
[GitHub issue tracker](https://github.com/joneum/form-input-nginx-module/issues)
of this repository.  The issue tracker of the original project is not
watched.

[Back to TOC](#table-of-contents)

Copyright & License
===================

Copyright (c) 2010, 2011, Jiale "calio" Zhi <vipcalio@gmail.com>.

Copyright (c) 2010-2016, Yichun "agentzh" Zhang <agentzh@gmail.com>, CloudFlare Inc.

Copyright (c) 2026, Jochen Neumeister <joneum@FreeBSD.org>.

This module is licensed under the terms of the BSD license.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

[Back to TOC](#table-of-contents)

