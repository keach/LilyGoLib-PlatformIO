import {createHash} from "node:crypto";
import {spawnSync} from "node:child_process";
import {
    existsSync,
    mkdirSync,
    readFileSync,
    renameSync,
    rmSync,
    writeFileSync,
} from "node:fs";
import https from "node:https";
import {dirname, resolve} from "node:path";
import {fileURLToPath} from "node:url";

const scriptDirectory = dirname(fileURLToPath(import.meta.url));
const projectDirectory = resolve(scriptDirectory, "..");
const fontRevision = "f8d157532fbfaeda587e826d4cd5b21a49186f7c";
const fontUrl =
    `https://raw.githubusercontent.com/notofonts/noto-cjk/${fontRevision}` +
    "/Sans/OTF/Japanese/NotoSansCJKjp-Regular.otf";
const fontSha256 =
    "68a3fc98800b2a27b371f2fb79991daf3633bd89309d4ffaa6946fd587f375b5";
const cachedFont = resolve(
    scriptDirectory,
    ".font-cache/NotoSansCJKjp-Regular.otf"
);

function parseArguments(arguments_) {
    const options = {bpp: null, font: null};
    for (let index = 0; index < arguments_.length; index += 1) {
        const argument = arguments_[index];
        if (argument === "--bpp") {
            options.bpp = Number(arguments_[index += 1]);
        } else if (argument === "--font") {
            options.font = arguments_[index += 1];
        } else {
            throw new Error(`Unknown argument: ${argument}`);
        }
    }
    if (options.bpp !== 2 && options.bpp !== 4) {
        throw new Error("--bpp must be either 2 or 4");
    }
    return options;
}

function sha256(path) {
    return createHash("sha256").update(readFileSync(path)).digest("hex");
}

function download(url, destination) {
    return new Promise((resolvePromise, reject) => {
        const request = https.get(url, (response) => {
            if (response.statusCode >= 300 && response.statusCode < 400 &&
                response.headers.location) {
                response.resume();
                download(response.headers.location, destination)
                    .then(resolvePromise, reject);
                return;
            }
            if (response.statusCode !== 200) {
                response.resume();
                reject(new Error(`Font download failed: HTTP ${response.statusCode}`));
                return;
            }
            const temporary = `${destination}.download`;
            const chunks = [];
            response.on("data", (chunk) => chunks.push(chunk));
            response.on("end", () => {
                writeFileSync(temporary, Buffer.concat(chunks));
                renameSync(temporary, destination);
                resolvePromise();
            });
            response.on("error", reject);
        });
        request.on("error", reject);
    });
}

async function resolveFont(explicitFont) {
    if (explicitFont) {
        const resolved = resolve(explicitFont);
        if (!existsSync(resolved)) {
            throw new Error(`Font file does not exist: ${resolved}`);
        }
        if (sha256(resolved) !== fontSha256) {
            throw new Error("Local font SHA-256 does not match the pinned source");
        }
        return resolved;
    }
    mkdirSync(dirname(cachedFont), {recursive: true});
    if (!existsSync(cachedFont) || sha256(cachedFont) !== fontSha256) {
        console.log(`Downloading Noto Sans CJK JP Regular from ${fontRevision}`);
        await download(fontUrl, cachedFont);
    }
    if (sha256(cachedFont) !== fontSha256) {
        rmSync(cachedFont, {force: true});
        throw new Error("Downloaded font SHA-256 does not match the pinned source");
    }
    return cachedFont;
}

function readCharacters() {
    const rangesPath = resolve(
        scriptDirectory, "character_sets/japanese_unicode_ranges.txt"
    );
    const jisPath = resolve(
        scriptDirectory, "character_sets/jis_x_0208_level1.txt"
    );
    const characters = [];
    for (const line of readFileSync(rangesPath, "utf8").split(/\r?\n/u)) {
        const value = line.replace(/#.*/u, "").trim();
        if (!value) continue;
        const [startText, endText = startText] = value.split("-");
        const start = Number.parseInt(startText, 16);
        const end = Number.parseInt(endText, 16);
        for (let codePoint = start; codePoint <= end; codePoint += 1) {
            characters.push(String.fromCodePoint(codePoint));
        }
    }
    const jisCharacters = readFileSync(jisPath, "utf8").replace(/\s/gu, "");
    characters.push(...jisCharacters);
    return [...new Set(characters)].join("");
}

const options = parseArguments(process.argv.slice(2));
const sourceFont = await resolveFont(options.font);
const symbols = readCharacters();
const fontName = `lv_font_japanese_16_${options.bpp}bpp`;
const outputPath = resolve(projectDirectory, `src/fonts/${fontName}.c`);
const generatedPath = `${outputPath}.generated`;

const result = spawnSync(
    "npx",
    [
        "--yes", "lv_font_conv@1.5.3",
        "--font", sourceFont,
        "--size", "16",
        "--bpp", String(options.bpp),
        "--format", "lvgl",
        "--symbols", symbols,
        "--no-compress",
        "--no-kerning",
        "--lv-font-name", fontName,
        "--output", generatedPath,
    ],
    {stdio: "inherit"}
);

if (result.status !== 0) {
    rmSync(generatedPath, {force: true});
    process.exit(result.status ?? 1);
}

const header = `/*******************************************************************************
 * Noto Sans CJK JP Regular, subset for the T-Watch S3 Japanese UI.
 * Copyright 2014-2021 Adobe (http://www.adobe.com/).
 * Licensed under the SIL Open Font License, Version 1.1.
 * See NOTO-SANS-JP-LICENSE.txt in this directory.
 *
 * Source revision: ${fontRevision}
 * Source SHA-256: ${fontSha256}
 * Size: 16 px
 * Bpp: ${options.bpp}
 * Character set: ASCII, Japanese punctuation, Hiragana, Katakana,
 *                fullwidth forms, and JIS X 0208 level-1 kanji
 * Compression: disabled for compatibility with the current LVGL configuration
 ******************************************************************************/`;

let generated = readFileSync(generatedPath, "utf8");
generated = generated.replace(
    /\/\*[\s\S]*?\*\/(?=\n\n#ifdef LV_LVGL_H_INCLUDE_SIMPLE)/u,
    header
);
generated = generated.replace('#include "lvgl/lvgl.h"', '#include "lvgl.h"');
writeFileSync(outputPath, `${generated.trimEnd()}\n`);
rmSync(generatedPath);

console.log(`Generated ${outputPath}`);
console.log(`Requested ${[...symbols].length} unique characters at ${options.bpp} bpp`);
