'use strict';
const {contextBridge, ipcRenderer} = require('electron');
contextBridge.exposeInMainWorld('preparer', Object.freeze({
  platform: process.platform,
  chooseCard: () => ipcRenderer.invoke('choose-card'),
  inspect: () => ipcRenderer.invoke('inspect'),
  plan: configuration => ipcRenderer.invoke('plan', configuration),
  prepare: configuration => ipcRenderer.invoke('prepare', configuration),
  cancel: () => ipcRenderer.invoke('cancel'),
  diagnostics: () => ipcRenderer.invoke('diagnostics'),
  cardSpace: () => ipcRenderer.invoke('card-space'),
  previewCache: filter => ipcRenderer.invoke('cache-preview', filter),
  deleteCache: id => ipcRenderer.invoke('cache-delete', id),
  cancelCache: () => ipcRenderer.invoke('cache-cancel'),
  discardCache: () => ipcRenderer.invoke('cache-discard'),
  onCacheEvent: callback => {
    const listener = (_, event) => callback(event);
    ipcRenderer.on('cache-event', listener);
    return () => ipcRenderer.removeListener('cache-event', listener);
  },
  onEvent: callback => {
    const listener = (_, event) => callback(event);
    ipcRenderer.on('job-event', listener);
    return () => ipcRenderer.removeListener('job-event', listener);
  }
}));
