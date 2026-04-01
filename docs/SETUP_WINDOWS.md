# Windows Setup

## Prerequisites
- Windows 10/11 x64
- Visual Studio 2019 or 2022 with Desktop C++
- CMake 3.16+
- Git

## 1) Build Livox SDK2
```powershell
git clone https://github.com/Livox-SDK/Livox-SDK2.git
cd Livox-SDK2
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The service CMake searches these common outputs automatically:
- `Livox-SDK2\include`
- `Livox-SDK2\build\sdk_core\Release`
- `Livox-SDK2\build\sdk_core\Debug`

## 2) Configure this repo
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DLIVOX_SDK=C:\dev\Livox-SDK2
cmake --build build --config Release
```

## 3) Edit configs
`data\mid360_sdk_config.json`
- host IP for the Windows machine NIC connected to the lidar network
- host ports for command, push, point, imu, and log traffic
- the four Mid-360 IPs in `lidar_ip`

`data\config.yaml`
- per-lidar extrinsic transforms
- tracking thresholds
- ROI limits
- background capture/load settings
- optional rectangular zones
- optional live HTML debug view
- TCP and OSC output

## 4) Validate config before going on-site
```powershell
build\Release\livox_tracking_service.exe --validate-config data\config.yaml
```

## 5) Run
```powershell
build\Release\livox_tracking_service.exe data\config.yaml
```

If `debug_view.enabled: true`, open:
- `debug\feedback.html`

This page auto-refreshes and shows:
- dynamic top-view points
- detection boxes
- track IDs and velocity direction
- zones and occupancy
- lidar connection and packet counters

## 6) Calibrate empty-room background
With nobody in the room:
```powershell
build\Release\livox_tracking_service.exe --calibrate-background data\config.yaml
```
Then set `background.enabled: true` in `data\config.yaml` before the normal run.

## Notes
- The app now depends on the official SDK2 JSON config for network routing.
- The current tracker is heuristic. Expect to tune `cluster_tolerance`, `min_cluster_points`, `person_*`, and `track_match_distance` on site.
- Zone occupancy is derived from track centroids in the shared room frame.
- TCP JSON is the safest first integration path. OSC is available now. TUIO is not implemented yet.
