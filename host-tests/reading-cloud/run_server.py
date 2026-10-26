from pathlib import Path
import subprocess,os,socket,time,json,urllib.request,argparse,tempfile
r=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser();parser.add_argument('--server-root',required=True,type=Path);parser.add_argument('--emxx',default=str(r/'.venv/emsdk/upstream/emscripten/em++.exe'));args=parser.parse_args()
server=args.server_root.resolve();temporary=tempfile.TemporaryDirectory(prefix='crossnext-official-sync-');q=Path(temporary.name)
socket_probe=socket.socket();socket_probe.bind(('127.0.0.1',0));port=socket_probe.getsockname()[1];socket_probe.close()
url=f'http://127.0.0.1:{port}'
includes=['host-tests/reading-cloud/stubs','host-tests/reading-suite/stubs','lib/Memory','lib/Serialization','.pio/libdeps/simulator_x4_pro/ArduinoJson/src']
files=['host-tests/reading-cloud/test_http_transport.cpp','host-tests/reading-cloud/test_server.cpp','src/apps_local/reading/ReadingStatsCloudClient.cpp','src/apps_local/reading/ReadingStatsUpload.cpp','src/apps_local/reading/ReadingStatsProjection.cpp','src/apps_local/reading/ReadingStatsCloudStore.cpp','src/apps_local/reading/ReadingStatsCloudImport.cpp','src/apps_local/reading/ReadingStatsCore.cpp']
subprocess.run([args.emxx,'-std=c++20','-O1','-sENVIRONMENT=node','-sEXIT_RUNTIME=1','-sALLOW_MEMORY_GROWTH=1','-sSTACK_SIZE=1048576',*['-I'+x for x in includes],*files,'-o',str(q/'official.cjs')],cwd=r,check=True)
env=os.environ.copy();env.update(PORT=str(port),LISTEN_ADDRESS='127.0.0.1',DATABASE_PATH=str(q/f'sync-test-{port}.db'))
log=(q/'official-server.log').open('w');process=subprocess.Popen(['node','--import','tsx','src/index.ts'],cwd=server,env=env,stdout=log,stderr=subprocess.STDOUT)
def request(path,body=None,method='GET',auth=True):
 headers={'Content-Type':'application/json'}
 if auth:headers.update({'x-auth-user':'test-user','x-auth-key':'test-hash'})
 req=urllib.request.Request(url+path,data=json.dumps(body).encode() if body is not None else None,method=method,headers=headers)
 with urllib.request.urlopen(req,timeout=10) as res:return json.load(res)
try:
 for _ in range(100):
  try:request('/healthz');break
  except OSError:time.sleep(.1)
 else:raise RuntimeError('server did not start: '+(q/'official-server.log').read_text())
 request('/users/create',{'username':'test-user','password':'test-hash'},'POST',False)
 global_stats={'v':5,'device_id':'prior-reader','device':'Prior reader','seconds':300,'sessions':2,'pages':10,'completed':0,'tod':[0,300,0,0],'dow':[300,0,0,0,0,0,0],'anchor_day':9795,'history_b64':'','streak':1,'daily':[{'date':'2026-10-26','seconds':300}]}
 request('/api/v1/stats/global',global_stats,'PUT')
 book={'v':5,'document':'00000000000000000000000000000001','seconds':300,'sessions':2,'pages':10,'completed':False,'avg_fwd':30,'pace_n':10,'eta':0,'start_manual':False,'finish_manual':False,'start_date':1700000000,'finished_date':0,'tod':[0,300,0,0],'dow':[300,0,0,0,0,0,0]}
 request('/api/v1/stats/books',{'device_id':'prior-reader','items':[book]},'PUT')
 subprocess.run(['node',str(q/'official.cjs'),url],cwd=r,check=True)
 summary=request('/api/v1/stats/summary');assert (summary['seconds'],summary['pages'],summary['sessions'])==(450,16,4),summary
 devices=request('/api/v1/stats/global')['devices'];assert len(devices)==2
 print('Official server persisted totals: 450 seconds, 16 pages, 4 sessions; two origins; repeated sync unchanged.')
 print('Server revision: '+subprocess.check_output(['git','rev-parse','HEAD'],cwd=server,text=True).strip())
finally:
 process.terminate();process.wait(timeout=10);log.close();temporary.cleanup()
