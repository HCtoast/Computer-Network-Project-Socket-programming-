const dom = {
    user: document.querySelector('.mc-user-name'),
    serverHostName: document.querySelector('.mc-server-hostname'),
    serverPort: document.querySelector('.mc-server-port'),
    refreshBtn: document.querySelector('.mc-refresh-btn'),
    popup404: document.querySelector('.mc-404-popup'),

    mailListContainer: document.querySelector('.mc-mail-list'),

    mailDetailView: document.querySelector('.mc-mail-detail-view'),
    mailTitle: document.querySelector('.mail-detail-title'),
    mailSender: document.querySelector('.mail-detail-sender'),
    mailContent: document.querySelector('.mail-detail-content'),
};

const global = {
    mailList: [],
    userName: '',
    serverHostname: '',
    serverPort: 0,
}

const eventListeners = {
    "ipc-server-hearbeat-response": (data) => { },
    "ipc-get-mail-list-response": (data) => { },
    "ipc-get-mail-content-response": (data) => { },
    "ipc-send-mail-response": (data) => { },
}

document.addEventListener('DOMContentLoaded', (ev) => {
    window.api.addBroadcastListener((message) => {
        message = JSON.parse(message);
        eventListeners[message.type]?.(message.data);
    });

    dom.refreshBtn.addEventListener('click', SyncMailList);

    SyncMailList();
});

function SyncMailList() {
    window.api.Send(JSON.stringify({
        type: 'ipc-get-mail-list',
        data: {
            hostname: global.serverHostname,
            port: global.serverPort,
            user: 'test'
        }
    }));
}
