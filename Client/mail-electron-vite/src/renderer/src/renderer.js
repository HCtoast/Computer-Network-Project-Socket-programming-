import '../assets/base.css'

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
            // data.body에는 API 서버가 보낸 JSON 문자열이 담겨 있음 ({"total":N,"items":[]})
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
            // 1차 파싱: API 응답 JSON ({ok:true, id:"...", raw:"{...메일 본문 JSON...}"})
            const apiResponse = JSON.parse(data.body);
            
            if (apiResponse.raw) {
                 // 2차 파싱: raw 필드 안에 있는 메일 본문 JSON 문자열 파싱
                const mailContent = JSON.parse(apiResponse.raw);
                ViewMailContent(mailContent); 
            } else {
                Notify('상세 내용 오류', '메일 본문 정보가 누락되었습니다.');
            }
           
        } catch (error) {
            console.error('JSON 파싱 오류:', error);
            Notify('JSON 파싱 오류', '받은 데이터가 올바른 JSON 형식이 아닙니다.');
        }
    }
};

document.addEventListener("DOMContentLoaded", (ev) => {
    window.api.addBroadcastListener((message) => {
        message = JSON.parse(message);
        eventListeners[message.type]?.(message.data);
    });

    // ["mc-user-name", "mc-server-hostname", "mc-server-port", "mc-refresh-btn", "mc-404-popup", "mc"]

    dom.refreshBtn.addEventListener('click', SyncMailList);

    SyncMailList();
});

function SyncMailList() {
    window.api.Send(JSON.stringify({
        type: 'ipc-get-mail-list',
        data: {
            hostname: '127.0.0.1',
            port: 8080,
            user: 'test'
        }
    }));

    // UI 업데이트
    dom.serverHostName.textContent = '127.0.0.1';
    dom.serverPort.textContent = '8080';
    dom.user.textContent = 'test';
}

function UpdateMailListUI(list) {
    if (!dom.mailListContainer) return;

    // 기존 목록 초기화 (화면을 비움)
    dom.mailListContainer.innerHTML = ''; 

    // 상세 보기 영역 숨기기
    if (dom.mailDetailView) {
         dom.mailDetailView.style.display = 'none';
    }

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

// 메일 상세 내용 요청
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
            mailIndex: mail_index
        }
    }));

    console.log(`메일 인덱스 ${mail_index} 상세 내용 요청`);
}

// 메일 발송
function PostMail() { }

// 메일 제목, 내용 출력
/**
 * @param {string} title
 * @param {string} content
 */
function Notify(title, content) {
    console.error(`[알림] ${title}: ${content}`);
    alert(`${title}\n${content}`);
 }

// 메일 리스트
/** format json object into mail list item dom element
 * @param {object} json
*/
function MailListItem(json) {
    const item = document.createElement('li');
    item.classList.add('mc-mail-list-item');
    
    item.dataset.mail_index = json.mail_index;

    item.innerHTML = `
        <span class="mail-sender">${json.user || '발신자 정보 없음'}</span>
        <span class="mail-title">${json.title || '(제목 없음)'}</span>
    `;

    item.addEventListener('click', () => {
        GetMailContent(Number(item.dataset.mail_index));
    });

    return item;
 }

// 메일 내용 표시
/** @param {object} json */
function ViewMailContent(json) {
    dom.mailTitle.textContent = json.title || '(제목 없음)';
    dom.mailSender.textContent = `보낸 사람: ${json.user || '알 수 없음'}`;

    dom.mailContent.innerHTML = (json.body || '메일 본문이 없습니다.').replace(/\n/g, '<br>') || '메일 본문이 없습니다.';

    dom.mailDetailView.style.display = 'block';

    console.log(`메일 상세 내용 표시 완료: ${json.title}`);
}

