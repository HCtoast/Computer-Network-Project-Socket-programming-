const { app, BrowserWindow } = require('electron');

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
