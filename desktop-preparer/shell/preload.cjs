'use strict';
const {contextBridge, ipcRenderer} = require('electron');
contextBridge.exposeInMainWorld('preparer', Object.freeze({
  platform: process.platform,
  chooseCard: () => ipcRenderer.invoke('choose-card'),
  inspect: () => ipcRenderer.invoke('inspect'),
  prepare: configuration => ipcRenderer.invoke('prepare', configuration),
  cancel: () => ipcRenderer.invoke('cancel'),
  diagnostics: () => ipcRenderer.invoke('diagnostics'),
  onEvent: callback => {
    const listener = (_, event) => callback(event);
    ipcRenderer.on('job-event', listener);
    return () => ipcRenderer.removeListener('job-event', listener);
  }
}));
