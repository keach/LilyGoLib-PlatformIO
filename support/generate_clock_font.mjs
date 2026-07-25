import {spawnSync} from "node:child_process";
import {existsSync, readFileSync, rmSync, writeFileSync} from "node:fs";
import {dirname, resolve} from "node:path";
import {fileURLToPath} from "node:url";

const sourceFont = process.argv[2];
if (!sourceFont) {
    console.error(
        "Usage: node support/generate_clock_font.mjs /path/to/DSEG7Classic-Bold.ttf"
    );
    process.exit(1);
}

const scriptDirectory = dirname(fileURLToPath(import.meta.url));
const outputPath = resolve(
    scriptDirectory,
    "../src/fonts/lv_font_watch_digits_36.c"
);
const generatedPath = `${outputPath}.generated`;

const result = spawnSync(
    "npx",
    [
        "--yes",
        "lv_font_conv@1.5.3",
        "--font",
        resolve(sourceFont),
        "--size",
        "36",
        "--bpp",
        "4",
        "--format",
        "lvgl",
        "--range",
        "0x2D,0x30-0x35,0x37-0x38,0x62=>0x36,0x71=>0x39",
        "--no-compress",
        "--no-kerning",
        "--lv-font-name",
        "lv_font_watch_digits_36",
        "--output",
        generatedPath,
    ],
    {stdio: "inherit"}
);

if (result.status !== 0) {
    if (existsSync(generatedPath)) {
        rmSync(generatedPath);
    }
    process.exit(result.status ?? 1);
}

const header = `/*******************************************************************************
 * T-Watch Custom Digits, derived from DSEG7 Classic Bold by keshikan.
 * Copyright (c) 2020, keshikan (https://www.keshikan.net)
 * Modified for this project by remapping "b" to "6" and "q" to "9".
 * Licensed under the SIL Open Font License, Version 1.1.
 * See DSEG-LICENSE.txt in this directory.
 *
 * Size: 36 px
 * Bpp: 4
 * Glyphs: -0123456789
 * Compression: disabled for compatibility with the current LVGL configuration
 ******************************************************************************/
`;

let generated = readFileSync(generatedPath, "utf8");
generated = generated.replace(
    /\/\*[\s\S]*?\*\/\n\n(?=#ifdef LV_LVGL_H_INCLUDE_SIMPLE)/,
    `${header}\n`
);
generated = generated.replace('#include "lvgl/lvgl.h"', '#include "lvgl.h"');

writeFileSync(outputPath, `${generated.trimEnd()}\n`);
rmSync(generatedPath);
