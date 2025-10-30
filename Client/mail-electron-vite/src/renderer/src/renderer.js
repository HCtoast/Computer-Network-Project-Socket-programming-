import '../assets/base.css'

const dom = {
    user: document.querySelector('.mc-user-name'),
    serverHostName: document.querySelector('.mc-server-hostname'),
    serverPort: document.querySelector('.mc-server-port'),
    refreshBtn: document.querySelector('.mc-refresh-btn'),
    popup404: document.querySelector('.mc-404-popup'),

    mailListContainer: document.querySelector('.mc-mail-list'),

    // popup
    popupContainer: document.querySelector('.popup-container'),
    closePopupBtn: document.querySelector('.popup-close-btn'),

    mailDetailView: document.querySelector('.mc-mail-detail-view'),
    mailTitle: document.querySelector('.mc-mail-detail-title'),
    mailSender: document.querySelector('.mc-mail-detail-sender'),
    mailRecipient: document.querySelector('.mc-mail-detail-recipient'),
    mailDate: document.querySelector('.mc-mail-detail-date'),
    mailContent: document.querySelector('.mc-mail-detail-content'),
};

let mailList = [];

// API 응답 수신, JSON 파싱
const eventListeners = {
    "ipc-server-heartbeat-response": (data) => {
        if (data.statusCode === 200) {
            console.log("API 서버 연결 성공");
        } else {
            console.error("API 서버 연결 실패");
        }
    },
    "ipc-get-mail-list-response": (data) => {
        console.log(data);
        
        // 1. HTTP 상태 코드 확인 및 에러 처리
        if (data.statusCode !== 200) {
            Notify('메일 목록 로드 실패', `서버 응답 오류: ${data.statusCode} ${data.statusMessage}`);
            return;
        }

        // 2. JSON 문자열 파싱
        try {
            const apiResponse = JSON.parse(data.body); 

            // 3. 데이터 유효성 검사 및 전역 변수 업데이트
            if (apiResponse && Array.isArray(apiResponse.items)) {
                mailList = apiResponse.items;
                console.log('메일 목록 파싱 성공:', mailList);
                
                // 4. UI 업데이트 함수 호출
                UpdateMailListUI(mailList);
            } else {
                Notify('데이터 형식 오류', '서버에서 예상치 못한 형식의 데이터를 받았습니다.');
            }

        } catch (error) {
            console.error('JSON 파싱 오류:', error);
            Notify('JSON 파싱 오류', '받은 데이터가 올바른 JSON 형식이 아닙니다.');
        }
    },
    "ipc-get-mail-content-response": (data) => {
        if (data.statusCode !== 200) {
            Notify('메일 상세 로드 실패', `서버 응답 오류: ${data.statusCode} ${data.statusMessage}`);
            return;
        }

        try {
            // 1차 파싱
            const apiResponse = JSON.parse(data.body);
            
            if (apiResponse.raw) {
                let rawContent = apiResponse.raw;
                let mailContent = {};

                try {
                    // 2차 파싱
                    mailContent = JSON.parse(rawContent);
                } catch (innerError) {
                    console.error('2차 JSON 파싱 실패', innerError);

                    // 오류 복구
                    if (typeof rawContent === 'string' && rawContent.length > 0) {
                        let fixedContent = rawContent.replace(/\\"/g, '$$TMP$$');
                        fixedContent = fixedContent.replace(/"/g, '\\"');
                        fixedContent = fixedContent.replace(/\$\$TMP\$\$/g, '\\"');
                        
                        try {
                            mailContent = JSON.parse(fixedContent);
                            console.log('따옴표 이스케이프 후 파싱 성공');
                        } catch (finalError) {
                            console.error('따옴표 이스케이프 후에도 파싱 실패', finalError);
                        }
                    }

                    mailContent.title = '(JSON 파싱 오류 발생)';
                    mailContent.user = '(발신자 정보 알 수 없음)';
                    mailContent.body = `[원본 오류 내용]:\n서버에서 받은 메일 본문이 올바른 JSON 형식이 아닙니다.\n\n${rawContent}`;
                }

                ViewMailContent(mailContent);

            } else {
                Notify('상세 내용 오류', '메일 본문 정보가 누락되었습니다.');
            }

        } catch (error) {
            console.error('최종 JSON 파싱 오류:', error);
            Notify('JSON 파싱 오류', '받은 데이터가 올바른 JSON 형식이 아닙니다.');
        }
    }
}

document.addEventListener("DOMContentLoaded", (ev) => {
    window.api.addBroadcastListener((message) => {
        message = JSON.parse(message);
        eventListeners[message.type]?.(message.data);
    });

    dom.user.textContent = 'alice'; 
    dom.serverHostName.textContent = '127.0.0.1';
    dom.serverPort.textContent = '8080';

    dom.refreshBtn.addEventListener('click', SyncMailList);
    dom.closePopupBtn.addEventListener('click', HideMailContent);

    HideMailContent();

    SyncMailList();
});

function SyncMailList() {
    const hostname = dom.serverHostName.textContent;
    const port = dom.serverPort.textContent;
    const user = dom.user.textContent;

    window.api.Send(JSON.stringify({
        type: 'ipc-get-mail-list',
        data: {
            hostname: hostname,
            port: port,
            user: user
        }
    }));

    console.log(`메일 목록 동기화 요청: ${user}@${hostname}:${port}`);
}

function HideMailContent() {
    if (dom.popupContainer) {
        dom.popupContainer.style.display = 'none';
    }
}

function UpdateMailListUI(list) {
    if (!dom.mailListContainer) return;

    // 기존 목록 초기화 (화면을 비움)
    dom.mailListContainer.innerHTML = ''; 

    HideMailContent();

    if (!Array.isArray(list) || list.length === 0) {
        dom.mailListContainer.innerHTML = '<p>받은 메일이 없습니다.</p>';
        return;
    }

    // 2. 새로운 목록 항목 추가
    list.forEach(mail => {
        const mailItem = MailListItem(mail);
        dom.mailListContainer.appendChild(mailItem);
    });
    
    console.log(`총 ${list.length}개의 메일 목록이 화면에 업데이트되었습니다.`);
}


/** @param {int} mail_index */
function GetMailContent(mail_index) {
    const hostname = dom.serverHostName.textContent;
    const port = dom.serverPort.textContent;
    const user = dom.user.textContent;

    window.api.Send(JSON.stringify({
        type: 'ipc-get-mail-content',
        data: {
            hostname: hostname,
            port: port,
            user: user,
            mailIndex: mail_index.toString()
        }
    }));

    console.log(`메일 인덱스 ${mail_index} 상세 내용 요청`);
}


function PostMail() { }


/**
 * @param {string} title
 * @param {string} content
 */
function Notify(title, content) {
    console.error(`[알림] ${title}: ${content}`);
    alert(`${title}\n${content}`);
}


// 받은 메일 리스트
/** format json object into mail list item dom element
 * @param {object} json
*/
function MailListItem(json) {
    const item = document.createElement('li');
    item.classList.add('mc-mail-list-item');
    
    item.dataset.mail_index = json.mail_index;

    item.innerHTML = `
        <span class="mc-mail-sender">${json.user || '발신자 정보 없음'}</span>
        <span class="mc-mail-title">${json.title || '(제목 없음)'}</span>
    `;

    item.addEventListener('click', () => {
        GetMailContent(Number(item.dataset.mail_index));
    });

    return item;
}


// 메일 상세 내용
/** @param {object} json */
function ViewMailContent(json) {
    dom.mailTitle.textContent = json.title || '(제목 없음)';
    dom.mailSender.textContent = `보낸 이: ${json.user || '알 수 없음'}`;
    dom.mailRecipient.textContent = `받는 이: ${json.to || '알 수 없음'}`;

    const dateStr = json.date ? json.date.replace('T', ' ').replace('Z', '').substring(0, 16) : '(날짜 정보 없음)';
    dom.mailDate.textContent = `날짜: ${dateStr}`;

    const bodyContent = (json.body || '메일 본문이 없습니다.').replace(/\n/g, '<br>');
    dom.mailContent.innerHTML = bodyContent;

    if (dom.popupContainer) {
        dom.popupContainer.style.display = 'flex';
    }
    
    console.log(`메일 상세 내용 표시 완료: ${json.title} (팝업)`);
}
