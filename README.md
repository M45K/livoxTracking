# livox_tracking_service

Windows-oriented Livox Mid-360 tracking service for fixed multi-lidar rooms.

Current rebuild scope:
- ingest point clouds from Livox SDK2 using the official SDK JSON config
- apply per-lidar extrinsics into one shared room frame
- calibrate and load an empty-room background model
- downsample and cluster candidate person blobs
- maintain simple track IDs over time
- compute zone occupancy from active tracks
- generate a live HTML debug view for visual feedback
- publish live positions over TCP JSON
- publish live positions over OSC UDP

The old OpenCV top-view viewer path is no longer the primary runtime.

## Files
- `data/mid360_sdk_config.json` – official Livox SDK2 network config
- `data/config.yaml` – app-level tracking, output, and extrinsic config
- `src/livox_receiver.*` – SDK2 ingest and handle/IP mapping
- `src/point_processing.*` – filtering, voxel downsample, clustering
- `src/tracker.*` – track assignment and lifecycle
- `src/tcp_json_publisher.*` – TCP line-delimited JSON output
- `src/osc_publisher.*` – OSC UDP output

## Build
Windows with Livox SDK2:
1. Build Livox-SDK2 with Visual Studio.
2. Configure this repo with `-DLIVOX_SDK=...`.
3. Build `livox_tracking_service`.

Minimal local compile without hardware/SDK:
```bash
cmake -S . -B build -DUSE_LIVOX=OFF
cmake --build build
./build/livox_tracking_service --validate-config data/config.yaml
```

## Run
```powershell
build\Release\livox_tracking_service.exe data\config.yaml
```

Empty-room background calibration:
```powershell
build\Release\livox_tracking_service.exe --calibrate-background data\config.yaml
```
This saves the static voxel map configured by `background.file`. Run it only when the room is empty.

Visual feedback:
- by default the service updates `debug/feedback.html`
- open that file on the Windows machine in a browser during the run
- it shows the top view, zones, tracks, detections, and lidar status

TCP output is newline-delimited JSON:
```json
{"timestamp_ms":1711972800000,"people":[{"id":1,"x":1.245,"y":0.884,"z":1.126,"vx":0.031,"vy":-0.012,"vz":0.000,"width":0.542,"depth":0.491,"height":1.624,"points":48,"last_seen_ms":1711972800000}],"zones":[{"name":"entrance","occupancy":1,"ids":[1]}]}
```

OSC output sends one `/livox/person` message per active track with:
- `int32 id`
- `float x`
- `float y`
- `float z`
- `float vx`
- `float vy`
- `int32 points`
- `int32 frame_timestamp_ms`
- `int32 last_seen_ms`

## Current limitations
- no TUIO publisher yet
- no UI viewer in the new path
- clustering and tracking are heuristic and need on-site tuning

## Docs
- `docs/SETUP_WINDOWS.md`
- `docs/ARCHITECTURE.md`
- `docs/USER_MANUAL.md`
