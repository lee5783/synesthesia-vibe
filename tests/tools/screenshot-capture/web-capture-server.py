#!/usr/bin/env python3
#
#  web-capture-server.py
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-10-12.
#

import argparse
import functools
import http.server
import os
import signal
import sys
import urllib.parse

UPLOAD_PREFIX = "/upload/"
MAX_UPLOAD_BYTES = 64 * 1024 * 1024
EXTRA_MIME_TYPES = {
    ".wasm": "application/wasm",
    ".js": "text/javascript",
    ".mjs": "text/javascript",
    ".json": "application/json",
    ".milk": "text/plain",
    ".wav": "audio/wav",
}

HELP_EPILOG = """
Serves --site on http://<bind>:<port>/ for the web sample's capture mode and accepts
POST /upload/<out>/<file> into --out/<out>/<file>, which is the upload contract of
samples/web/capture.js (?capture=<id>&upload=http://<bind>:<port>/upload). Each upload is
written to a temporary name and renamed, so a poller that sees --out/<out>/done also sees
every file posted before it. Path segments are percent-decoded and must be plain file names
(no "/", "\\", "..", or a leading "."). Python 3 standard library only.

Prints "SERVING http://<bind>:<port>/" on stdout once listening.

EXIT CODE

  0  stopped by SIGINT/SIGTERM.
  2  usage error, --site missing, or the port could not be bound.
"""


def is_plain_name(segment):
    return (
        segment != ""
        and not segment.startswith(".")
        and "/" not in segment
        and "\\" not in segment
        and "\0" not in segment
    )


class CaptureRequestHandler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map, **EXTRA_MIME_TYPES}

    def __init__(self, *args, upload_root, **kwargs):
        self.upload_root = upload_root
        super().__init__(*args, **kwargs)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def do_POST(self):
        path = urllib.parse.urlsplit(self.path).path
        if not path.startswith(UPLOAD_PREFIX):
            self.send_error(404, "POST only accepted under " + UPLOAD_PREFIX)
            return

        segments = [urllib.parse.unquote(part) for part in path[len(UPLOAD_PREFIX):].split("/")]
        if len(segments) != 2 or not all(is_plain_name(segment) for segment in segments):
            self.send_error(400, "expected " + UPLOAD_PREFIX + "<out>/<file>")
            return

        try:
            length = int(self.headers.get("Content-Length", ""))
        except ValueError:
            self.send_error(411, "Content-Length required")
            return
        if length < 0 or length > MAX_UPLOAD_BYTES:
            self.send_error(413, "upload too large")
            return

        body = self.rfile.read(length)
        if len(body) != length:
            self.send_error(400, "short upload body")
            return

        directory = os.path.join(self.upload_root, segments[0])
        target = os.path.join(directory, segments[1])
        temporary = os.path.join(directory, "." + segments[1] + ".part")
        try:
            os.makedirs(directory, exist_ok=True)
            with open(temporary, "wb") as handle:
                handle.write(body)
            os.replace(temporary, target)
        except OSError as error:
            self.send_error(500, "could not write upload: " + str(error))
            return

        self.send_response(201)
        self.send_header("Content-Length", "0")
        self.end_headers()

    def log_message(self, format, *args):
        sys.stderr.write("%s %s\n" % (self.log_date_time_string(), format % args))


def parse_arguments():
    parser = argparse.ArgumentParser(
        prog="web-capture-server.py",
        description="Static server + upload sink for the SynesthesiaVibe web capture mode.",
        epilog=HELP_EPILOG,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--site", required=True, help="Directory to serve (build/samples/web).")
    parser.add_argument("--out", required=True, help="Directory uploads are written under.")
    parser.add_argument("--port", type=int, default=8090, help="TCP port. Default: 8090.")
    parser.add_argument("--bind", default="127.0.0.1", help="Bind address. Default: 127.0.0.1.")
    return parser.parse_args()


def raise_interrupt(signum, frame):
    raise KeyboardInterrupt


def main():
    arguments = parse_arguments()
    site = os.path.abspath(arguments.site)
    out = os.path.abspath(arguments.out)

    if not os.path.isfile(os.path.join(site, "index.html")):
        sys.stderr.write("Site not found (no index.html): %s\n" % site)
        return 2
    os.makedirs(out, exist_ok=True)

    handler = functools.partial(CaptureRequestHandler, directory=site, upload_root=out)
    try:
        server = http.server.ThreadingHTTPServer((arguments.bind, arguments.port), handler)
    except OSError as error:
        sys.stderr.write("Could not bind %s:%d: %s\n" % (arguments.bind, arguments.port, error))
        return 2

    signal.signal(signal.SIGTERM, raise_interrupt)
    print("SERVING http://%s:%d/" % (arguments.bind, arguments.port), flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
