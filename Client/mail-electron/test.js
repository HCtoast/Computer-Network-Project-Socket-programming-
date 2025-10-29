const http = require('http');

function Request(hostname, port = 80, path = '/', method = "GET", headers = {}, body = null) {
    if (!headers['User-Agent']) {
        headers['User-Agent'] = 'Mail-Client/0.1';
    }

    if (body !== null) {
        headers['Content-Type'] = 'application/json';
        headers['Content-Length'] = Buffer.byteLength(body);
    }

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
    // let result = await Request('localhost', 3000, '/', 'GET');
    // console.log(result.body);
    //
    // let result2 = await Request('localhost', 3000, '/', 'POST', { 'ACCEPT': 'application/json' }, JSON.stringify({ test: 'data' }));
    // console.log(JSON.parse(result2.body));

    let result3 = await Request('127.0.0.1', 8080, '/api/inbox', 'GET');
    console.log(JSON.parse(result3.body));
}

test();
