"""rrd_open, `rrdtool verify`, create and restore agree on what a valid RRD is.

Each malformed header below is written to the same file name, so the error
rrd_open reports (through `rrdtool info`) can be compared verbatim with the
one verify reports, and with the one create or restore gives when asked to
write the equivalent file.
"""
import ctypes
import json
import os
from pathlib import Path
import re
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


class Live(ctypes.Structure):
    _fields_ = [("last_up", ctypes.c_long), ("last_up_usec", ctypes.c_long)]


class Prep(ctypes.Structure):
    _fields_ = [("last_ds", ctypes.c_char * 30), ("scratch", ctypes.c_double * 10)]


# One DS, an AVERAGE RRA, then HWPREDICT with the SEASONAL, DEVSEASONAL,
# DEVPREDICT and FAILURES RRAs create adds for it.
STEP = 60
CREATE = ["--start", "1000000000", "--step", str(STEP), "DS:a:GAUGE:120:U:U",
          "RRA:AVERAGE:0.5:1:4", "RRA:HWPREDICT:20:0.1:0.0035:10"]
RRA_CNT = 6
FAILURES = 5
RRA_WINDOW_LEN = 4
DS = ctypes.sizeof(Header)
RRA0 = DS + ctypes.sizeof(Source)
WINDOW = RRA0 + FAILURES * ctypes.sizeof(Archive) + Archive.par.offset + 8 * RRA_WINDOW_LEN
LIVE = RRA0 + RRA_CNT * ctypes.sizeof(Archive)
PREP = LIVE + ctypes.sizeof(Live)


def q(value):
    return struct.pack("=Q", value)


def l(value):
    return struct.pack("=q", value)


# (description, offset, bytes, XML substitution or None)
REJECTED = [
    ("zero step", Header.pdp_step.offset, q(0),
     (r"<step>\d+</step>", "<step>0</step>")),
    ("ds_cnt beyond the file", Header.ds_cnt.offset, q(2**40), None),
    ("zero rows", RRA0 + Archive.rows.offset, q(0), None),
    ("RRA time span overflow", RRA0 + Archive.pdps.offset, q(2**62),
     (r"<pdp_per_row>1</pdp_per_row>", "<pdp_per_row>%d</pdp_per_row>" % 2**62)),
    ("negative last update", LIVE + Live.last_up.offset, l(-1),
     (r"<lastupdate>\d+</lastupdate>", "<lastupdate>-1</lastupdate>")),
    ("unnormalized microseconds", LIVE + Live.last_up_usec.offset, l(1000000), None),
    ("unknown seconds beyond the step", PREP + Prep.scratch.offset, q(STEP + 1),
     (r"<unknown_sec> *\d+ *</unknown_sec>", "<unknown_sec> %d </unknown_sec>" % (STEP + 1))),
    ("FAILURES window of 0", WINDOW, q(0),
     (r"<window_len>\d+</window_len>", "<window_len>0</window_len>")),
    ("FAILURES window above 28", WINDOW, q(29),
     (r"<window_len>\d+</window_len>", "<window_len>29</window_len>")),
    ("bad cookie", 0, b"BAD!", None),
    ("unsupported version", Header.version.offset, b"9999\0", None),
]

# Boundary values every reader must keep accepting.
ACCEPTED = [
    ("unknown seconds equal to the step", PREP + Prep.scratch.offset, q(STEP)),
    ("last microsecond", LIVE + Live.last_up_usec.offset, l(999999)),
    ("FAILURES window of 28", WINDOW, q(28)),
]


@unittest.skipUnless(ctypes.sizeof(ctypes.c_ulong) == 8 and not os.environ.get("MSYSTEM"),
                     "LP64 header mutations")
class AgreementTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.directory = Path(self.tmp.name)
        self.binary = os.environ["RRDTOOL_BIN"]
        self.env = dict(os.environ)
        self.env.pop("RRDCACHED_ADDRESS", None)
        self.check("create", "good.rrd", *CREATE)
        self.check("update", "good.rrd", "1000000060:1", "1000000120:2")
        self.good = (self.directory / "good.rrd").read_bytes()
        self.xml = self.check("dump", "good.rrd").decode()
        # the layout the offsets above assume
        self.assertEqual(struct.unpack_from("=Q", self.good, Header.rra_cnt.offset)[0],
                         RRA_CNT)
        failures = RRA0 + FAILURES * ctypes.sizeof(Archive)
        self.assertEqual(self.good[failures:failures + 9], b"FAILURES\0")
        self.assertEqual(struct.unpack_from("=Q", self.good, WINDOW)[0], 9)
        self.assertTrue(self.verify("good.rrd")["valid"])

    def run_cli(self, *args):
        return subprocess.run([self.binary, *args], cwd=self.directory, env=self.env,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def check(self, *args):
        result = self.run_cli(*args)
        self.assertEqual(result.returncode, 0, (args, result.stderr))
        return result.stdout

    def open_error(self, name):
        """The rrd_open error for name, or None when it opens."""
        result = self.run_cli("info", name)
        if result.returncode == 0:
            return None
        message = result.stderr.decode().strip()
        self.assertTrue(message.startswith("ERROR: "), message)
        return message[len("ERROR: "):]

    def verify(self, name):
        result = self.run_cli("verify", "--json", name)
        report = json.loads(result.stdout)
        self.assertEqual(result.returncode, 0 if report["valid"] else 1)
        return report

    def write(self, name, offset, value):
        damaged = bytearray(self.good)
        damaged[offset:offset + len(value)] = value
        (self.directory / name).write_bytes(damaged)

    def test_rejected_by_open_and_verify(self):
        for what, offset, value, _ in REJECTED:
            with self.subTest(what):
                self.write("bad.rrd", offset, value)
                error = self.open_error("bad.rrd")
                self.assertIsNotNone(error)
                report = self.verify("bad.rrd")
                self.assertFalse(report["valid"])
                self.assertEqual(report["errors"],
                                 [{"code": "open_failed", "message": error}])

    def test_truncated_file(self):
        (self.directory / "bad.rrd").write_bytes(self.good[:-8])
        error = self.open_error("bad.rrd")
        self.assertIn("is too small", error)
        self.assertEqual(self.verify("bad.rrd")["errors"],
                         [{"code": "open_failed", "message": error}])

    def test_boundaries_accepted(self):
        for what, offset, value in ACCEPTED:
            with self.subTest(what):
                self.write("edge.rrd", offset, value)
                self.assertIsNone(self.open_error("edge.rrd"))
                self.assertTrue(self.verify("edge.rrd")["valid"])

    def test_restore_refuses_what_open_rejects(self):
        for what, offset, value, xml in REJECTED:
            if xml is None:
                continue
            with self.subTest(what):
                self.write("bad.rrd", offset, value)
                error = self.open_error("bad.rrd")
                (self.directory / "bad.rrd").unlink()
                pattern, replacement = xml
                damaged, count = re.subn(pattern, replacement, self.xml, count=1)
                self.assertEqual(count, 1)
                (self.directory / "bad.xml").write_text(damaged)
                result = self.run_cli("restore", "bad.xml", "bad.rrd")
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stderr.decode().strip(), "ERROR: " + error)
                self.assertFalse((self.directory / "bad.rrd").exists())

    def test_restore_round_trip(self):
        (self.directory / "good.xml").write_text(self.xml)
        self.check("restore", "good.xml", "restored.rrd")
        self.assertIsNone(self.open_error("restored.rrd"))
        self.assertTrue(self.verify("restored.rrd")["valid"])

    def test_create_refuses_what_open_rejects(self):
        # the smallest pdp_cnt whose time span exceeds LONG_MAX; larger
        # values can wrap pdp_cnt * step, which create rejects itself
        pdps = (2**63 - 1) // STEP + 1
        self.write("bad.rrd", RRA0 + Archive.pdps.offset, q(pdps))
        error = self.open_error("bad.rrd")
        (self.directory / "bad.rrd").unlink()
        result = self.run_cli("create", "bad.rrd", "--step", str(STEP),
                              "DS:a:GAUGE:120:U:U", "RRA:AVERAGE:0.5:%d:4" % pdps)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stderr.decode().strip(), "ERROR: " + error)
        self.assertFalse((self.directory / "bad.rrd").exists())


if __name__ == "__main__":
    unittest.main()
