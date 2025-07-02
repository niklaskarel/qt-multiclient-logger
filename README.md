# Qt Multi-Client TCP Logger

This ongoing project is a Qt-based application that functions as a TCP server designed to handle up to three concurrent clients (modules), each sending structured JSON messages over TCP. It features real-time message logging, GUI-based controls, error-handling mechanisms tailored for critical communication systems or monitoring tools and Writer that currently writes to a single session log file, with module identifiers included in each entry for traceability; advanced options are part of the ongoing development roadmap. It also displaysa basi graph using QCustomPlot of the incoming data points after some basic thresholding and filtering.

## Features

- Supports up to 3 TCP clients simultaneously
- Parses JSON messages with `clientId`, `type`, `message`, and `timestamp`
- GUI with real-time log view, control buttons for stopping individual modules and graph that shows sensor data for each client using a moving average
- Settings dialog to configure: Port number, IP address, Plotting window duration, Thresholds for incoming sensor values, Flush interval and buffer size
- Graceful disconnection and cleanup on critical failures
- Python script included to simulate real client traffic with randomized message generation.
- Logger system with bounded message queue and severity-based message discarding
- Writer that writes the log messages to separate txt files in a predefined directory (ongoing)

## UI Preview

This is a real-time log viewer showing messages from three simulated modules over TCP.
The GUI reacts to message severity—buttons are automatically disabled after CRITICAL messages.

![Screenshot of the main window of the Qt Logger GUI](Main_window.png)
![Screenshot of the settings dialog for connection settings](Settings_dialog_connection_settings.png)
![Screenshot of the settings dialog for data settings](Settings_dialog_data_settings.png)
![Screenshot of the settings dialog for logger settings](Settings_dialog_logger_settings.png)

## Message Format (JSON)
```json
{
  "client": 1,
  "type": "INFO", // INFO, WARNING, ERROR, CRITICAL, DATA
  "message": "Module started",
  "timestamp": "2025-05-06 06:29:51"
}
```

## Critical Behavior Logic
- If **module 3** sends a CRITICAL message → all modules are disconnected and the application stops.
- If **module 1 or 2** sends CRITICAL message → only that module is stopped.
- If **module 3** sends a ERROR message → all three stop Module buttons are activated and the user can manually stop the modules.
- If **module 1 or 2** sends ERROR message → only that module"s stop button is activated.

## Build Instructions
1. Install Qt 6.8.3 and CMake 3.28+
2. `cmake -S . -B build`
3. `cmake --build build`
4. `cd build`
5. run the executable

Alternatively this procedure can be dont from the Qt Creator

## Code Highlights
- `EventReceiver` inherits from `QTcpServer` and tracks client sockets in a `QMap`
- Uses `peek()` to non-destructively validate clientId before attaching handlers
- Ensures robust signal-safe disconnection via `deleteLater()` and deferred read triggering
- Fully event-driven, minimal blocking (500ms max on connection handshake)

## License
MIT License (or specify yours)

## Known issues

~~As of 13/6/2025 The application crashes when the python script generates a critical error for module 3~~
Fixed on 17/6/2025: Crash caused by improper deletion of client sockets on CRITICAL messages from module 3 was resolved.
18/6/2025 Python proccess crashed when closing after a critical error in module 3.
          INFO, WARNING, ERROR messages are not displayex after a few seconds on the logger, only the DATA messages.

---

**Author:** Nikolaos Laskarelias

Feel free to adapt, extend, or integrate this logger with your own backend or UI requirements.

### TODO

## Technical Improvements
- Explore alternative communication protocols between frontend and backend, including:
  - **WebSocket:** for real-time bidirectional message streaming (client ↔ server)
  - **REST API:** for structured control, querying, and persistent log submission
  - This includes manual JSON serialization/parsing and potential integration with PostgreSQL for durable log storage.
- ~~Refactor EventReceiver to use QThread per client socket to improve stability and avoid crashes due to cross-thread access or unexpected deletions. Current design using QTcpServer in the main thread is fragile under high traffic~~ Done in 21.06.2025
- Refactor Logger to run in its own thread to handle high-throughput message processing.
- Current architecture is tightly coupled: MainWindow handles transport and control logic. Refactoring planned to introduce an EventOrchestrator for better separation of concerns (UI vs. logic vs. transport).
- Add QCustomPlot as a dynamic library and not as a static as it is increasing the time for building.
- Add support for **MQTT** as a communication method between system components, using a publish/subscribe architecture via an MQTT broker
- Add second way of messaging formating (f.e **XML**, **Protobuf**)
- Add a settings panel to adjust:
  - Give the user the possibility to chose a communication method between TCPSocket, WebSocket or REST
  - Give the user the possibility to chose message formating


## Functional Extensions
- Support WebSocket or UDP modes for remote message streaming
- Graphical message statistics (e.g., number of errors)
- Authentication/token for sender script (basic security)
- Use JSON schema validation for incoming messages

## Testing
- Add unit tests for `Logger` and `Writer `class
- Stress-test script: burst of messages at 5ms intervals

## Deployment
- Bundle release with standalone Python
- Add `.bat` or `.sh` launcher script for convenience
