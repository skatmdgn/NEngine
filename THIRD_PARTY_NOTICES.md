# Third-Party Notices

NEngine includes third-party components listed below. This file records the version and license option used by NEngine. The complete upstream license text is retained beside each vendored dependency.

## stb_image

- Project: stb
- Upstream: nothings/stb
- Pinned commit: `2c980bb59875b0d32144a71867fbdebb2f77cd20`
- Component: `stb_image.h`
- Copyright: Copyright (c) 2017 Sean Barrett
- License used by NEngine: MIT License alternative
- Vendored license text: `third_party/stb/LICENSE`

NEngine uses stb_image for PNG and JPEG CPU image decoding. The dependency is compiled into NEngineRender and does not require a separate runtime library.
