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
 * Modified for this project by remapping "b" to "6" and "q" to "9",
 * and by drawing "7" with the top, upper-right, and lower-right segments only.
 * Licensed under the SIL Open Font License, Version 1.1.
 * See DSEG-LICENSE.txt in this directory.
 *
 * Size: 36 px
 * Bpp: 4
 * Glyphs: -0123456789
 * Compression: disabled for compatibility with the current LVGL configuration
 ******************************************************************************/
`;

function createSevenGlyphBitmap() {
    const width = 23;
    const height = 36;
    const pixels = new Uint8Array(width * height);

    const setPixel = (x, y, value = 0x0f) => {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            pixels[y * width + x] = Math.max(pixels[y * width + x], value);
        }
    };

    // Top segment with short bevelled ends.
    for (let y = 0; y <= 4; y++) {
        const inset = Math.max(0, 2 - y);
        for (let x = 2 + inset; x <= 20 - inset; x++) {
            setPixel(x, y);
        }
    }

    // Upper-right and lower-right segments. Keep a small center gap so the
    // glyph reads as three independent seven-segment bars.
    for (let y = 3; y <= 16; y++) {
        const inset = y <= 4 || y >= 15 ? 1 : 0;
        for (let x = 18 + inset; x <= 22 - inset; x++) {
            setPixel(x, y);
        }
    }
    for (let y = 19; y <= 34; y++) {
        const inset = y <= 20 || y >= 33 ? 1 : 0;
        for (let x = 18 + inset; x <= 22 - inset; x++) {
            setPixel(x, y);
        }
    }

    const bytes = [];
    for (let index = 0; index < pixels.length; index += 2) {
        const high = pixels[index];
        const low = index + 1 < pixels.length ? pixels[index + 1] : 0;
        bytes.push((high << 4) | low);
    }
    return bytes;
}

function replaceSevenGlyphBitmap(source) {
    const bitmapMatch = source.match(
        /(static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap\[\] = \{)([\s\S]*?)(\n\};)/
    );
    if (!bitmapMatch) {
        throw new Error("glyph_bitmap array was not found");
    }

    const descriptors = [...source.matchAll(/\.bitmap_index = (\d+)/g)].map(
        (match) => Number(match[1])
    );
    const sevenGlyphId = 8; // reserved, '-', '0' ... '7'
    const start = descriptors[sevenGlyphId];
    const end = descriptors[sevenGlyphId + 1];
    if (!Number.isInteger(start) || !Number.isInteger(end) || end <= start) {
        throw new Error("bitmap indexes for glyph '7' were not found");
    }

    const bitmapValues = [...bitmapMatch[2].matchAll(/0x[0-9a-f]+/gi)].map(
        (match) => Number.parseInt(match[0], 16)
    );
    const replacement = createSevenGlyphBitmap();
    if (replacement.length !== end - start) {
        throw new Error(
            `custom '7' bitmap is ${replacement.length} bytes; expected ${end - start}`
        );
    }
    bitmapValues.splice(start, replacement.length, ...replacement);

    const lines = [];
    for (let index = 0; index < bitmapValues.length; index += 8) {
        const values = bitmapValues
            .slice(index, index + 8)
            .map((value) => `0x${value.toString(16)}`)
            .join(", ");
        lines.push(`    ${values},`);
    }

    return source.replace(
        bitmapMatch[0],
        `${bitmapMatch[1]}\n${lines.join("\n")}\n${bitmapMatch[3]}`
    );
}

let generated = readFileSync(generatedPath, "utf8");
generated = replaceSevenGlyphBitmap(generated);
generated = generated.replace(
    /\/\*[\s\S]*?\*\/\n\n(?=#ifdef LV_LVGL_H_INCLUDE_SIMPLE)/,
    `${header}\n`
);
generated = generated.replace('#include "lvgl/lvgl.h"', '#include "lvgl.h"');

writeFileSync(outputPath, `${generated.trimEnd()}\n`);
rmSync(generatedPath);
