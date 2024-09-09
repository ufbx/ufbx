const fs = require("fs").promises
const path = require("path")
const { parseArgs } = require("node:util")

const options = {
    wasm: {
        type: "string",
        default: "build/ufbx_hasher.wasm",
    },
    root: {
        type: "string",
        default: "data",
    },
    verbose: {
        type: "boolean",
    },
}

const { values: args } = parseArgs({ options })

async function main() {
    const wasmData = await fs.readFile(args.wasm)

    const memory = new WebAssembly.Memory({
        initial: 40,
        maximum: 1000,
    })

    function mem() { return wasmModule.instance.exports.memory.buffer }

    const wasmModule = await WebAssembly.instantiate(wasmData, {
        js: { mem: memory },
        host: {
            hostVerbose: (ptr, length) => {
                if (!args.verbose) return
                const data8 = new Uint8Array(wasmModule.instance.exports.memory.buffer)
                const strData = data8.slice(ptr, ptr + length)
                const str = new TextDecoder().decode(strData)
                console.log(str.trimEnd())
            },
            hostError: (ptr, length) => {
                const data8 = new Uint8Array(wasmModule.instance.exports.memory.buffer)
                const strData = data8.slice(ptr, ptr + length)
                const str = new TextDecoder().decode(strData)
                console.error(str.trimEnd())
            },
            hostExit: (code) => {
                process.exit(code)
            },
        },
    })

    function hashAlloc(size) {
        return wasmModule.instance.exports.hashAlloc(size)
    }

    function hashFree(ptr) {
        wasmModule.instance.exports.hashFree(ptr)
    }

    async function hashScene(filename) {
        const data = await fs.readFile(path.join(args.root, filename))

        const resultPtr = hashAlloc(2)
        const dataPtr = hashAlloc(data.length)

        const dataSlice = new Uint8Array(mem(), dataPtr, data.length)
        dataSlice.set(data)

        new DataView(mem()).setBigUint64(resultPtr, 0n, true)

        wasmModule.instance.exports.hashScene(resultPtr, dataPtr, data.length)

        const hash = new DataView(mem()).getBigUint64(resultPtr, true)
        console.log(`${filename}: ${hash}`)

        hashFree(dataPtr)
        hashFree(resultPtr)
    }

    const badFiles = new Set([
        "synthetic_id_collision_7500_ascii.fbx",
    ])

    const files = await fs.readdir(args.root)
    for (const file of files) {
        if (!file.endsWith(".fbx")) continue
        if (file.includes("_fail_")) continue
        if (badFiles.has(file)) continue

        await hashScene(file)
    }
}

main()
