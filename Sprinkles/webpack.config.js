const path = require("path");
const webpack = require("webpack");

const isProd = process.env.NODE_ENV === "production";

module.exports = {
    entry: "./src/main.ts",
    devtool: "source-map",
    mode: isProd ? "production" : "development",
    module: {
        rules: [
            {
                test: /\.tsx?$/,
                use: "ts-loader",
                exclude: /node_modules/,
            },
            {
                test: /\.css$/i,
                loader: "css-loader",
                options: {
                    sourceMap: false,
                    exportType: "string",
                    modules: false
                }
            }
        ]
    },
    plugins: [
        new webpack.DefinePlugin({
            PRODUCTION: isProd,
            VERSION: process.env["VERSION_INFO"]
        })
    ],
    optimization: {
        chunkIds: "named",
        moduleIds: "named",
        minimize: false,//isProd,
        mangleExports: false
    },
    experiments: {
        topLevelAwait: true
    },
    resolve: {
        extensions: [".tsx", ".ts", ".js"],
    },
    output: {
        path: path.resolve(__dirname, "dist"),
        filename: "bundle.js",
    }
};