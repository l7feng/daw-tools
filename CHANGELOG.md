# 变更记录

> 2026-11-09 建立。按时间倒序记录提交；版本号以 `VERSION` 为准，标签以 `git tag` 为准。建立前的逐版说明未单独保留，回溯以 `git log` 为唯一来源。

| 日期 | 提交 | 说明 |
| --- | --- | --- |
| 2026-11-05 | `36c4908` | 文件属性：补 COMPANY_NAME 为 l7feng |
| 2026-11-05 | `9f4f769` | 产品名统一为 DAW Tools 并接入程序图标 |
| 2026-11-01 | `6beb90f` | Stabilize GUI zoom gestures and isolate time selections |
| 2026-11-01 | `4b45a18` | Fix remote keyboard input and synchronize GUI edit ranges |
| 2026-11-01 | `96e177e` | Build Pro Tools-style native edit workspace |
| 2026-11-01 | `c9135b9` | Restore referenced VST3 state across cold project starts |
| 2026-11-01 | `2d334e0` | Guard freeze validity and render delivery; complete dynamic PDC and plugin workflows |
| 2026-11-01 | `9773a3c` | Guard render inputs and deadlines; preserve plugin and freeze snapshots |
| 2026-11-01 | `cd4fe04` | Add isolated background stem batches with transactional delivery |
| 2026-11-01 | `52d8042` | Verify tail and MIDI freeze; isolate shared-send bus stems |
| 2026-11-01 | `d9f0e72` | Add native track freeze with persistent verified project assets |
| 2026-11-01 | `cc72467` | Add isolated track and direct-bus stem rendering |
| 2026-11-01 | `d59f73a` | Verify plugin tail consistency and master recovery |
| 2026-11-01 | `1b2c560` | Reject unavailable plugins before export and verify state recovery |
| 2026-11-01 | `e28d8b4` | Verify MIDI controllers and transport cleanup; protect project recovery |
| 2026-11-01 | `b5e2d92` | Verify Tracktion MIDI tempo rendering and project roundtrip |
| 2026-11-01 | `478d2f9` | Validate deterministic MIDI VST3 timing without audio devices |
| 2026-11-01 | `26aa07f` | Add multi-track realtime stress and process telemetry |
| 2026-11-01 | `4e6f936` | Validate Tracktion transport on real ASIO audio callbacks |
| 2026-11-01 | `275396c` | Pin official ASIO SDK and validate native Steinberg capture |
| 2026-11-01 | `2abe1ac` | Stream capture to disk and verify crash recovery boundaries |
| 2026-11-01 | `31c9b97` | Add automated UR22C capture and discontinuity measurements |
| 2026-11-01 | `915eb3b` | Validate parallel-track PDC with deterministic VST3 delay |
| 2026-11-01 | `ddf58fb` | Verify S1 independent plugin instances and persisted bypass |
| 2026-11-01 | `79ccb62` | Verify Tracktion plugin chain with isolated backend probe |
| 2026-11-01 | `74b072a` | Improve editor layout and add desktop GUI launcher |
| 2026-11-01 | `b861fcb` | Validate deterministic VST3 gain and instance state roundtrip |
| 2026-11-01 | `4747a6f` | Add isolated VST3 scan worker and failure-tolerant supervisor |
| 2026-10-28 | `dbb94bc` | Build offline editing mixer and cancellable snapshot export |
| 2026-10-28 | `2669440` | Validate isolated native engine and offline signal roundtrip |
| 2026-10-28 | `9ccb2b5` | Establish pinned native toolchain and engine build baseline |
