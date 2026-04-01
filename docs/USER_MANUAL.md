# User Manual

This manual is for running `livox_tracking_service` with 4 fixed Livox Mid-360 sensors in one room.

## 1. What The Service Does

The service:
- receives point clouds from the 4 lidars through Livox SDK2
- transforms all points into one shared room coordinate system
- removes static background if a background model is enabled
- detects moving person-like clusters
- assigns a stable track ID to each detected person
- generates a live HTML debug view
- publishes live positions over TCP JSON
- can also publish over OSC

The service does not currently provide:
- TUIO output
- a viewer UI
- perfect tracking without tuning

## 2. Files You Need

- `data/mid360_sdk_config.json`
  Livox SDK2 network configuration
- `data/config.yaml`
  app configuration, extrinsics, tracking thresholds, outputs
- `build\Release\livox_tracking_service.exe`
  the executable on Windows

## 3. Before You Start

Check these items on the Windows PC:
- the PC is on the same network as the lidars
- the lidar-facing NIC has the correct host IP
- the lidar IPs in `data/mid360_sdk_config.json` are correct
- the extrinsic matrices in `data/config.yaml` match the real room layout

For the sample config in this repo, the expected host IP is:
- `169.254.37.1`

The expected lidar IPs are:
- `169.254.37.136`
- `169.254.37.110`
- `169.254.37.118`
- `169.254.37.124`

## 4. Build

Build Livox SDK2 first, then build this repo.

Example:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DLIVOX_SDK=C:\dev\Livox-SDK2
cmake --build build --config Release
```

## 5. Validate The Config

Before a normal run, validate the config:

```powershell
build\Release\livox_tracking_service.exe --validate-config data\config.yaml
```

Expected result:
- one `[CONFIG]` line
- no error

If validation fails, fix `data/config.yaml` before continuing.

## 6. Optional: Capture Empty-Room Background

Use this only when the room is empty.

Command:

```powershell
build\Release\livox_tracking_service.exe --calibrate-background data\config.yaml
```

What happens:
- the service runs for `background.capture_seconds`
- it records static voxels seen repeatedly
- it saves the result to `background.file`

Default output file:
- `data/background.vox`

After calibration:
1. open `data/config.yaml`
2. set `background.enabled: true`
3. run the service normally

If the room furniture changes, recapture the background.

## 7. Run The Service

```powershell
build\Release\livox_tracking_service.exe data\config.yaml
```

Expected startup messages:
- `[CONFIG] ...`
- optional `[BACKGROUND] ...` if background is enabled
- `[RUN] tracking loop started`

During operation you will see `[STAT]` lines every 2 seconds.

Important fields:
- `raw`
  raw fused points collected in the cycle
- `filtered`
  points after ROI/Z filtering and downsampling
- `dynamic`
  points remaining after background suppression
- `detections`
  candidate person clusters in the current cycle
- `active_tracks`
  current tracked people

Each lidar also shows:
- `connected=yes/no`
- `packets=...`
- `points=...`

## 8. TCP Output

TCP JSON is enabled by default.

Default bind:
- `0.0.0.0:9100`

Each line is one JSON object.

Example:

```json
{"timestamp_ms":1711972800000,"people":[{"id":1,"x":1.245,"y":0.884,"z":1.126,"vx":0.031,"vy":-0.012,"vz":0.000,"width":0.542,"depth":0.491,"height":1.624,"points":48,"last_seen_ms":1711972800000}],"zones":[{"name":"entrance","occupancy":1,"ids":[1]}]}
```

Meaning:
- `id`
  track ID
- `x y z`
  position in room coordinates, meters
- `vx vy vz`
  velocity estimate
- `width depth height`
  current detected cluster size
- `points`
  points in the cluster
- `zones`
  occupancy information for configured areas

## 9. Visual Feedback

Visual feedback is provided through an auto-refreshing HTML file.

Default file:
- `debug/feedback.html`

To use it:
1. keep `debug_view.enabled: true` in `data/config.yaml`
2. start the service
3. open `debug/feedback.html` in a browser on the Windows PC

The page shows:
- dynamic points in top view
- lidar positions
- detection boxes
- current tracks with IDs
- configured zones
- lidar connection state, packets, and points

This is the easiest way to watch the system live over RustDesk.

## 10. OSC Output

OSC is disabled by default.

To enable it, edit `data/config.yaml`:
- set `osc.enabled: true`
- set `osc.target_ip`
- set `osc.port`

Each active person generates one OSC message at the configured address.

Default address:
- `/livox/person`

## 11. Zones

Zones are optional rectangular regions in room coordinates.

Example:

```yaml
zones:
  - name: entrance
    x_min: -0.5
    x_max: 0.5
    y_min: -2.0
    y_max: -1.0
```

When a tracked person centroid is inside the rectangle:
- the zone occupancy increases
- the track ID appears in that zone's `ids` list in TCP JSON

## 12. Normal Operator Workflow

Daily startup:
1. Verify the PC is on the lidar network.
2. Validate the config.
3. If needed, capture a new empty-room background.
4. Start the service.
5. Watch the first 2 or 3 `[STAT]` lines.
6. Open `debug/feedback.html`.
7. Confirm all 4 lidars show `connected=yes`.
8. Confirm packet and point counters keep increasing.
9. Confirm the TCP client receives data.

## 13. How To Stop

Press `Ctrl+C` in the terminal running the service.

Expected result:
- `[EXIT] stopping tracking service`

## 14. Troubleshooting

### No lidars connect

Check:
- Windows NIC IP
- lidar IPs in `data/mid360_sdk_config.json`
- cabling and switch
- firewall rules
- Livox SDK2 build path used in CMake

### Only some lidars connect

Check:
- the missing lidar IP
- its power and cable
- whether its IP matches both config files

### Too many ghost detections in an empty room

Try:
- capture a fresh background model
- enable background suppression
- raise `min_cluster_points`
- tighten `cluster_tolerance`
- narrow `detection_z_min` and `detection_z_max`

### The debug page does not update

Check:
- `debug_view.enabled: true`
- the service is running
- the browser has opened `debug/feedback.html`
- the file is not blocked by permissions
- the service has write access to the `debug` folder

### People disappear too early

Try:
- increase `track_timeout_ms`
- reduce `min_cluster_points`
- slightly increase `cluster_tolerance`

### Two people merge into one track

Try:
- reduce `cluster_tolerance`
- reduce `voxel_leaf`
- verify the room has enough lidar overlap

## 15. Current Limits

This is an operational first version.

Keep in mind:
- tracking quality depends on on-site tuning
- background suppression depends on a clean empty-room capture
- no TUIO output yet
- no GUI viewer yet
