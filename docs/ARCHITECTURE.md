# Architecture

## Runtime
1. `LivoxReceiver`
   Loads the official SDK2 JSON config and starts device discovery with `LivoxLidarSdkInit(...)`.
2. `OnInfoChange`
   Maps each lidar handle to the configured room sensor by serial or IP, sets `kLivoxLidarNormal`, requests Cartesian high-resolution points, and enables point sending.
3. `OnPointData`
   Converts packets into metric XYZI points and applies the lidar extrinsic matrix into the shared room frame.
4. `FilterAndDownsamplePoints`
   Applies ROI and Z filtering, then voxel-downsamples the fused room cloud.
5. `BackgroundModel`
   Optionally removes voxels learned from an empty-room calibration capture.
6. `DetectPeopleClusters`
   Runs a 2D Euclidean clustering pass over the filtered cloud and rejects clusters that do not look like a person footprint/height envelope.
7. `Tracker`
   Maintains stable IDs with nearest-neighbor association and timeout-based removal.
8. `Zones`
   Computes occupancy counts and track membership for configured rectangular regions.
9. Publishers
   Emits the active tracks over TCP JSON and optionally over OSC UDP.

## Config split
- `data/mid360_sdk_config.json`
  Livox SDK2 host/lidar network config. This is the file passed to `LivoxLidarSdkInit(...)`.
- `data/config.yaml`
  Application config. Contains per-lidar extrinsics, tracking thresholds, ROI, background model settings, zones, and output settings.

## Coordinate model
- Each lidar reports in its own sensor frame.
- `T` transforms each point into the shared room frame.
- Track positions are published directly in that shared room frame in meters.

## Current protocol model
- TCP:
  one JSON object per line with all active people and zone occupancy for the frame
- OSC:
  one `/livox/person` packet per active person

## Next rebuild steps
- add zone events and dwell time
- add TUIO mapper for normalized interactive surfaces
- add replay input for recorded packets or CSV/PCD
