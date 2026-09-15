#!/usr/bin/env python3
import http.server, socketserver, socket, sys, os, uuid, urllib.parse, threading

def get_local_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        # doesn't even have to be reachable
        s.connect(('10.255.255.255', 1))
        IP = s.getsockname()[0]
    except Exception:
        IP = '127.0.0.1'
    finally:
        s.close()
    return IP

class ThreadingTCPServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    daemon_threads = True

if len(sys.argv) < 3:
    print("Usage: zenith_share.py [send|recv] <path>")
    sys.exit(1)

mode = sys.argv[1]
target_path = sys.argv[2]
token = str(uuid.uuid4().hex)[:10]

if mode == "send":
    if not os.path.exists(target_path):
        sys.exit(1)
    filename = os.path.basename(target_path)
    file_size = os.path.getsize(target_path)
    size_mb = file_size / (1024 * 1024)
    
    html = f"""<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Zenith Share</title>
    <style>
        body {{ font-family: sans-serif; display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100vh; margin: 0; background-color: #121212; color: white; }}
        .card {{ background: #1e1e1e; padding: 30px; border-radius: 12px; text-align: center; box-shadow: 0 10px 30px rgba(0,0,0,0.5); max-width: 90%; width: 400px; }}
        h2 {{ margin-top: 0; word-break: break-all; color: #ffffff; }}
        p {{ color: #a0a0a0; margin-bottom: 30px; }}
        a.btn {{ display: inline-block; background: #3498db; color: white; text-decoration: none; padding: 15px 30px; border-radius: 8px; font-weight: bold; font-size: 18px; }}
    </style>
</head>
<body>
    <div class="card">
        <h2>{filename}</h2>
        <p>File Size: {size_mb:.2f} MB</p>
        <a href="/dl/{token}/{urllib.parse.quote(filename)}" class="btn" download="{filename}">Download File</a>
    </div>
</body>
</html>"""

    class SendHandler(http.server.SimpleHTTPRequestHandler):
        def log_message(self, format, *args): pass
        def address_string(self): return self.client_address[0]
        
        def do_GET(self):
            parsed = urllib.parse.urlparse(self.path)
            if parsed.path.rstrip('/') == f"/dl/{token}/{urllib.parse.quote(filename)}".rstrip('/'):
                self.send_response(200)
                self.send_header("Content-Type", "application/octet-stream")
                self.send_header("Content-Disposition", f'attachment; filename="{filename}"')
                self.send_header("Content-Length", str(file_size))
                self.end_headers()
                with open(target_path, 'rb') as f:
                    self.copyfile(f, self.wfile)
            elif parsed.path.rstrip('/') == f"/d/{token}":
                encoded = html.encode('utf-8')
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(encoded)))
                self.end_headers()
                self.wfile.write(encoded)
            else:
                self.send_response(404)
                self.send_header("Content-Type", "text/plain")
                self.end_headers()
                self.wfile.write(b"404 Not Found")

    ThreadingTCPServer.allow_reuse_address = True
    port = 43821
    httpd = None
    while port <= 43830:
        try:
            httpd = ThreadingTCPServer(("", port), SendHandler)
            break
        except Exception:
            port += 1
    
    if httpd:
        print(f"URL:http://{get_local_ip()}:{port}/d/{token}", flush=True)
        httpd.serve_forever()


elif mode == "recv":
    if not os.path.isdir(target_path):
        sys.exit(1)
    
    target_dir_name = os.path.basename(target_path)
    html = f"""<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Zenith Receive</title>
    <style>
        body {{ font-family: sans-serif; display: flex; flex-direction: column; align-items: center; padding-top: 40px; margin: 0; background-color: #121212; color: white; }}
        .card {{ background: #1e1e1e; padding: 30px; border-radius: 12px; text-align: center; box-shadow: 0 10px 30px rgba(0,0,0,0.5); max-width: 90%; width: 400px; }}
        .file-input-wrapper {{ margin: 20px 0; position: relative; overflow: hidden; display: inline-block; }}
        .file-input-wrapper input[type=file] {{ font-size: 100px; position: absolute; left: 0; top: 0; opacity: 0; cursor: pointer; height: 100%; }}
        .btn {{ display: inline-block; background: #3498db; color: white; padding: 15px 30px; border-radius: 8px; font-weight: bold; border: none; }}
        #status {{ margin-top: 20px; color: #4cd137; font-weight: bold; }}
    </style>
</head>
<body>
    <div class="card">
        <h2>Upload Files</h2>
        <p>Target: {target_dir_name}</p>
        <div class="file-input-wrapper">
            <button class="btn">Select Files</button>
            <input type="file" id="filePicker" multiple onchange="uploadFiles()">
        </div>
        <div id="status"></div>
    </div>
    <script>
        async function uploadFiles() {{
            const files = document.getElementById('filePicker').files;
            const status = document.getElementById('status');
            if (files.length === 0) return;
            
            for (let i = 0; i < files.length; i++) {{
                const file = files[i];
                status.innerText = 'Uploading ' + (i+1) + ' of ' + files.length + '...';
                await fetch('/upload/{token}', {{
                    method: 'POST',
                    headers: {{ 'X-Filename': encodeURIComponent(file.name) }},
                    body: file
                }});
            }}
            status.innerText = '✅ All files uploaded securely!';
            document.getElementById('filePicker').value = '';
        }}
    </script>
</body>
</html>"""

    class RecvHandler(http.server.SimpleHTTPRequestHandler):
        def log_message(self, format, *args): pass
        def address_string(self): return self.client_address[0]
        
        def do_GET(self):
            parsed = urllib.parse.urlparse(self.path)
            if parsed.path.rstrip('/') == f"/r/{token}":
                encoded = html.encode('utf-8')
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(encoded)))
                self.end_headers()
                self.wfile.write(encoded)
            else:
                self.send_response(404)
                self.send_header("Content-Type", "text/plain")
                self.end_headers()
                self.wfile.write(b"404 Not Found")

        def do_POST(self):
            parsed = urllib.parse.urlparse(self.path)
            if parsed.path.rstrip('/') == f"/upload/{token}":
                filename = urllib.parse.unquote(self.headers.get('X-Filename', 'uploaded_file'))
                length = int(self.headers.get('Content-Length', 0))
                
                filename = os.path.basename(filename)
                save_path = os.path.join(target_path, filename)
                
                base, ext = os.path.splitext(filename)
                counter = 1
                while os.path.exists(save_path):
                    save_path = os.path.join(target_path, f"{base} ({counter}){ext}")
                    counter += 1
                    
                with open(save_path, 'wb') as f:
                    # chunked read for large files to prevent memory exhaustion
                    remaining = length
                    while remaining > 0:
                        chunk = self.rfile.read(min(remaining, 8192))
                        if not chunk: break
                        f.write(chunk)
                        remaining -= len(chunk)
                    
                self.send_response(200)
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                self.wfile.write(b"OK")
            else:
                self.send_response(404)
                self.send_header("Content-Type", "text/plain")
                self.end_headers()
                self.wfile.write(b"404 Not Found")

    ThreadingTCPServer.allow_reuse_address = True
    port = 43821
    httpd = None
    while port <= 43830:
        try:
            httpd = ThreadingTCPServer(("", port), RecvHandler)
            break
        except Exception:
            port += 1
            
    if httpd:
        print(f"URL:http://{get_local_ip()}:{port}/r/{token}", flush=True)
        httpd.serve_forever()
