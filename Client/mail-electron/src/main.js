const { app, BrowserWindow, ipcMain } = require('electron');
const http = require('http');

const createWindow = () => {
    const win = new BrowserWindow({
        minWidth: 1280,
        minHeight: 720,
        fullscreen: true,
    });

    win.loadFile('./vite/index.html');
};

app.whenReady().then(() => {
    createWindow();
});

app.on('window-all-closed', () => {
    if (process.platform != "darwin") app.quit();
});

function Request(hostname, port = 80, path = '/', method = "GET", headers = {}, body = null) {
    if (!headers['User-Agent']) {
        headers['User-Agent'] = 'Mail-Client/0.1';
    }

    if (body !== null) {
        headers['Content-Type'] = 'application/json';
        headers['Content-Length'] = Buffer.byteLength(body);
    }

    const options = {
        hostname,
        port,
        path,
        method,
        headers
    };

    return new Promise((resolve, reject) => {
        const req = http.request(options, (res) => {
            let chunks = [];

            res.setEncoding('utf8');

            res.on('data', (chunk) => {
                chunks += chunk;
            });

            res.on('end', () => {
                resolve({
                    statusCode: res.statusCode,  // 예: 200, 404 등
                    headers: res.headers,        // 응답 헤더
                    body: chunks,                // 응답 바디 (string)
                });
            });
        });

        req.on('error', (err) => {
            reject(err);
        });

        if (body) {
            req.write(body);
        }

        req.end();
    });
}
