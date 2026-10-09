# Independent tex3ds fixtures

These small binary containers and PNG previews were produced by tex3ds v2.3.0 from a 16-by-16 colored alpha pattern. Each of the fourteen PICA encodings has containers generated with `none`, `lz10`, `lz11`, `rle`, `huff`, and `auto` compression. The PNG for each format is the tool's independent decoded preview. `rgba8-mips.t3x` includes a mip chain.

The fixtures are checked in so normal tests require neither Docker nor tex3ds. Import tests allow a one-byte channel difference for normalized versus hardware bit-replication expansion.

Generation uses `tex3ds -f <format> -z <compression> -o <format>-<compression>.t3x -p <format>.png external-source.png`. The mip fixture adds `-m box`. The source-pattern writer and reproducible generation scripts are in the test class and workspace validation artifacts, respectively.

Reference tool: [devkitPro/tex3ds](https://github.com/devkitPro/tex3ds).
