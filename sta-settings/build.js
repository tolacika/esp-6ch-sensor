const fs = require("fs");
const path = require("path");
const postcss = require("postcss");
const cssnano = require("cssnano");
const htmlMinifier = require("html-minifier").minify;
const UglifyJS = require("uglify-js");

// Paths to your source files
const htmlFilePath = path.resolve(__dirname, "./src/index.html");
const cssFilePath = path.resolve(__dirname, "./src/style.css");
const jsFilePath = path.resolve(__dirname, "./src/script.js");
const outputFilePath = path.resolve(__dirname, "./dist/index.html");

// Read source files
const htmlContent = fs.readFileSync(htmlFilePath, "utf8");
const cssContent = fs.readFileSync(cssFilePath, "utf8");
const jsContent = fs.readFileSync(jsFilePath, "utf8");

// Process CSS with Tailwind and Minify
async function processCSS() {
  const result = await postcss([cssnano]).process(cssContent, {
    from: cssFilePath,
  });
  return result.css;
}

// Minify JavaScript
function minifyJS(jsCode) {
  const result = UglifyJS.minify(jsCode);
  if (result.error) {
    throw result.error;
  }
  return result.code;
}

// Inline resources into HTML
async function build() {
  try {
    const optimizedCSS = await processCSS();
    const minifiedJS = minifyJS(jsContent);

    // Inline CSS and JS into the HTML
    const inlinedHTML = htmlContent
      .replace(
        "</head>",
        `<style>${optimizedCSS}</style>\n</head>`
      )
      .replace(
        "</body>",
        `<script>${minifiedJS}</script>\n</body>`
      );

    // Minify the final HTML
    const minifiedHTML = htmlMinifier(inlinedHTML, {
      collapseWhitespace: true,
      removeComments: true,
      minifyCSS: true,
      minifyJS: true,
    });

    // Write the output
    fs.writeFileSync(outputFilePath, minifiedHTML);
    console.log(`Build successful! File saved to ${outputFilePath}`);
  } catch (error) {
    console.error("Build failed:", error);
  }
}

build();