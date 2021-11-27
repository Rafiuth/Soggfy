const path = require('path');
const devMode = process.env.NODE_ENV !== "production";

module.exports = {
    entry: './src/main.ts',
    devtool: 'source-map',
    mode: devMode ? 'development' : 'production',
    module: {
        rules: [
            {
                test: /\.tsx?$/,
                use: 'ts-loader',
                exclude: /node_modules/,
            },
            {
                test: /\.css$/i,
                loader: 'css-loader',
                options: {
                    sourceMap: false,
                    exportType: 'string',
                    modules: false
                }
            }
        ]
    },
    optimization: {
        chunkIds: 'named',
        moduleIds: 'named',
        minimize: !devMode,
        mangleExports: false
    },
    experiments: {
        topLevelAwait: true
    },
    resolve: {
        extensions: [ '.tsx', '.ts', '.js' ],
    },
    output: {
        path: path.resolve(__dirname, 'dist'),
        filename: 'bundle.js',
    }
};