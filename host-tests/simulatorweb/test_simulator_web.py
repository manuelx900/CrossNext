import http.client
import http.server
import importlib.util
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


web = load("simulator_web", ROOT / "scripts_local/simulator_web.py")
wasm = load("wasm_build", ROOT / "tools_local/wasm/build.py")


class ServerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        (self.root / "emulator").mkdir()
        (self.root / "emulator/crossplay.js").write_bytes(b"packed fixture")
        (self.root / "plain.txt").write_text("plain fixture")
        self.site = mock.patch.object(web, "SITE", self.root)
        self.site.start()
        self.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), web.Handler)
        self.thread = threading.Thread(target=self.server.serve_forever)
        self.thread.start()
        self.url = f"http://127.0.0.1:{self.server.server_port}/"

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()
        self.site.stop()
        self.temp.cleanup()

    def request(self, path, method="GET"):
        connection = http.client.HTTPConnection("127.0.0.1", self.server.server_port)
        connection.request(method, path)
        response = connection.getresponse()
        result = response.status, dict(response.headers), response.read()
        connection.close()
        return result

    def test_page_is_isolated_and_head_matches(self):
        status, headers, body = self.request("/")
        self.assertEqual(status, 200)
        self.assertEqual(headers["Cross-Origin-Opener-Policy"], "same-origin")
        self.assertEqual(headers["Cross-Origin-Embedder-Policy"], "require-corp")
        self.assertNotIn("Content-Encoding", headers)
        self.assertEqual(int(headers["Content-Length"]), len(body))
        status, head, body = self.request("/", "HEAD")
        self.assertEqual(status, 200)
        self.assertEqual(head["Content-Length"], headers["Content-Length"])
        self.assertEqual(body, b"")

    def test_encoding_follows_served_file(self):
        status, headers, body = self.request("/emulator/crossplay.js")
        self.assertEqual(status, 200)
        self.assertEqual(headers["Content-Encoding"], "br")
        self.assertEqual(body, b"packed fixture")
        for path in ("/plain.txt", "/emulator/../plain.txt"):
            status, headers, body = self.request(path)
            self.assertEqual(status, 200)
            self.assertNotIn("Content-Encoding", headers)
            self.assertEqual(body, b"plain fixture")
        status, headers, _ = self.request("/emulator/missing.js")
        self.assertEqual(status, 404)
        self.assertNotIn("Content-Encoding", headers)

    def test_reuses_only_this_checkout(self):
        self.assertTrue(web.existing_server(self.url))
        with mock.patch.object(web, "SERVER_ID", "another-checkout"):
            # The server and client ordinarily live in different processes.
            with mock.patch.object(web.Handler, "end_headers", http.server.SimpleHTTPRequestHandler.end_headers):
                self.assertFalse(web.existing_server(self.url))


class BuildStateTests(unittest.TestCase):
    def test_build_can_clean_outputs_while_locked(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            outputs = root / ".pio/build/wasm"
            outputs.mkdir(parents=True)
            lock = root / web.BUILD_LOCK.relative_to(web.REPO)
            with mock.patch.object(web, "BUILD_LOCK", lock), \
                    mock.patch.object(web, "current_build", return_value=False), \
                    mock.patch.object(web, "existing_server", return_value=True), \
                    mock.patch.object(web, "build", side_effect=lambda: shutil.rmtree(outputs.parent)):
                web.prepare_server(8765)
            self.assertFalse(outputs.parent.exists())
            self.assertTrue(lock.exists())

    def test_concurrent_launchers_build_only_once(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            worker = root / "worker.py"
            worker.write_text('''import importlib.util
from pathlib import Path
import sys
import time
spec = importlib.util.spec_from_file_location("launcher", sys.argv[1])
web = importlib.util.module_from_spec(spec)
spec.loader.exec_module(web)
root = Path(sys.argv[2])
web.BUILD_LOCK = root / "build.lock"
web.current_build = lambda: (root / "built").exists()
def build():
    with (root / "builds.log").open("a") as log:
        log.write("build\\n")
    (root / "started").touch()
    time.sleep(0.5)
    (root / "built").touch()
web.build = build
web.existing_server = lambda url: True
web.prepare_server(8765)
''', encoding="utf-8")
            command = [sys.executable, str(worker), str(ROOT / "scripts_local/simulator_web.py"), str(root)]
            processes = []
            try:
                processes.append(subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE))
                deadline = time.monotonic() + 10
                while not (root / "started").exists() and time.monotonic() < deadline:
                    time.sleep(0.02)
                self.assertTrue((root / "started").exists(), "First build did not start")
                processes.append(subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE))
                for process in processes:
                    stdout, stderr = process.communicate(timeout=15)
                    self.assertEqual(process.returncode, 0, (stdout, stderr))
                self.assertEqual((root / "builds.log").read_text().splitlines(), ["build"])
            finally:
                for process in processes:
                    if process.poll() is None:
                        process.kill()
                    process.communicate()

    def test_source_and_artifact_changes_invalidate_state(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "emulator").mkdir()
            for name in web.ARTIFACTS:
                (root / "emulator" / name).write_bytes(name.encode())
            with mock.patch.object(web, "SITE", root), mock.patch.object(web, "STATE", root / "state.json"), \
                    mock.patch.object(web, "source_digest", return_value="source-1") as source:
                self.assertFalse(web.current_build())
                web.remember_build()
                self.assertTrue(web.current_build())
                source.return_value = "source-2"
                self.assertFalse(web.current_build())
                source.return_value = "source-1"
                (root / "emulator/crossplay.wasm").write_bytes(b"changed")
                self.assertFalse(web.current_build())
                (root / "emulator/crossplay.wasm").unlink()
                self.assertFalse(web.current_build())


class CommandTests(unittest.TestCase):
    @unittest.skipUnless(wasm.os.name == "nt", "Native Windows command-line grammar")
    def test_windows_paths_and_quoted_defines(self):
        args = wasm.split_command(r'g++ -IC:\reader\lib "C:\book folder\main.cpp" -DNAME=\"CrossPlay\"')
        self.assertEqual(args, ["g++", r"-IC:\reader\lib", r"C:\book folder\main.cpp", '-DNAME="CrossPlay"'])


if __name__ == "__main__":
    unittest.main(verbosity=2)
