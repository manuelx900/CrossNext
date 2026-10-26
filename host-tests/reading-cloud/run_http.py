"""Loopback HTTP + production client/import/store. Emscripten SDK boundary uses curl, not device TLS."""
from pathlib import Path
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
import argparse,json,subprocess,tempfile,threading
root=Path(__file__).resolve().parents[2]
class Server(BaseHTTPRequestHandler):
    calls=[]
    def log_message(self,*args):pass
    def do_GET(self):
        assert self.headers.get('x-auth-user')=='test-user'
        assert self.headers.get('x-auth-key')=='test-hash'
        self.calls.append(self.path)
        mode=self.path.split('/')[1];status=200
        if mode=='auth':status=401
        elif mode=='unsupported':status=404
        elif mode=='redirect':status=302
        summary={'seconds':600,'sessions':2,'pages':15,'completed':1,'daily':[{'date':'2026-10-26','seconds':600}]}
        item={'document':'00000000000000000000000000000001','device_id':'device-a','title':'HTTP book','stats':{'seconds':600,'sessions':2,'pages':15,'completed':True}}
        payload=summary if self.path.endswith('summary') else {'items':[item],'next':None}
        body=json.dumps(payload).encode()
        if mode=='invalid':body=b'{'
        if mode=='large':body=b'x'*(2*1024*1024+1)
        self.send_response(status);self.send_header('Content-Length',str(len(body)+(100 if mode=='partial' else 0)));self.end_headers()
        self.wfile.write(body);self.close_connection=True
parser=argparse.ArgumentParser();parser.add_argument('--emxx',default=str(root/'.venv/emsdk/upstream/emscripten/em++.exe'));args=parser.parse_args()
with tempfile.TemporaryDirectory(prefix='crossnext-sync-') as out:
    target=Path(out)/'http.cjs'
    includes=['host-tests/reading-cloud/stubs','host-tests/reading-suite/stubs','lib/Memory','lib/Serialization','.pio/libdeps/simulator_x4_pro/ArduinoJson/src']
    files=['host-tests/reading-cloud/test_http_transport.cpp','src/apps_local/reading/ReadingStatsProjection.cpp','host-tests/reading-cloud/test_http.cpp','src/apps_local/reading/ReadingStatsCloudClient.cpp','src/apps_local/reading/ReadingStatsCloudImport.cpp','src/apps_local/reading/ReadingStatsCloudStore.cpp','src/apps_local/reading/ReadingStatsCore.cpp']
    subprocess.run([args.emxx,'-std=c++20','-O1','-sENVIRONMENT=node','-sEXIT_RUNTIME=1','-sALLOW_MEMORY_GROWTH=1','-sSTACK_SIZE=1048576',*['-I'+x for x in includes],*files,'-o',str(target)],cwd=root,check=True)
    server=ThreadingHTTPServer(('127.0.0.1',0),Server);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    try:subprocess.run(['node',str(target),f'http://127.0.0.1:{server.server_port}'],check=True)
    finally:server.shutdown();thread.join()
    assert len(Server.calls)>=10
    print(f'Loopback server: {len(Server.calls)} authenticated requests checked')
