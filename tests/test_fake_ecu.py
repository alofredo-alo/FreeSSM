#!/usr/bin/env python3
"""Exercise the simulator over its pseudo-terminal, without a vehicle."""
import os
import pathlib
import select
import subprocess
import sys
import termios
import unittest

SIMULATOR = pathlib.Path(__file__).resolve().parents[1] / 'tools' / 'fake_ecu.py'


def frame(payload):
    msg = bytes([0x80, 0x10, 0xF0, len(payload)]) + payload
    return msg + bytes([sum(msg) & 0xff])


class SimulatorTest(unittest.TestCase):
    def start_ecu(self, *args):
        proc = subprocess.Popen([sys.executable, '-u', str(SIMULATOR), '--fast', *args],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.addCleanup(self.stop_ecu, proc)
        ready, _, _ = select.select([proc.stdout], [], [], 5)
        self.assertTrue(ready, 'simulator did not start')
        line = proc.stdout.readline().strip()
        self.assertIn('listening on /dev/pts/', line)
        fd = os.open(line.rsplit(' ', 1)[-1], os.O_RDWR | os.O_NOCTTY)
        self.addCleanup(os.close, fd)
        termios.tcflush(fd, termios.TCIOFLUSH)
        return proc, fd

    @staticmethod
    def stop_ecu(proc):
        proc.terminate()
        try:
            proc.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.communicate()

    def read_bytes(self, fd, count):
        result = b''
        while len(result) < count:
            ready, _, _ = select.select([fd], [], [], 3)
            self.assertTrue(ready, 'timed out waiting for simulator')
            result += os.read(fd, count - len(result))
        return result

    def test_identification_with_echo_and_without_echo(self):
        for no_echo in (False, True):
            with self.subTest(no_echo=no_echo):
                proc, fd = self.start_ecu(*(['--no-echo'] if no_echo else []))
                request = frame(b'\xbf')
                os.write(fd, request)
                if not no_echo:
                    self.assertEqual(self.read_bytes(fd, len(request)), request)
                header = self.read_bytes(fd, 4)
                self.assertEqual(header[:3], b'\x80\xf0\x10')
                body = self.read_bytes(fd, header[3] + 1)
                self.assertEqual(body[0], 0xff)
                self.assertEqual(sum(header + body[:-1]) & 0xff, body[-1])
                self.assertIsNone(proc.poll())

    def test_invalid_checksum_is_ignored(self):
        _, fd = self.start_ecu('--no-echo')
        request = bytearray(frame(b'\xbf'))
        request[-1] ^= 1
        os.write(fd, request)
        ready, _, _ = select.select([fd], [], [], 0.25)
        self.assertFalse(ready)


if __name__ == '__main__':
    unittest.main()
