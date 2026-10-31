"""Exercise the installed command protocol against real and damaged RRD files."""
import base64
import ctypes
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest


class Header(ctypes.Structure):
    _fields_ = [("cookie", ctypes.c_char * 4),
                ("version", ctypes.c_char * 5),
                ("float_cookie", ctypes.c_double),
                ("ds_cnt", ctypes.c_ulong), ("rra_cnt", ctypes.c_ulong),
                ("pdp_step", ctypes.c_ulong), ("par", ctypes.c_double * 10)]


class Source(ctypes.Structure):
    _fields_ = [("name", ctypes.c_char * 20), ("type", ctypes.c_char * 20),
                ("par", ctypes.c_double * 10)]


class Archive(ctypes.Structure):
    _fields_ = [("cf", ctypes.c_char * 20), ("rows", ctypes.c_ulong),
                ("pdps", ctypes.c_ulong), ("par", ctypes.c_double * 10)]


class CliTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.directory = Path(self.tmp.name)
        self.binary = os.environ["RRDTOOL_BIN"]
        self.env = dict(os.environ)
        self.env.pop("RRDCACHED_ADDRESS", None)
        self.run_cli("create", "data.rrd", "--start", "1000000000",
                     "--step", "60", "DS:a:GAUGE:120:U:U",
                     "DS:b:GAUGE:120:U:U", "RRA:AVERAGE:0.5:1:8")
        self.run_cli("update", "data.rrd", "1000000060:12:U",
                     "1000000120:24:6", "1000000180:36:9")

    def run_cli(self, *args, code=0):
        result = subprocess.run([self.binary, *args], cwd=self.directory,
                                env=self.env, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE)
        self.assertEqual(result.returncode, code,
                         (args, result.stdout, result.stderr))
        return result.stdout

    def output(self, *args, code=0):
        return json.loads(self.run_cli("--json", *args, code=code))

    def test_verify_readonly(self):
        path = self.directory / "data.rrd"
        before = (hashlib.sha256(path.read_bytes()).digest(), path.stat().st_mtime_ns)
        self.env["RRDCACHED_ADDRESS"] = "unix:/nonexistent/rrdcached.sock"
        result = self.output("verify", "--diagnose-unknown", "data.rrd")
        self.assertTrue(result["valid"])
        self.assertEqual(result["errors"], [])
        self.assertEqual(result["ds_count"], 2)
        self.assertFalse(result["unknown_diagnostics"]["historical_causes_recorded"])
        self.assertEqual(before, (hashlib.sha256(path.read_bytes()).digest(),
                                  path.stat().st_mtime_ns))
        self.assertIn(b"OK (0 errors)", self.run_cli("verify", "data.rrd"))
        self.assertTrue(json.loads(self.run_cli("verify", "data.rrd", "--json"))["valid"])
        self.assertTrue(self.output("verify", "--", "data.rrd")["valid"])
        self.assertIn(b"--diagnose-unknown", self.run_cli("help", "verify"))
        self.assertIn(b"historical causes", self.run_cli(
            "verify", "--diagnose-unknown", "data.rrd"))

    def test_unknown_counts(self):
        info = self.output("info", "data.rrd")
        path = self.directory / "data.rrd"
        content = bytearray(path.read_bytes())
        values = [float("nan"), float("inf"), 3.5, float("-inf")] * 4
        content[info["header_size"]:] = struct.pack("=16d", *values)
        path.write_bytes(content)
        result = self.output("verify", "--diagnose-unknown", "data.rrd")
        cells = result["unknown_diagnostics"]["archives"][0]["data_sources"]
        self.assertEqual((cells[0]["known"], cells[0]["unknown"]), (4, 4))
        self.assertEqual(cells[1]["infinite"], 8)
        self.assertEqual(cells[1]["unknown"], 0)
        data = self.output("fetch", "data.rrd", "AVERAGE", "--start", "1000000000",
                           "--end", "1000000180")
        emitted = [value for row in data["data"] for value in row["values"]]
        self.assertIn("Infinity", emitted)
        self.assertIn("-Infinity", emitted)

    def test_chunk_boundaries(self):
        self.run_cli("create", "wide.rrd", "--start", "1000000000", "--step", "60",
                     "DS:a:GAUGE:120:U:U", "DS:b:GAUGE:120:U:U", "DS:c:GAUGE:120:U:U",
                     "RRA:AVERAGE:0.5:1:1000", "RRA:MAX:0.5:2:600")
        header = self.output("info", "wide.rrd")["header_size"]
        path = self.directory / "wide.rrd"
        content = bytearray(path.read_bytes())
        content[header:] = struct.pack("=4800d", *([3.5, float("nan"), float("inf")] * 1600))
        path.write_bytes(content)
        result = self.output("verify", "--diagnose-unknown", "wide.rrd")
        for archive, rows in zip(result["unknown_diagnostics"]["archives"], (1000, 600)):
            cells = archive["data_sources"]
            self.assertEqual(cells[0]["known"], rows)
            self.assertEqual(cells[1]["unknown"], rows)
            self.assertEqual(cells[2]["infinite"], rows)

    def test_existing_reports(self):
        info = self.output("info", "data.rrd")
        self.assertEqual(info["step"], 60)
        self.assertIsNone(info["ds[a].min"])
        self.assertEqual(self.output("last", "data.rrd")["timestamp"], 1000000180)
        self.assertIsInstance(self.output("first", "data.rrd")["timestamp"], int)
        self.assertEqual(self.output("lastupdate", "data.rrd")["values"], ["36", "9"])
        data = self.output("fetch", "data.rrd", "AVERAGE", "--start", "1000000000",
                           "--end", "1000000240")
        self.assertEqual(data["ds_names"], ["a", "b"])
        self.assertEqual(len(data["data"]), (data["end"] - data["start"]) // 60)
        self.assertTrue(any(None in row["values"] for row in data["data"]))
        self.assertEqual(json.loads(self.run_cli("info", "--json", "data.rrd")), info)
        self.assertIsInstance(self.output("updatev", "data.rrd", "1000000240:48:12"), dict)
        self.assertIn("data.rrd", self.output("list", ".")["listing"])

    def test_raw_counter_and_unknown(self):
        self.run_cli("create", "counter.rrd", "--start", "1000000000", "--step", "60",
                     "DS:c:COUNTER:120:U:U", "RRA:AVERAGE:0.5:1:4")
        self.run_cli("update", "counter.rrd", "1000000060:18446744073709551610")
        self.assertEqual(self.output("lastupdate", "counter.rrd")["values"],
                         ["18446744073709551610"])
        self.run_cli("update", "counter.rrd", "1000000120:U")
        self.assertEqual(self.output("lastupdate", "counter.rrd")["values"], [None])

    def test_remote_protocol(self):
        result = subprocess.run([self.binary, "-"], cwd=self.directory, env=self.env,
                                input=(b"--json info data.rrd\n"
                                       b"--json info missing.rrd\n"
                                       b"verify --json --diagnose-unknown data.rrd\n"
                                       b"--json last data.rrd\n"),
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = result.stdout.splitlines()
        self.assertEqual(len(lines), 8, result.stdout)
        self.assertEqual(json.loads(lines[0])["step"], 60)
        self.assertTrue(lines[1].startswith(b"OK"))
        self.assertIn("error", json.loads(lines[2]))
        self.assertTrue(lines[3].startswith(b"ERROR:"))
        self.assertTrue(json.loads(lines[4])["valid"])
        self.assertTrue(lines[5].startswith(b"OK"))
        self.assertEqual(json.loads(lines[6])["timestamp"], 1000000180)
        self.assertTrue(lines[7].startswith(b"OK"))

    def test_graph_and_xport(self):
        definition = "DEF:a=data.rrd:a:AVERAGE"
        interval = ["--start", "1000000000", "--end", "1000000240"]
        probe = subprocess.run([self.binary, "graphv", "-", *interval,
                                definition, "LINE1:a#ff0000"], cwd=self.directory,
                               env=self.env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if probe.returncode:
            if b"compiled without graphics" in probe.stderr:
                self.skipTest("graph support unavailable")
            self.fail("graph probe failed: " + probe.stderr.decode())
        for command in ("graph", "graphv"):
            graph = self.output(command, "-", *interval, definition, "LINE1:a#ff0000")
            self.assertEqual(graph["image"]["encoding"], "base64")
            self.assertTrue(base64.b64decode(graph["image"]["data"]).startswith(b"\x89PNG"))
        for title in ("\nleading\n\nsecond", "one<br>two\\nthree\nfour"):
            graph = self.output("graphv", "-", *interval, "--title", title,
                                definition, "LINE1:a#ff0000")
            self.assertIn("image", graph)
        args = [*interval, definition, "XPORT:a:values"]
        native = json.loads(self.run_cli("xport", "--json", *args))
        self.assertEqual(self.output("xport", *args), native)

    def test_errors_and_no_mutation(self):
        self.assertIn("error", self.output("xport", "DEF:" + ":".join(["x"] * 11), code=1))
        for command in ("info", "fetch", "lastupdate", "first", "last", "graphv", "xport"):
            with self.subTest(command=command):
                self.assertIn("error", self.output(command, "missing.rrd", code=1))
        path = self.directory / "data.rrd"
        before = path.read_bytes()
        self.assertIn("error", self.output("update", "data.rrd", "1000000240:1:2", code=1))
        self.assertEqual(path.read_bytes(), before)
        for args in (("missing.rrd",), ("%s%n.rrd",), ("--bad", "data.rrd"), (),
                     ("data.rrd", "extra.rrd")):
            result = self.output("verify", *args, code=1)
            self.assertFalse(result["valid"])
            self.assertTrue(result["errors"])

    def test_filename_encoding(self):
        windows = os.name == "nt" or bool(os.environ.get("MSYSTEM"))
        unicode_name = 'é日😀.rrd'
        # Pipe input specifies UTF-8 independently of Windows' ANSI argv
        # conversion and its existing CreateFileA file opener.
        result = subprocess.run([self.binary, "-"], cwd=self.directory, env=self.env,
                                input=('verify --json "' + unicode_name + '"\n').encode('utf-8'),
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = result.stdout.splitlines()
        self.assertEqual(len(lines), 2, result.stdout)
        self.assertEqual(json.loads(lines[0])["filename"], unicode_name)
        self.assertTrue(lines[1].startswith(b"ERROR:"))
        names = ['quote"\\\n.rrd']
        if not windows:
            names.append(unicode_name)
        for name in names:
            if windows:
                # The existing Windows RRD opener uses CreateFileA. Check
                # JSON encoding without requiring Unicode file-open support.
                result = self.output("verify", name, code=1)
            else:
                path = self.directory / name
                path.write_bytes((self.directory / "data.rrd").read_bytes())
                result = self.output("verify", name)
            self.assertEqual(result["filename"], name)
        if not windows:
            name = os.fsdecode(b"invalid-\xff-\xe0.rrd")
            (self.directory / name).write_bytes((self.directory / "data.rrd").read_bytes())
            self.assertTrue(self.output("verify", name)["valid"])

    def test_compute_and_holt_winters(self):
        self.run_cli("create", "compute.rrd", "--start", "1000000000", "--step", "60",
                     "DS:a:GAUGE:120:U:U", "DS:b:COMPUTE:a,2,*", "RRA:AVERAGE:0.5:1:8")
        self.assertTrue(self.output("verify", "--diagnose-unknown", "compute.rrd")["valid"])
        self.run_cli("create", "hw.rrd", "--start", "1000000000", "--step", "60",
                     "DS:a:GAUGE:120:U:U", "RRA:MHWPREDICT:20:0.1:0.0035:10")
        self.assertTrue(self.output("verify", "--diagnose-unknown", "hw.rrd")["valid"])

    @unittest.skipUnless(ctypes.sizeof(ctypes.c_ulong) == 8 and not os.environ.get("MSYSTEM"),
                         "LP64 header mutations")
    def test_compute_and_hw_damage(self):
        self.test_compute_and_holt_winters()
        path = self.directory / "compute.rrd"
        original = path.read_bytes()
        program = ctypes.sizeof(Header) + ctypes.sizeof(Source) + Source.par.offset
        # Compact RPN nodes contain a signed char operator and a short operand.
        for node in (struct.pack("=bxh", 32, 0),  # empty END
                     struct.pack("=bxh", 127, 0), # unknown operator
                     struct.pack("=bxh", 7, 0),   # prohibited TIME
                     struct.pack("=bxh", 1, 9)):  # invalid source reference
            damaged = bytearray(original)
            damaged[program:program + 4] = node
            path.write_bytes(damaged)
            self.assertFalse(self.output("verify", "compute.rrd", code=1)["valid"])
        damaged = bytearray(original)
        damaged[program:program + 80] = struct.pack("=bxh", 0, 1) * 20
        path.write_bytes(damaged)
        self.assertFalse(self.output("verify", "compute.rrd", code=1)["valid"])
        path = self.directory / "hw.rrd"
        original = path.read_bytes()
        first = ctypes.sizeof(Header) + ctypes.sizeof(Source)
        cases = [(first + Archive.par.offset + 8, struct.pack("=d", -1)),
                 (first + Archive.par.offset + 16, struct.pack("=d", 2)),
                 (first + Archive.par.offset + 24, struct.pack("=Q", 999)),
                 (first + ctypes.sizeof(Archive) + Archive.par.offset + 8,
                  struct.pack("=d", -1)),
                 (first + ctypes.sizeof(Archive) + Archive.par.offset + 16,
                  struct.pack("=d", 2))]
        for offset, value in cases:
            damaged = bytearray(original)
            damaged[offset:offset + len(value)] = value
            path.write_bytes(damaged)
            self.assertFalse(self.output("verify", "hw.rrd", code=1)["valid"])

    @unittest.skipUnless(ctypes.sizeof(ctypes.c_ulong) == 8 and not os.environ.get("MSYSTEM"),
                         "LP64 header mutations")
    def test_damaged_headers(self):
        original = (self.directory / "data.rrd").read_bytes()
        ds = ctypes.sizeof(Header)
        rra = ds + 2 * ctypes.sizeof(Source)
        header_size = self.output("info", "data.rrd")["header_size"]
        live = rra + ctypes.sizeof(Archive)
        cases = [(0, b"BAD!"), (4, b"12345"),
                 (4, b"9999\0"), (4, b"%s%n\0"), (4, b"0000\0"),
                 (Header.pdp_step.offset, struct.pack("=Q", 0)),
                 (Header.ds_cnt.offset, struct.pack("=Q", 2**63)),
                 (ds, b"x" * 20), (ds, b"?\0"), (ds, b"\0"),
                 (ds + Source.type.offset, b"X\0"),
                 (ds + Source.type.offset, b"X" * 20),
                 (ds + ctypes.sizeof(Source), b"a\0"),
                 (ds + Source.par.offset + 8, struct.pack("=2d", 10, 1)),
                 (live + 8, struct.pack("=q", 1000000)),
                 (live + 16, b"X" * 30),
                 (header_size - 8, struct.pack("=Q", 8)),
                 (rra, b"INVALID\0"), (rra, b"X" * 20),
                 (rra + Archive.rows.offset, struct.pack("=Q", 0)),
                 (rra + Archive.pdps.offset, struct.pack("=Q", 2**63)),
                 (rra + Archive.par.offset, struct.pack("=d", 1.0)),
                 (rra + Archive.par.offset, struct.pack("=d", float("nan")))]
        for offset, value in cases:
            with self.subTest(offset=offset, value=value):
                damaged = bytearray(original)
                damaged[offset:offset + len(value)] = value
                (self.directory / "bad.rrd").write_bytes(damaged)
                result = self.output("verify", "--diagnose-unknown", "bad.rrd", code=1)
                self.assertFalse(result["valid"])
                self.assertTrue(result["errors"])
        for damaged in (original[:20], original[:-8], original + b"trailer"):
            (self.directory / "bad.rrd").write_bytes(damaged)
            self.assertFalse(self.output("verify", "bad.rrd", code=1)["valid"])
        damaged = bytearray(original)
        damaged[ds:ds + 20] = b"X" * 20
        damaged[rra:rra + 20] = b"X" * 20
        (self.directory / "bad.rrd").write_bytes(damaged)
        self.assertGreaterEqual(len(self.output("verify", "bad.rrd", code=1)["errors"]), 2)
        self.assertIn(b"INVALID", self.run_cli("verify", "bad.rrd", code=1))


if __name__ == "__main__":
    unittest.main()
