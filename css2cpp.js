/*
  Script to take a CSS file, minify it and create C string to be placed in Program Memory
*/

const fs = require('fs')
const path = require('path')
const csso = require('csso')

function escapeString (input) {
  const source = typeof input === 'string' || input instanceof String ? input : ''
  return source.replace(/"/g, '\\"')
}

const inputFilename = process.argv[2] || 'style.css'
const shortName = path.basename(inputFilename).replace(/\W+/, '_').toLowerCase()
const outputFilename = process.argv[3] || inputFilename.replace(/\.css$/, '_css') + '.cpp'
console.log('Input file: ' + inputFilename)
console.log('Output file: ' + outputFilename)

const cssData = fs.readFileSync(inputFilename)
const minifiedCss = csso.minify(cssData).css

let output = `// This file was generated by css2cpp.js from ${inputFilename}\n`
output += '#include <avr/pgmspace.h>\n'
output += '\n'
output += `const PROGMEM char pm_${shortName}[] = "${escapeString(minifiedCss)}";\n`

fs.writeFile(outputFilename, output, function (err) {
  if (err) return console.log(err)
  console.log('Done.')
})