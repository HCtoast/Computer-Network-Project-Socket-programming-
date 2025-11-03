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


