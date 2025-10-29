import '../assets/base.css'

const dom = {};

const eventListeners = {
    "ipc-server-heartbeat-response": (data) => {

    },
    "ipc-get-mail-list-response": (data) => {
        console.log(data);
    }
};

document.addEventListener("DOMContentLoaded", (ev) => {
    window.api.addBroadcastListener((message) => {
        message = JSON.parse(message);
        eventListeners[message.type]?.(message.data);
    });

    SyncMailList();
});

function SyncMailList() {
    window.api.Send(JSON.stringify({
        type: 'ipc-get-mail-list',
        data: {
            hostname: '127.0.0.1',
            port: 8080
        }
    }));
}

/** @param {string} id */
function GetMailContent(id) { }

function PostMail() { }

/**
 * @param {string} title
 * @param {string} content
 */
function Notify(title, content) { }

/** format json object into mail list item dom element
 * @param {object} json
*/
function MailListItem(json) { }

/** @param {object} json */
function ViewMailContent(json) { }

