import pathlib
import struct
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import telemetry


class TelemetryTests(unittest.TestCase):
    def test_standard_crc_vector(self):
        self.assertEqual(telemetry.crc16(b"123456789"), 0x29B1)

    def test_round_trip_and_scaling(self):
        frame = telemetry.make_test_frame(sequence=65535)
        states = list(telemetry.FrameDecoder().feed(frame))
        self.assertEqual(len(states), 1)
        self.assertEqual(states[0].sequence, 65535)
        self.assertAlmostEqual(states[0].roll_rad, 0.1)
        self.assertAlmostEqual(states[0].altitude_m, 12.345)
        self.assertAlmostEqual(states[0].temperature_c, 25.125)

    def test_escape_reserved_bytes(self):
        raw = bytes([telemetry.SOF, telemetry.ESC, 0x00])
        self.assertEqual(telemetry.escape(raw), bytes([0x7E, 0x7D, 0x5E, 0x7D, 0x5D, 0, 0x7E]))

    def test_crc_corruption_is_rejected(self):
        frame = bytearray(telemetry.make_test_frame())
        frame[10] ^= 1
        decoder = telemetry.FrameDecoder()
        self.assertEqual(list(decoder.feed(bytes(frame))), [])
        self.assertEqual(decoder.errors, 1)

    def test_stream_chunks_and_sequence_loss(self):
        stream = telemetry.make_test_frame(10) + telemetry.make_test_frame(13)
        decoder = telemetry.FrameDecoder()
        states = []
        for offset in range(0, len(stream), 3):
            states.extend(decoder.feed(stream[offset:offset + 3]))
        self.assertEqual([s.sequence for s in states], [10, 13])
        self.assertEqual(decoder.lost, 2)

    def test_wrong_length_is_rejected(self):
        with self.assertRaises(telemetry.FormatError):
            telemetry.decode_raw(bytes(12))

    def test_truncated_frame_resynchronizes(self):
        decoder = telemetry.FrameDecoder()
        truncated = telemetry.make_test_frame(20)[:12] + bytes([telemetry.SOF])
        states = list(decoder.feed(truncated + telemetry.make_test_frame(21)))
        self.assertEqual([state.sequence for state in states], [21])
        self.assertEqual(decoder.format_errors, 1)
        self.assertEqual(decoder.resync_events, 1)

    def test_invalid_escape_drops_until_delimiter(self):
        decoder = telemetry.FrameDecoder()
        invalid = bytes([telemetry.SOF, telemetry.ESC, 0x00, 0x11,
                         telemetry.SOF])
        states = list(decoder.feed(invalid + telemetry.make_test_frame(5)))
        self.assertEqual([state.sequence for state in states], [5])
        self.assertEqual(decoder.escape_errors, 1)
        self.assertEqual(decoder.resync_events, 1)

    def test_oversized_garbage_recovers(self):
        decoder = telemetry.FrameDecoder()
        garbage = bytes([telemetry.SOF]) + bytes([0x55]) * 80 + bytes([telemetry.SOF])
        states = list(decoder.feed(garbage + telemetry.make_test_frame(8)))
        self.assertEqual([state.sequence for state in states], [8])
        self.assertEqual(decoder.overflow_errors, 1)
        self.assertEqual(decoder.resync_events, 1)

    def test_dangling_escape_is_rejected(self):
        decoder = telemetry.FrameDecoder()
        stream = bytes([telemetry.SOF, 0x01, telemetry.ESC, telemetry.SOF])
        self.assertEqual(list(decoder.feed(stream)), [])
        self.assertEqual(decoder.escape_errors, 1)


if __name__ == "__main__":
    unittest.main()
