# Physical device acceptance reports (not automated-device test results)

There are currently **zero** physical device acceptance reports. Windows and Android
hardware tasks remain unchecked until a tester supplies real hardware evidence.

Put Windows reports in `evidence/hardware/windows/<checklist-ID>.json`,
Android reports in `evidence/hardware/android/<checklist-ID>.json`. Every report
must contain these fields:

- `schema_version`: 1, `platform`: `windows` or `android`
- `commit_sha`: exact 40-character source/test SHA, `test_id`: stable checklist ID
- `result`: PASS or FAIL, `tester`: real tester, `timestamp_utc`: UTC ISO-8601
- `hardware`: Windows requires os (Windows 10/11), gpu, driver, audio_device,
  input_device. Android requires os (Android version), manufacturer, model, abi,
  api_level, gpu.
- `steps`: nonempty array of action / expected / observed / result fields
- `artifacts`: array of `{"path":"evidence/artifacts/<file>","sha256":"..."}`
  pointing to real committed files, whose SHA-256 must match.

`python tools/verify_acceptance.py` validates structure and artifact hashes.
It does **not** certify that the tester actually ran the test, nor that a
zero-report result is equivalent to Windows/Android real-device acceptance.
