# Third-party code

NickelTiles builds on these projects. All are under the MIT license; their
notices are kept here as the license requires.

| Project | What NickelTiles uses |
| --- | --- |
| [NickelHook](https://github.com/pgaskin/NickelHook) by Patrick Gaskin | Compiled into the library: loading into Nickel, hooking, the failsafe and uninstall handling. Included as a git submodule with its own license. |
| [NickelHome](https://github.com/nicoverbruggen/NickelHome) by Nico Verbruggen | The way Kobo's home rows are found and hidden (`src/ui/home_rows.cc`) and the structure of the config parser (`src/core/config.c`) follow NickelHome. |
| [NickelMenu](https://github.com/pgaskin/NickelMenu) by Patrick Gaskin | NickelTiles reads NickelMenu's config files and runs their actions the way NickelMenu does. No code is copied. |

## NickelHook and NickelMenu

```
Copyright (c) 2020-2025 Patrick Gaskin
```

## NickelHome

```
Copyright (c) 2020-2025 Patrick Gaskin (NickelMenu, NickelHook)
Copyright (c) 2026 Nico Verbruggen (NickelHome)
```

## License text (the same for all of the above)

```
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```
