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
* [Limitations](#limitations)
* [Compatibility](#compatibility)
* [Source Repository](#source-repository)
* [Bugs and Patches](#bugs-and-patches)
* [Copyright & License](#copyright--license)

Description
===========

This is a nginx module that reads HTTP POST and PUT request body encoded
in "application/x-www-form-urlencoded", and parse the arguments in
request body into nginx variables.

This module depends on the ngx_devel_kit (NDK) module.

This is a maintained continuation of
[calio/form-input-nginx-module](https://github.com/calio/form-input-nginx-module),
which has seen no release since 0.12 in 2016.

Installation
============

Grab the nginx source code from [nginx.org](https://nginx.org/), for example,
the version 1.28.0 (see [nginx compatibility](#compatibility)), and then build the source with this module:

```bash
wget 'https://nginx.org/download/nginx-1.28.0.tar.gz'
tar -xzvf nginx-1.28.0.tar.gz
cd nginx-1.28.0/

./configure --add-module=/path/to/ngx_devel_kit \
    --add-module=/path/to/form-input-nginx-module

make -j2
make install
```

Download the latest version of the release tarball of this module from its
[file list](https://github.com/joneum/form-input-nginx-module/tags), and the
latest tarball for [ngx_devel_kit](https://github.com/openresty/ngx_devel_kit)
from its [file list](https://github.com/openresty/ngx_devel_kit/tags).

Building as a dynamic module
----------------------------

Starting from NGINX 1.9.11, you can also compile this module as a dynamic module, by using the `--add-dynamic-module=PATH` option instead of `--add-module=PATH` on the
`./configure` command line above. And then you can explicitly load the module in your `nginx.conf` via the [load_module](http://nginx.org/en/docs/ngx_core_module.html#load_module)
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

    array_join ' ' $data; # now $data is an string
    array_join ' ' $foo;  # now $foo is an string
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

The test suite passes against these versions of nginx:

* 1.31.x (tested: 1.31.5)
* 1.28.x (tested: 1.28.0)
* 1.26.x (tested: 1.26.3)
* 1.24.x (tested: 1.24.0)
* 1.22.x (tested: 1.22.0)

Releases down to 0.8.54 were supported by earlier versions of this module
and are no longer tested.

A note on nginx 1.30.4: form_input itself works there, but
array-var-nginx-module and set-misc-nginx-module do not.  Every request
passing through array_join or set_unescape_uri terminates the worker
process, which makes set_form_input_multi and decoding unusable on that
release.  1.30.0 to 1.30.3 and 1.31.x are not affected.

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

