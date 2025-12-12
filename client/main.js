const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const net = require('net');
const dgram = require('dgram');
const os = require('os');

let mainWindow = null;
let tcpClient = null;
let isConnected = false;

const UDP_DISCOVERY_PORT = 5555;
const DISCOVERY_TIMEOUT = 3000;

function createWindow() {
    mainWindow = new BrowserWindow({
        width: 1200,
        height: 800,
        minWidth: 800,
        minHeight: 600,
        webPreferences: {
            nodeIntegration: false,
            contextIsolation: true,
            preload: path.join(__dirname, 'preload.js')
        },
        titleBarStyle: 'hiddenInset',
        show: false,
        backgroundColor: '#1a1a2e'
    });

    mainWindow.loadFile('index.html');

    mainWindow.once('ready-to-show', () => {
        mainWindow.show();
    });

    mainWindow.on('closed', () => {
        mainWindow = null;
        tcpClient?.destroy();
    });
}

app.whenReady().then(createWindow);

app.on('window-all-closed', () => {
    if (process.platform !== 'darwin')
        app.quit();
});

app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0)
        createWindow();
});

ipcMain.handle('discover-servers', async () => {
    return new Promise((resolve) => {
        const servers = [];
        const socket = dgram.createSocket('udp4');

        socket.on('error', (err) => {
            console.error('UDP error:', err);
            socket.close();
            resolve(servers);
        });

        socket.on('message', (msg, rinfo) => {
            const response = msg.toString();
            if (response.startsWith("hello i'm a quiznet server:")) {
                const parts = response.split(':');
                if (parts.length >= 3) {
                    // Skip localhost responses
                    if (rinfo.address === '127.0.0.1')
                        return;

                    const server = {
                        ip: rinfo.address,
                        name: parts[1],
                        port: parseInt(parts[2])
                    };
                    servers.push(server);
                    mainWindow?.webContents.send('server-discovered', server);
                }
            }
        });

        socket.bind(() => {
            socket.setBroadcast(true);
            const message = Buffer.from('looking for quiznet servers');

            const interfaces = os.networkInterfaces();
            const broadcastAddresses = new Set(['255.255.255.255']);

            for (const name of Object.keys(interfaces))
                for (const iface of interfaces[name])
                    if (iface.family === 'IPv4' && !iface.internal) {
                        const ipParts = iface.address.split('.').map(Number);
                        const maskParts = iface.netmask.split('.').map(Number);
                        const broadcastParts = ipParts.map((ip, i) => ip | (~maskParts[i] & 255));
                        const broadcastAddr = broadcastParts.join('.');
                        broadcastAddresses.add(broadcastAddr);
                        console.log(`Interface ${name}: ${iface.address} -> broadcast ${broadcastAddr}`);
                    }

            for (const addr of broadcastAddresses) {
                console.log(`Sending discovery to ${addr}:${UDP_DISCOVERY_PORT}`);
                socket.send(message, 0, message.length, UDP_DISCOVERY_PORT, addr);
            }
        });

        setTimeout(() => {
            socket.close();
            resolve(servers);
        }, DISCOVERY_TIMEOUT);
    });
});

ipcMain.handle('connect-server', async (event, { ip, port }) => {
    return new Promise((resolve, reject) => {
        tcpClient?.destroy();

        tcpClient = new net.Socket();
        let buffer = '';

        tcpClient.connect(port, ip, () => {
            isConnected = true;
            console.log('Connected to server:', ip, port);
            resolve({ success: true });
        });

        tcpClient.on('data', (data) => {
            buffer += data.toString();
            let newlineIndex;
            while ((newlineIndex = buffer.indexOf('\n')) !== -1) {
                const message = buffer.substring(0, newlineIndex);
                buffer = buffer.substring(newlineIndex + 1);

                if (message.trim())
                    try {
                        const json = JSON.parse(message);
                        mainWindow?.webContents.send('server-message', json);
                    } catch (e) {
                        console.error('Failed to parse message:', message);
                    }
            }
        });

        tcpClient.on('error', (err) => {
            console.error('TCP error:', err);
            isConnected = false;
            mainWindow?.webContents.send('connection-error', err.message);
            reject(err);
        });

        tcpClient.on('close', () => {
            isConnected = false;
            mainWindow?.webContents.send('connection-closed');
        });
    });
});

ipcMain.handle('disconnect-server', () => {
    if (tcpClient) {
        tcpClient.destroy();
        tcpClient = null;
        isConnected = false;
    }
    return { success: true };
});

ipcMain.handle('send-request', (event, { method, endpoint, data }) => new Promise((resolve, reject) => {
    if (!tcpClient || !isConnected) {
        reject(new Error('Not connected to server'));
        return;
    }

    let message;
    if (method === 'POST') {
        const body = data ? JSON.stringify(data) : '{}';
        message = `${method} ${endpoint}\n${body}\n`;
    } else message = `${method} ${endpoint}\n`;

    tcpClient.write(message, (err) => {
        if (err) {
            reject(err);
        } else resolve({ success: true });
    });
}));

ipcMain.handle('is-connected', () => isConnected);
