Name
====

form-input-nginx-module - read the fields of an `application/x-www-form-urlencoded`
request body into nginx variables.

Table of Contents
=================

* [Name](#name)
* [Description](#description)
* [Status](#status)
* [Synopsis](#synopsis)
* [Installation](#installation)
    * [Building as a dynamic module](#building-as-a-dynamic-module)
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

nginx hands out the arguments of a query string as `$arg_name`, but it
has nothing for the body of a form submission.  This module closes that
gap: it parses the body of a POST or PUT request carrying a content type
of `application/x-www-form-urlencoded` and assigns a field of that body
to a variable.

The work happens in the rewrite phase, before a content handler sees the
request, so the value is available to everything that reads variables
afterwards, `if`, `map`, `proxy_set_header`, the access log, a Lua
handler, and so on.

Only the locations that name a directive are affected.  Everywhere else
the module stays out of the way and nginx streams request bodies as it
normally would.

[Back to TOC](#table-of-contents)

Status
======

The module is maintained and released here.  Every change is run
against the current nginx releases and through the whole test suite
before it is pushed, and that suite is part of this repository so the
claim can be checked.  See [Test Suite](#test-suite) and
[Compatibility](#compatibility).

[Back to TOC](#table-of-contents)

Synopsis
========

```nginx
location /login {
    set_form_input      $user;          # the "user" field of the body
    set_unescape_uri    $user;          # values arrive percent encoded

    proxy_set_header    X-User $user;
    proxy_pass          http://backend;
}
```

```nginx
location /search {
    set_form_input      $q query;       # the "query" field, into $q
    set_unescape_uri    $q;

    if ($q = "") {
        return 400;
    }

    proxy_pass http://backend;
}
```

A field that a form sends more than once, `tag=a&tag=b&tag=c` from a set
of checkboxes for example, needs `set_form_input_multi` and
`array_join`:

```nginx
location /tags {
    set_form_input_multi $tags tag;
    array_join ',' $tags;               # $tags is a string from here on

    proxy_set_header X-Tags $tags;
    proxy_pass http://backend;
}
```

[Back to TOC](#table-of-contents)

Installation
============

The module needs [ngx_devel_kit](https://github.com/openresty/ngx_devel_kit)
in every build, and it has to be named before this module on the
`./configure` line.  Grab the current nginx release from
[nginx.org](https://nginx.org/en/download.html) and build the two
together:

```bash
v=1.31.5   # whatever the current release is

wget "https://nginx.org/download/nginx-$v.tar.gz"
tar -xzf "nginx-$v.tar.gz"
cd "nginx-$v"

./configure --add-module=/path/to/ngx_devel_kit \
    --add-module=/path/to/form-input-nginx-module

make -j4
make install
```

Two further modules belong on the same line in most cases, and both need
ngx_devel_kit as well:

* [set-misc-nginx-module](https://github.com/openresty/set-misc-nginx-module)
brings `set_unescape_uri`.  Field values come out of the body as they
were sent, that is percent encoded, so without it a value like
`a+b%26c` never becomes `a b&c`.

* [array-var-nginx-module](https://github.com/openresty/array-var-nginx-module)
brings `array_join`.  It is not optional for `set_form_input_multi`:
nginx refuses to start when that directive is used in a build without
it, because nothing else can read the variable it produces.

Released tarballs of this module are on the
[tags page](https://github.com/joneum/form-input-nginx-module/tags).

Building as a dynamic module
----------------------------

Pass `--add-dynamic-module=PATH` instead of `--add-module=PATH` and load
the result from `nginx.conf` with
[load_module](https://nginx.org/en/docs/ngx_core_module.html#load_module).
ngx_devel_kit has to be loaded first:

```nginx
load_module modules/ndk_http_module.so;
load_module modules/ngx_http_form_input_module.so;
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
`application/x-www-form-urlencoded` are looked at.  Parameters behind
the type do not matter, `; charset=UTF-8` is still that type, but a
longer type that merely begins with the same characters is not.
Everything else passes through and the variable stays empty.  The
variable is also empty when the field does not occur in the body.

If the field occurs more than once, the first occurrence wins.  Use
`set_form_input_multi` to get all of them.

Field names are matched without regard to case, so `set_form_input $v
data;` also reads a field sent as `DATA`.  This is what nginx itself
does for query arguments, `ngx_http_arg()` behind `$arg_name` matches
the same way.  Together with the rule above it means a client decides
which spelling wins by sending it first: out of `DATA=a&data=b` the
variable receives `a`.

The value is assigned exactly as it appears in the body, that is still
percent encoded and with `+` standing for a space.  See
[Limitations](#limitations) for how to decode it.

The directive belongs in a `location`.  Older versions of this module
also accepted it in a `server` or `http` block, where it had no effect
and left the variable empty without a warning.  nginx now refuses to
start on such a configuration.

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
can read, `array_join` in particular.  Pass the variable through one of
them before anything else touches it.

It carries that array on every request, including the ones the module
does not look at, a GET or a different content type.  The array is empty
then and `array_join` makes an empty string of it, so a location using
these two directives answers such a request normally instead of failing
it.

Writing the variable straight into a response puts the raw bytes of the
array structure there, live heap addresses included, instead of the
field values.  That is a property of the calling convention array-var
defines, and array-var's own array variables behave the same way.  In a
build without array-var nothing could read the variable at all, so nginx
refuses to start when the directive is used there.

[Back to TOC](#table-of-contents)

Limitations
===========

* Only bodies encoded as `application/x-www-form-urlencoded` are parsed.
Any other content type, `multipart/form-data` in particular, is left
alone and the variables stay empty.  File uploads are out of scope.

* Field values are handed out exactly as they appear in the body, that
is still percent encoded and with `+` for a space.  Use
`set_unescape_uri` from
[set-misc-nginx-module](https://github.com/openresty/set-misc-nginx-module)
to decode them.

* A location that names a directive reads the whole request body before
the rewrite phase finishes.  That is what the module is for, but it also
means `proxy_request_buffering off` no longer gets a location anything:
the body is already on hand before the upstream is contacted.

Request bodies that nginx writes to a temporary file, which happens as
soon as they exceed `client_body_buffer_size`, are read back from that
file since version 0.12.1.  Earlier versions silently discarded them,
which is why older documentation asked for `client_max_body_size` and
`client_body_buffer_size` to be set to the same value.  That is no
longer necessary.

[Back to TOC](#table-of-contents)

Compatibility
=============

The module is kept working with the current nginx releases.  Before
anything is pushed the test suite is run against the current mainline
and stable releases, and against older ones down to 1.22, which is the
oldest release it is checked on.

One caveat, and it is not this module's doing: on nginx 1.30.4
array-var-nginx-module and set-misc-nginx-module terminate the worker
process on every request that passes through `array_join` or
`set_unescape_uri`.  That leaves `set_form_input_multi` and decoding
unusable on that one release.  Their own test suites fail there
completely and pass on the releases before and after it, in an nginx
built without this module just the same.

[Back to TOC](#table-of-contents)

Test Suite
==========

The tests are written for
[Test::Nginx::Socket](https://metacpan.org/dist/Test-Nginx), install it
from CPAN:

```bash
cpanm --notest Test::Nginx::Socket
```

They do not exercise this module on its own.  Build an nginx that
carries ngx_devel_kit,
[echo-nginx-module](https://github.com/openresty/echo-nginx-module) for
the output, set-misc-nginx-module and array-var-nginx-module, plus this
module.  ngx_devel_kit has to come before the modules that use it:

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

`valgrind.suppress` in the repository root is picked up automatically
when the tests are run with `TEST_NGINX_USE_VALGRIND`.

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
of this repository.

[Back to TOC](#table-of-contents)

Copyright & License
===================

Copyright (c) 2010, 2011, Jiale "calio" Zhi <vipcalio@gmail.com>.

Copyright (c) 2010-2016, Yichun "agentzh" Zhang <agentzh@gmail.com>, CloudFlare Inc.

Copyright (c) 2026, Jochen Neumeister <joneum@FreeBSD.org>.

This module is licensed under the terms of the BSD 2-Clause License.
The full text is also in the LICENSE file.

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
