import { app, shell, BrowserWindow, ipcMain, webContents } from 'electron'
import { join } from 'path'
import { electronApp, optimizer, is } from '@electron-toolkit/utils'
import icon from '../../resources/icon.png?asset'
import http from 'http'

function createWindow() {
    // Create the browser window.
    const mainWindow = new BrowserWindow({
        width: 1280,
        height: 720,
        show: false,
        autoHideMenuBar: true,
        ...(process.platform === 'linux' ? { icon } : {}),
        webPreferences: {
            preload: join(__dirname, '../preload/index.js'),
            sandbox: false,
            contextIsolation: true,
            unsafeEval: false,
        }
    })

    mainWindow.on('ready-to-show', () => {
        mainWindow.show()
    })

    mainWindow.webContents.setWindowOpenHandler((details) => {
        shell.openExternal(details.url)
        return { action: 'deny' }
    })

    // HMR for renderer base on electron-vite cli.
    // Load the remote URL for development or the local html file for production.
    if (is.dev && process.env['ELECTRON_RENDERER_URL']) {
        mainWindow.loadURL(process.env['ELECTRON_RENDERER_URL'])
    } else {
        mainWindow.loadFile(join(__dirname, '../renderer/index.html'))
    }

    return mainWindow;
}

// This method will be called when Electron has finished
// initialization and is ready to create browser windows.
// Some APIs can only be used after this event occurs.
app.whenReady().then(() => {
    // Set app user model id for windows
    electronApp.setAppUserModelId('com.electron')

    // Default open or close DevTools by F12 in development
    // and ignore CommandOrControl + R in production.
    // see https://github.com/alex8088/electron-toolkit/tree/master/packages/utils
    app.on('browser-window-created', (_, window) => {
        optimizer.watchWindowShortcuts(window)
    })

    let window = createWindow();

    // IPC test
    ipcMain.on('broadcast', (event, message) => {
        if (message == null || !message) {
            console.log('empty message received');
            console.log(event);
            return;
        }
        message = JSON.parse(message);
        broadcastListeners[message.type]?.(message.data, window.webContents);
    });

    app.on('activate', function() {
        // On macOS it's common to re-create a window in the app when the
        // dock icon is clicked and there are no other windows open.
        if (BrowserWindow.getAllWindows().length === 0) createWindow()
    })
})

app.on('window-all-closed', () => {
    if (process.platform !== 'darwin') {
        app.quit()
    }
})

const broadcastListeners = {
    "ipc-server-heartbeat": async ({ hostname, port }, webContents) => {
        const response = await Request(hostname, port, '/api/heartbeat', 'GET');

        webContents.send('broadcast', JSON.stringify({
            'type': 'ipc-server-heartbeat-response',
            'data': {
                statusCode: response.statusCode,
                statusMessage: response.statusMessage,
                body: response.body
            }
        }));
    },
    "ipc-get-mail-list": async ({ hostname, port, user }, webContents) => {
        const response = await Request(hostname, port, '/api/list', 'GET', { 'X-Expect-Server': 'MAILAPI', 'Host': '', 'X-User': user });

        webContents.send('broadcast', JSON.stringify({
            'type': 'ipc-get-mail-list-response',
            'data': {
                statusCode: response.statusCode,
                statusMessage: response.statusMessage,
                body: response.body
            }
        }));
    },
    "ipc-send-mail": async ({ hostname, port, mail }, webContents) => {
        const response = await Request(hostname, port, '/api/send/', 'POST', { 'Accept': 'application:json' }, mail); // mail includes author, receiver, title, content

        ipcMain.emit('broadcast', JSON.stringify({
            'ipc-send-mail-response': {
                statusCode: response.statusCode,
                statusMessage: response.statusMessage,
                body: response.body
            }
        }));
    }
};

function Request(hostname, port = 8080, path = '/', method = "GET", headers = {}, body = null) {
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
                    statusCode: res.statusCode,
                    headers: res.headers,
                    body: chunks,
                });
            });
        })

        req.on('error', (err) => {
            reject(err);
        });

        if (body) {
            req.write(body);
        }

        req.end();
    }).catch(err => console.error(err));
}
