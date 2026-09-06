#!/usr/bin/env python3
"""Decode RTOS sensor-node telemetry to JSON or CSV.

No third-party packages are needed for file/stdin decoding. Install pyserial
only when using --port with a live board.
"""

from __future__ import annotations

import argparse
import csv
import json
import struct
import sys
from dataclasses import asdict, dataclass
from typing import BinaryIO, Iterable, Iterator

SOF = 0x7E
ESC = 0x7D
VERSION = 1
STATE_TYPE = 1
STATE_PAYLOAD_LENGTH = 44
RAW_LENGTH = 52
STATE_STRUCT = struct.Struct("<BBHHIIiiiiiiIIIH")


class TelemetryError(ValueError):
    """Base class for rejected telemetry bodies."""


class FormatError(TelemetryError):
    """The frame shape, version, type, or reserved fields are invalid."""


class CRCError(TelemetryError):
    """The frame checksum does not match its body."""


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


@dataclass(frozen=True)
class State:
    sequence: int
    timestamp_us: int
    status: int
    roll_rad: float
    pitch_rad: float
    yaw_rad: float
    altitude_m: float
    vertical_speed_mps: float
    temperature_c: float


def decode_raw(raw: bytes) -> State:
    if len(raw) != RAW_LENGTH:
        raise FormatError(f"wrong raw length: {len(raw)}")
    fields = STATE_STRUCT.unpack(raw)
    if crc16(raw[:-2]) != fields[-1]:
        raise CRCError("CRC mismatch")
    version, packet_type, payload_length, sequence = fields[:4]
    if version != VERSION or packet_type != STATE_TYPE:
        raise FormatError(f"unsupported packet v{version} type {packet_type}")
    if payload_length != STATE_PAYLOAD_LENGTH:
        raise FormatError(f"wrong payload length: {payload_length}")
    if any(fields[index] != 0 for index in (12, 13, 14)):
        raise FormatError("reserved fields must be zero")
    return State(
        sequence=sequence,
        timestamp_us=fields[4],
        status=fields[5],
        roll_rad=fields[6] / 1e6,
        pitch_rad=fields[7] / 1e6,
        yaw_rad=fields[8] / 1e6,
        altitude_m=fields[9] / 1e3,
        vertical_speed_mps=fields[10] / 1e3,
        temperature_c=fields[11] / 1e3,
    )


class FrameDecoder:
    def __init__(self) -> None:
        self.buffer = bytearray()
        self.escaped = False
        self.dropping = False
        self.errors = 0
        self.crc_errors = 0
        self.format_errors = 0
        self.overflow_errors = 0
        self.escape_errors = 0
        self.resync_events = 0
        self.frames = 0
        self.lost = 0
        self.last_sequence: int | None = None

    def feed(self, chunk: bytes) -> Iterator[State]:
        for byte in chunk:
            if byte == SOF:
                if self.escaped:
                    self.escaped = False
                    self.buffer.clear()
                    self.errors += 1
                    self.escape_errors += 1
                    self.resync_events += 1
                    continue
                if self.dropping:
                    self.dropping = False
                    self.buffer.clear()
                    self.resync_events += 1
                    continue
                if self.buffer:
                    try:
                        state = decode_raw(bytes(self.buffer))
                    except CRCError:
                        self.errors += 1
                        self.crc_errors += 1
                        self.resync_events += 1
                    except FormatError:
                        self.errors += 1
                        self.format_errors += 1
                        self.resync_events += 1
                    else:
                        if self.last_sequence is not None:
                            expected = (self.last_sequence + 1) & 0xFFFF
                            self.lost += (state.sequence - expected) & 0xFFFF
                        self.last_sequence = state.sequence
                        self.frames += 1
                        yield state
                self.buffer.clear()
                self.escaped = False
            elif self.escaped:
                if byte not in (SOF ^ 0x20, ESC ^ 0x20):
                    self.buffer.clear()
                    self.escaped = False
                    self.dropping = True
                    self.errors += 1
                    self.escape_errors += 1
                    continue
                self.buffer.append(byte ^ 0x20)
                self.escaped = False
            elif byte == ESC:
                self.escaped = True
            elif len(self.buffer) < RAW_LENGTH:
                self.buffer.append(byte)
            else:
                self.buffer.clear()
                self.errors += 1
                self.overflow_errors += 1
                self.dropping = True


def escape(raw: bytes) -> bytes:
    output = bytearray([SOF])
    for byte in raw:
        if byte in (SOF, ESC):
            output.extend((ESC, byte ^ 0x20))
        else:
            output.append(byte)
    output.append(SOF)
    return bytes(output)


def make_test_frame(sequence: int = 7) -> bytes:
    values = [VERSION, STATE_TYPE, STATE_PAYLOAD_LENGTH, sequence, 123456, 0x0F,
              100000, -200000, 300000, 12345, -250, 25125, 0, 0, 0, 0]
    raw_without_crc = struct.pack("<BBHHIIiiiiiiIII", *values[:-1])
    raw = raw_without_crc + struct.pack("<H", crc16(raw_without_crc))
    return escape(raw)


def chunks(stream: BinaryIO, size: int = 256) -> Iterator[bytes]:
    while data := stream.read(size):
        yield data


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--input", type=argparse.FileType("rb"), help="captured binary stream")
    source.add_argument("--port", help="serial port, for example COM5")
    parser.add_argument("--baud", type=int, default=921600)
    parser.add_argument("--csv", action="store_true", help="emit CSV instead of JSON lines")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)

    if args.self_test:
        assert crc16(b"123456789") == 0x29B1
        decoded = list(FrameDecoder().feed(make_test_frame()))
        assert len(decoded) == 1 and decoded[0].temperature_c == 25.125
        print("telemetry self-test: PASS")
        return 0

    stream: BinaryIO
    if args.port:
        try:
            import serial  # type: ignore
        except ImportError:
            parser.error("--port requires pyserial: python -m pip install pyserial")
        stream = serial.Serial(args.port, args.baud, timeout=1)
    else:
        stream = args.input or sys.stdin.buffer

    decoder = FrameDecoder()
    writer = None
    for chunk in chunks(stream):
        for state in decoder.feed(chunk):
            record = asdict(state)
            if args.csv:
                writer = writer or csv.DictWriter(sys.stdout, fieldnames=record.keys())
                if decoder.frames == 1:
                    writer.writeheader()
                writer.writerow(record)
            else:
                print(json.dumps(record, separators=(",", ":")))
    print(
        f"frames={decoder.frames} errors={decoder.errors} "
        f"crc={decoder.crc_errors} format={decoder.format_errors} "
        f"overflow={decoder.overflow_errors} escape={decoder.escape_errors} "
        f"resync={decoder.resync_events} lost={decoder.lost}",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
