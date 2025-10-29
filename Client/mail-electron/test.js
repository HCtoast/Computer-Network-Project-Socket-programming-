const http = require('http');

function Request(hostname, port = 80, path = '/', method = "GET", headers = {}, body = null) {
    const options = {
        hostname,
        port,
        path,
        method,
        headers
    };

    return new Promise((resolve, reject) => {
        const req = http.request(options, (res) => {
            let chunks = [];

            res.setEncoding('utf8');

            res.on('data', (chunk) => {
                chunks += chunk;
            });

            res.on('end', () => {
                resolve({
                    statusCode: res.statusCode,  // 예: 200, 404 등
                    headers: res.headers,        // 응답 헤더
                    body: chunks,                // 응답 바디 (string)
                });
            });
        });

        req.on('error', (err) => {
            reject(err);
        });

        if (body) {
            req.write(body);
        }

        req.end();
    });
}

async function test() {
    let result = await Request('localhost', 3000, '/', 'GET', { 'ACCEPT': 'application/json' });
    console.log(result);

    let result2 = await Request('localhost', 3000, '/', 'POST', { 'ACCEPT': 'application/json' }, JSON.stringify({ test: 'data' }));
    console.log(result2);
}

test();
