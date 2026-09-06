# Binary telemetry protocol v1

Transport framing is `7E <escaped body> 7E`. Within the body, bytes `7E` and
`7D` become `7D 5E` and `7D 5D`, respectively. A receiver discards oversized,
short, unknown-version, and bad-CRC frames, then resynchronizes at the next
`7E`.

The CRC is CRC-16/CCITT-FALSE: polynomial `0x1021`, initial value `0xFFFF`, no
reflection, and no final XOR. It covers the header and payload, not delimiters
or escaping. The check value for ASCII `123456789` is `0x29B1`.

## Header (6 bytes)

| Offset | Type | Meaning |
|---:|---|---|
| 0 | u8 | protocol version (`1`) |
| 1 | u8 | packet type (`1` = fused state) |
| 2 | u16 | payload length (`44`) |
| 4 | u16 | wrapping sequence number |

## Fused-state payload (44 bytes)

| Offset | Type | Unit / meaning |
|---:|---|---|
| 6 | u32 | microseconds since boot, wraps naturally |
| 10 | u32 | status bitmap from `sensor_types.h` |
| 14 | i32 | roll, microradians |
| 18 | i32 | pitch, microradians |
| 22 | i32 | yaw, microradians |
| 26 | i32 | altitude, millimetres |
| 30 | i32 | vertical speed, millimetres/second |
| 34 | i32 | temperature, millidegrees Celsius |
| 38 | 3 × u32 | reserved, must be zero |
| 50 | u16 | CRC-16 |

The full unescaped frame body is 52 bytes. At 50 Hz, even worst-case escaping
uses under 6% of a 921600 baud 8-N-1 link.

The firmware decoder keeps separate counters for valid frames, CRC errors,
format errors, oversized bodies, invalid/dangling escapes, resynchronizations,
and missing sequence numbers. Invalid input enters a bounded drop state and is
ignored until the next `0x7E`; no corrupted body can grow the buffer beyond 52
bytes or contaminate the following valid frame.
