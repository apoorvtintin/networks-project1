LISO the HTTP Web Server
========================

The Liso server supports HTTP 1.1 with 3 requests GET, HEAD and POST 
and also supports CGI.

Parser
===============
The Liso Server uses Lex and YACC for parsing HTTP requests 
in a fast and efficient manner. The parser supports multiple
headers and detect many syntax errors in requests.

Response
===============
The Liso Server serves static content stored on th server 
specified by the URI in the request.

CGI
===============
The LISO server also serves dynamic content through the CGI
feature.

Connections and timeouts
===============
Liso can mantain many connections simultanoeusly while maintinging
efficency and concurrency. Liso maintains a state for each connection
including buffers and CGI processing. The server also sends timeouts
to clients to preserve availability for other clients.

Daemonization
===============

The Liso server runs as daemon in the background like other 
nong running processes allowing the server to run other applications
as well.