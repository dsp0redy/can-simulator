# can-simulator
This Repository is for CAN simulator application

## Input format

The simulator now reads J1939 metadata from an XML file (not DBC/Excel at runtime).

Run:

```bash
./build/bin/Can-Simulator ./j1939-standard/j1939-standard.xml vcan0
```

Expected XML shape:

```xml
<j1939>
	<pgs>
		<pgn id="0x00F004" priority="6" dlc="8" source_address="0x80" cycle_ms="100" />
	</pgs>
	<sps>
		<spn id="190" pgn="0x00F004" start_bit="24" bit_size="16" scale="0.125" offset="0" min="0" max="8031.875" endianness="little" signedness="unsigned" />
	</sps>
</j1939>
```

Notes:
- Requests are still received on PGN `0x00EE00`, with requested PGN in payload bytes `[0..2]`.
- Response payload size follows XML PGN DLC and is not truncated to 8 bytes.
