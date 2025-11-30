const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('quiznet', {
    discoverServers: () => ipcRenderer.invoke('discover-servers'),
    onServerDiscovered: (callback) => ipcRenderer.on('server-discovered', (event, server) => callback(server)),

    connectServer: (ip, port) => ipcRenderer.invoke('connect-server', { ip, port }),
    disconnectServer: () => ipcRenderer.invoke('disconnect-server'),
    isConnected: () => ipcRenderer.invoke('is-connected'),

    sendRequest: (method, endpoint, data) => ipcRenderer.invoke('send-request', { method, endpoint, data }),

    onServerMessage: (callback) => ipcRenderer.on('server-message', (event, data) => callback(data)),
    onConnectionError: (callback) => ipcRenderer.on('connection-error', (event, error) => callback(error)),
    onConnectionClosed: (callback) => ipcRenderer.on('connection-closed', () => callback()),

    removeAllListeners: (channel) => ipcRenderer.removeAllListeners(channel)
});
