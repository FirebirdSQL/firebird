------------------
Connection strings
------------------

Connection string is a local or remote path to the database being attached.

The legacy syntax (supported by all Firebird versions) is the following:

For TCP (aka INET) protocol:

  <host> [ / <port>] : <file path to database or alias>

For local connections as simple as:

  <file path to database or alias>

If host name is omitted, local connection is implied. Depending on settings,
platform and Firebird version, it could be performed via either the embedded
engine or XNET (shared memory) protocol or TCP localhost loopback.

Examples:

  Connect via TCP using database name:

    192.168.0.11:/db/mydb.fdb
    192.168.0.11:C:\db\mydb.fdb
    myserver:C:\db\mydb.fdb
    localhost:/db/mydb.fdb

  Connect via TCP using database alias:

    192.168.0.11:mydb
    myserver:mydb
    localhost:mydb

  Connect via TCP using non-default port 3051:

    192.168.0.11/3051:C:\db\mydb.fdb
    192.168.0.11/3051:mydb
    myserver/3051:/db/mydb.fdb
    localhost/3051:/db/mydb.fdb
    myserver/3051:mydb
    localhost/3051:mydb

  Connect via TCP using non-default service name:

    192.168.0.11/fb_db:C:\db\mydb.fdb
    192.168.0.11/fb_db:mydb
    localhost/fb_db:/db/mydb.fdb
    myserver/fb_db:/db/mydb.fdb
    myserver/fb_db:mydb
    localhost/fb_db:mydb

  Local connection:

    /db/mydb.fdb
    C:\db\mydb.fdb
    mydb

Additionally, Firebird 3.0 introduces generalized URL-like syntax for
connection strings:

  [ <protocol> : // [ <host> [ : <port> ] ] ] / <file path to database or alias>

Where protocol is one of: INET (means TCP) or XNET (means shared memory).

Examples:

  Connect via TCP using database name:

    inet://192.168.0.11//db/mydb.fdb
    inet://192.168.0.11/C:\db\mydb.fdb
    inet://myserver/C:\db\mydb.fdb
    inet://localhost//db/mydb.fdb

  Connect via TCP using database alias:

    inet://192.168.0.11/mydb
    inet://myserver/mydb
    inet://localhost/mydb

  Connect via TCP using non-default port 3051:

    inet://192.168.0.11:3051/C:\db\mydb.fdb
    inet://192.168.0.11:3051/mydb
    inet://myserver:3051//db/mydb.fdb
    inet://localhost:3051//db/mydb.fdb
    inet://myserver:3051/mydb
    inet://localhost:3051/mydb

  Connect via TCP using non-default service name:

    inet://192.168.0.11:fb_db/C:\db\mydb.fdb
    inet://192.168.0.11:fb_db/mydb
    inet://localhost:fb_db//db/mydb.fdb
    inet://myserver:fb_db//db/mydb.fdb
    inet://myserver:fb_db/mydb
    inet://localhost:fb_db/mydb

  The "inet" protocol can be replaced by "inet4" or "inet6" to restrict client
  to IPv4 or IPv6 addresses corresponding to supplied name ("inet" protocol
  tries all addresses in the order determined by OS):

    inet4://myserver/mydb
    inet6://myserver/mydb

  Loopback connection via TCP:

    inet:///db/mydb.fdb
    inet://C:\db\mydb.fdb
    inet://mydb

  Local connection via shared memory:

    xnet://C:\db\mydb.fdb
    xnet://mydb

  Local (embedded by default) connection:

    /db/mydb.fdb
    C:\db\mydb.fdb
    mydb

If protocol and host name are omitted, local connection is implied. Depending on settings,
it could be performed via either the embedded engine or XNET (shared memory) protocol
or TCP localhost loopback.

On the server side, default provider configuration is:

  Providers = Remote, Engine13, Loopback

It means that if the remote provider fails to match the connection string (because of missing
protocol / host parts), then the embedded engine handles the hostless connection. If you need
to connect locally using a specific transport protocol, please specify:

  inet://<file path to database or alias>
  or
  xnet://<file path to database or alias>

Note: XNET (shared memory) protocol is available on Windows only.

