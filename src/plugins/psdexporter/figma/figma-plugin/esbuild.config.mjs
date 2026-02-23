import * as esbuild from "esbuild";
import * as fs from "fs";
import * as path from "path";

// Build main.ts (plugin sandbox)
const mainBuild = esbuild.build({
  entryPoints: ["src/main.ts"],
  bundle: true,
  outfile: "dist/main.js",
  format: "iife",
  target: "es2017",
});

// Build ui.ts and inline into ui.html
const uiBuild = esbuild
  .build({
    entryPoints: ["src/ui.ts"],
    bundle: true,
    outfile: "dist/ui.js",
    format: "iife",
    target: "es2017",
  })
  .then(() => {
    const html = fs.readFileSync("src/ui.html", "utf8");
    const js = fs.readFileSync("dist/ui.js", "utf8");
    const inlined = html.replace(
      "<!-- INLINE_SCRIPT -->",
      `<script>${js}</script>`
    );
    fs.mkdirSync("dist", { recursive: true });
    fs.writeFileSync("dist/ui.html", inlined);
    // Clean up intermediate js
    fs.unlinkSync("dist/ui.js");
  });

await Promise.all([mainBuild, uiBuild]);
console.log("Build complete.");
